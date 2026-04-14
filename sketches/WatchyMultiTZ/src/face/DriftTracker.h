#pragma once
// DriftTracker — cleanroom port of Sensor-Watch's Nanosec + Finetune
// algorithms for a PCF8563-class uncompensated RTC.
//
// Upstream (MIT): github.com/joeycastillo/Sensor-Watch —
//   movement/watch_faces/settings/nanosec_face.{h,c}
//   movement/watch_faces/settings/finetune_face.{h,c}
// Author: Mikhail Svarichevsky (dev@3.14.by). See DriftTracker.cpp for the
// full upstream MIT notice.
//
// Terminology:
//   FC ("freq_correction")   — per-unit static DC frequency offset in ppm.
//                              Learned from bracketed sync measurements.
//   tempco                   — quadratic / cubic correction around the
//                              tuning-fork turnover temperature.
//   EMA                      — exponential moving average, weight 1/8 once
//                              the initial "fold" phase is complete.
//
// Usage from the sketch:
//   DriftTracker dt;
//   dt.init(&storage, &thermo);
//
//   // in WatchyNetwork after a successful NTP sync:
//   dt.onSample(preUtc, postUtc, gmtOffset);
//
//   // in WatchyClock::nowUtc():
//   int64_t raw = rawRtcUtc();
//   return raw - (int64_t)lround(
//       dt.correctionSeconds(raw - dt.lastSyncUtc(), thermo.readCelsius()));

#include "../hal/Types.h"
#include <stdint.h>

namespace wmt {

class IPersistentStorage;
class IThermometer;

class DriftTracker {
public:
    // Circular-buffer capacity of the sync history (for the stats graph).
    static constexpr int RING_SIZE = 24;

    // Samples below this count use straight averaging (`fold`); above,
    // the EMA kicks in. Matches Finetune's convergence behaviour.
    static constexpr int N_FOLD = 8;

    // Defaults per Nanosec header.
    static constexpr int16_t DEFAULT_T0_X100        = 2500;  // 25.00 °C turnover
    static constexpr int16_t DEFAULT_QUAD_TEMPCO    = 3400;  // 0.034 ppm/°C²
    static constexpr int16_t DEFAULT_CUBIC_TEMPCO   = 0;     // set 1360 to enable
    static constexpr int16_t DEFAULT_AGING_X100     = 0;     // ppm/yr × 100
    static constexpr int8_t  DEFAULT_CADENCE_MIN    = 10;
    static constexpr int8_t  DEFAULT_PROFILE        = 2;     // quadratic

    // One entry in the rolling history displayed by DriftStatsScreen's
    // graph page. 4 bytes. Stored in NVS as a flat 96-byte blob.
    struct Sample {
        int16_t inst_ppm_x100;  // instantaneous ppm at this sync × 100
        int16_t ema_ppm_x100;   // running EMA at time of this sample × 100
    };

    DriftTracker() = default;

    // Load persistent state. Pass nullptr for storage if NVS is unavailable
    // (e.g. sim); the tracker then runs in volatile mode.
    // Pass nullptr for thermo to disable tempco (profile 1 behaviour).
    void init(IPersistentStorage *storage, IThermometer *thermo);

    // Record one bracketed sync measurement.
    //   preUtc     = what our clock said immediately BEFORE the library's
    //                syncNTP() call  (i.e. rtc-time − gmtOffset)
    //   postUtc    = what it says immediately AFTER syncNTP()  (= NTP truth)
    //   gmtOffset  = library's gmtOffset at the time of this sync
    //                (used as a DST/TZ-shift guard — sample is rejected if
    //                 the offset has changed since last sync)
    // On first call (when lastSyncUtc_ == 0) this just records the baseline;
    // no FC update yet. Subsequent calls compute inst_ppm and fold it in.
    void onSample(int64_t preUtc, int64_t postUtc, int32_t gmtOffset);

    // Total effective ppm for the current temperature, applied as:
    //   drift_s = elapsed_s * totalPpmNow(tempC) * 1e-6
    //   corrected_utc = rtc_utc - drift_s
    // Returns 0 if no FC has been learned yet (profile 0 == "off").
    float totalPpmNow(int8_t tempC) const;

    // Convenience: correction in seconds for an elapsed duration at the
    // given temperature. Result is SIGNED — positive means the RTC has
    // overshot, so the caller subtracts the return value from raw RTC UTC.
    // Sub-second precision (returns double).
    double correctionSeconds(int64_t elapsedSec, int8_t tempC) const;

    // --------------- Accessors (read-only, for the stats screen) ----------

    int16_t  fcPpmX100()         const { return state_.fc_x100; }
    uint8_t  sampleCount()       const { return state_.samples; }
    int64_t  lastSyncUtc()       const { return state_.last_sync_utc; }
    int32_t  gmtOffsetAtSync()   const { return state_.gmt_off; }
    int8_t   profile()           const { return state_.profile; }
    int16_t  centerTempX100()    const { return state_.t0_x100; }
    int16_t  quadTempcoX100000() const { return state_.q_x1e5; }
    int16_t  cubicTempcoX10M()   const { return state_.c_x1e7; }
    int16_t  agingX100()         const { return state_.age_x100; }
    int8_t   cadenceMin()        const { return state_.cadence_m; }

    // History: copies up to `maxOut` entries into `out`, oldest first,
    // and returns the number actually copied (≤ RING_SIZE, ≤ maxOut).
    int history(Sample *out, int maxOut) const;

    // --------------- Test / sim hooks -------------------------------------

    // For the sim + unit tests: inject a synthetic state so the stats
    // screen can render realistic-looking data without needing real syncs.
    struct SyntheticState {
        int16_t fc_x100;
        uint8_t samples;
        int64_t last_sync_utc;
        int32_t gmt_off;
        int8_t  profile;
        int16_t t0_x100;
        int16_t q_x1e5;
        int16_t c_x1e7;
        int16_t age_x100;
        int8_t  cadence_m;
        const Sample *history;   // oldest first
        int          historyN;   // number of valid Sample entries
    };
    void setSyntheticState(const SyntheticState &s);

    // Zero-out all state (FC, samples, history). Used by sim --reset.
    void clearAll();

private:
    // On-device persistent struct. Kept plain-old-data so we can dump it
    // to NVS as a single blob per field. Field names mirror the NVS keys.
    struct State {
        int8_t   profile        = DEFAULT_PROFILE;
        int16_t  fc_x100        = 0;                      // learned per-unit
        int16_t  t0_x100        = DEFAULT_T0_X100;
        int16_t  q_x1e5         = DEFAULT_QUAD_TEMPCO;
        int16_t  c_x1e7         = DEFAULT_CUBIC_TEMPCO;
        int16_t  age_x100       = DEFAULT_AGING_X100;
        int8_t   cadence_m      = DEFAULT_CADENCE_MIN;
        uint8_t  samples        = 0;                      // caps at 255
        int64_t  last_sync_utc  = 0;                      // 0 ⇒ no baseline
        int32_t  gmt_off        = 0;                      // at last sync
    };

    State                 state_;
    IPersistentStorage   *storage_ = nullptr;
    IThermometer         *thermo_  = nullptr;

    // Ring buffer for the graph page. `head_` points at the SLOT to write
    // next (i.e. `ring_[head_]` is the oldest once filled).
    Sample  ring_[RING_SIZE]{};
    uint8_t head_       = 0;       // next write slot (0..RING_SIZE-1)
    uint8_t ringFilled_ = 0;       // count of valid entries (caps RING_SIZE)

    // --- persistence helpers ---
    static constexpr const char *NS = "wmt_drift";
    void loadFromStorage();
    void saveScalars();            // one blob per field
    void saveRing();                // one blob for history + head
    bool storageOk() const { return storage_ != nullptr; }

    // Append one sample to the ring buffer, updating head_/ringFilled_.
    void pushSample(int16_t inst_ppm_x100, int16_t ema_ppm_x100);
};

} // namespace wmt
