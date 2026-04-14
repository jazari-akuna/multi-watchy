// DriftTracker — cleanroom port of algorithm from Sensor-Watch's
// nanosec_face.c and finetune_face.c by Mikhail Svarichevsky, MIT-licensed.
//
// ----------------------------------------------------------------------------
// Upstream MIT notice (Sensor-Watch — github.com/joeycastillo/Sensor-Watch):
//
// MIT License
//
// Copyright (c) 2020-present Joey Castillo and Sensor Watch contributors
// (Nanosec / Finetune algorithm: Copyright (c) Mikhail Svarichevsky)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
// ----------------------------------------------------------------------------

#include "DriftTracker.h"

#include "../hal/IPersistentStorage.h"
#include "../hal/IThermometer.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

namespace wmt {

namespace {

// NVS key names — kept short to fit the 15-char Preferences key limit.
constexpr const char *K_PROFILE   = "profile";
constexpr const char *K_FC        = "fc_x100";
constexpr const char *K_T0        = "t0_x100";
constexpr const char *K_Q         = "q_x1e5";
constexpr const char *K_C         = "c_x1e7";
constexpr const char *K_AGE       = "age_x100";
constexpr const char *K_CADENCE   = "cadence_m";
constexpr const char *K_SAMPLES   = "samples";
constexpr const char *K_LAST_UTC  = "last_sync_utc";
constexpr const char *K_GMT       = "gmt_off";
constexpr const char *K_RING_BUF  = "samples_buf";
constexpr const char *K_RING_HEAD = "samples_head";
constexpr const char *K_RING_FILL = "samples_fill";

constexpr int64_t MIN_ELAPSED_S = 600;     // <10 min: quantisation noise dominates

} // namespace

void DriftTracker::init(IPersistentStorage *storage, IThermometer *thermo) {
    storage_ = storage;
    thermo_  = thermo;
    if (storage_ != nullptr && storage_->begin(NS)) {
        loadFromStorage();
    }
}

void DriftTracker::onSample(int64_t preUtc, int64_t postUtc, int32_t gmtOffset) {
    const int64_t drift_s   = preUtc - postUtc;            // +ve ⇒ RTC was fast
    const int64_t elapsed_s = postUtc - state_.last_sync_utc;

    // First sample ever: just record the baseline, no FC update.
    if (state_.last_sync_utc == 0) {
        state_.last_sync_utc = postUtc;
        state_.gmt_off       = gmtOffset;
        saveScalars();
        return;
    }

    // Quantisation-dominated: skip but keep the existing baseline so a longer
    // span still gets measured cleanly at the next sync.
    if (elapsed_s < MIN_ELAPSED_S) return;

    // Rebaseline on DST/TZ shift so next sample has a clean elapsed.
    if (gmtOffset != state_.gmt_off) {
        state_.last_sync_utc = postUtc;
        state_.gmt_off       = gmtOffset;
        saveScalars();
        return;
    }

    // Outlier guard: anything past ~200 ppm is a manual set or a bad bracket.
    if (labs((long)drift_s) > 5 + (long)(elapsed_s / 5000)) return;

    const double inst_ppm = (double)drift_s * 1e6 / (double)elapsed_s;

    const float oldFc = state_.fc_x100 / 100.0f;
    float newFc;
    if (state_.samples < N_FOLD) {
        newFc = (oldFc * (float)state_.samples + (float)inst_ppm) /
                (float)(state_.samples + 1);
    } else {
        newFc = oldFc * (7.0f / 8.0f) + (float)inst_ppm * (1.0f / 8.0f);
    }
    state_.fc_x100 = (int16_t)lroundf(newFc * 100.0f);
    if (state_.samples < 255) state_.samples++;
    state_.last_sync_utc = postUtc;
    state_.gmt_off       = gmtOffset;

    pushSample((int16_t)lroundf((float)inst_ppm * 100.0f), state_.fc_x100);

    saveScalars();
    saveRing();
}

float DriftTracker::totalPpmNow(int8_t tempC) const {
    if (state_.profile == 0) return 0.0f;
    const float dt    = ((float)tempC * 100.0f - (float)state_.t0_x100) / 100.0f;
    const float quad  = -((float)state_.q_x1e5 / 1e5f) * dt * dt;
    const float cubic =  ((float)state_.c_x1e7 / 1e7f) * dt * dt * dt;
    const float aging = 0.0f;  // age field is read/written but not yet applied
    return (float)state_.fc_x100 / 100.0f + quad + cubic + aging;
}

double DriftTracker::correctionSeconds(int64_t elapsedSec, int8_t tempC) const {
    if (elapsedSec <= 0 || state_.last_sync_utc == 0) return 0.0;
    return (double)elapsedSec * (double)totalPpmNow(tempC) * 1e-6;
}

int DriftTracker::history(Sample *out, int maxOut) const {
    if (out == nullptr || maxOut <= 0) return 0;
    const int n = (ringFilled_ < (uint8_t)RING_SIZE) ? (int)ringFilled_ : RING_SIZE;
    const int copy = (n < maxOut) ? n : maxOut;
    if (ringFilled_ < (uint8_t)RING_SIZE) {
        // Partial ring: arrival order is simply 0..ringFilled_-1.
        for (int i = 0; i < copy; ++i) out[i] = ring_[i];
    } else {
        // Full ring: oldest sits at head_, then wraps.
        for (int i = 0; i < copy; ++i) {
            out[i] = ring_[(head_ + i) % RING_SIZE];
        }
    }
    return copy;
}

void DriftTracker::setSyntheticState(const SyntheticState &s) {
    state_.fc_x100        = s.fc_x100;
    state_.samples        = s.samples;
    state_.last_sync_utc  = s.last_sync_utc;
    state_.gmt_off        = s.gmt_off;
    state_.profile        = s.profile;
    state_.t0_x100        = s.t0_x100;
    state_.q_x1e5         = s.q_x1e5;
    state_.c_x1e7         = s.c_x1e7;
    state_.age_x100       = s.age_x100;
    state_.cadence_m      = s.cadence_m;

    memset(ring_, 0, sizeof(ring_));
    head_       = 0;
    ringFilled_ = 0;
    if (s.history != nullptr && s.historyN > 0) {
        const int n = (s.historyN < RING_SIZE) ? s.historyN : RING_SIZE;
        memcpy(ring_, s.history, (size_t)n * sizeof(Sample));
        head_       = (uint8_t)(n % RING_SIZE);
        ringFilled_ = (uint8_t)n;
    }
    // Sim-only: deliberately no persistence.
}

void DriftTracker::clearAll() {
    state_ = State{};
    memset(ring_, 0, sizeof(ring_));
    head_       = 0;
    ringFilled_ = 0;
    if (storageOk()) {
        storage_->remove(K_PROFILE);
        storage_->remove(K_FC);
        storage_->remove(K_T0);
        storage_->remove(K_Q);
        storage_->remove(K_C);
        storage_->remove(K_AGE);
        storage_->remove(K_CADENCE);
        storage_->remove(K_SAMPLES);
        storage_->remove(K_LAST_UTC);
        storage_->remove(K_GMT);
        storage_->remove(K_RING_BUF);
        storage_->remove(K_RING_HEAD);
        storage_->remove(K_RING_FILL);
    }
}

void DriftTracker::pushSample(int16_t inst_ppm_x100, int16_t ema_ppm_x100) {
    ring_[head_].inst_ppm_x100 = inst_ppm_x100;
    ring_[head_].ema_ppm_x100  = ema_ppm_x100;
    head_ = (uint8_t)((head_ + 1) % RING_SIZE);
    if (ringFilled_ < (uint8_t)RING_SIZE) ringFilled_++;
}

void DriftTracker::saveScalars() {
    if (!storageOk()) return;
    storage_->putBytes(K_PROFILE,  &state_.profile,       sizeof(state_.profile));
    storage_->putBytes(K_FC,       &state_.fc_x100,       sizeof(state_.fc_x100));
    storage_->putBytes(K_T0,       &state_.t0_x100,       sizeof(state_.t0_x100));
    storage_->putBytes(K_Q,        &state_.q_x1e5,        sizeof(state_.q_x1e5));
    storage_->putBytes(K_C,        &state_.c_x1e7,        sizeof(state_.c_x1e7));
    storage_->putBytes(K_AGE,      &state_.age_x100,      sizeof(state_.age_x100));
    storage_->putBytes(K_CADENCE,  &state_.cadence_m,     sizeof(state_.cadence_m));
    storage_->putBytes(K_SAMPLES,  &state_.samples,       sizeof(state_.samples));
    storage_->putBytes(K_LAST_UTC, &state_.last_sync_utc, sizeof(state_.last_sync_utc));
    storage_->putBytes(K_GMT,      &state_.gmt_off,       sizeof(state_.gmt_off));
}

void DriftTracker::saveRing() {
    if (!storageOk()) return;
    storage_->putBytes(K_RING_BUF,  ring_,        sizeof(ring_));
    storage_->putBytes(K_RING_HEAD, &head_,       sizeof(head_));
    storage_->putBytes(K_RING_FILL, &ringFilled_, sizeof(ringFilled_));
}

void DriftTracker::loadFromStorage() {
    if (!storageOk()) return;

    int8_t   v_profile;
    int16_t  v_fc, v_t0, v_q, v_c, v_age;
    int8_t   v_cadence;
    uint8_t  v_samples;
    int64_t  v_last;
    int32_t  v_gmt;

    if (storage_->getBytes(K_PROFILE,  &v_profile,  sizeof(v_profile))  == sizeof(v_profile))  state_.profile       = v_profile;
    if (storage_->getBytes(K_FC,       &v_fc,       sizeof(v_fc))       == sizeof(v_fc))       state_.fc_x100       = v_fc;
    if (storage_->getBytes(K_T0,       &v_t0,       sizeof(v_t0))       == sizeof(v_t0))       state_.t0_x100       = v_t0;
    if (storage_->getBytes(K_Q,        &v_q,        sizeof(v_q))        == sizeof(v_q))        state_.q_x1e5        = v_q;
    if (storage_->getBytes(K_C,        &v_c,        sizeof(v_c))        == sizeof(v_c))        state_.c_x1e7        = v_c;
    if (storage_->getBytes(K_AGE,      &v_age,      sizeof(v_age))      == sizeof(v_age))      state_.age_x100      = v_age;
    if (storage_->getBytes(K_CADENCE,  &v_cadence,  sizeof(v_cadence))  == sizeof(v_cadence))  state_.cadence_m     = v_cadence;
    if (storage_->getBytes(K_SAMPLES,  &v_samples,  sizeof(v_samples))  == sizeof(v_samples))  state_.samples       = v_samples;
    if (storage_->getBytes(K_LAST_UTC, &v_last,     sizeof(v_last))     == sizeof(v_last))     state_.last_sync_utc = v_last;
    if (storage_->getBytes(K_GMT,      &v_gmt,      sizeof(v_gmt))      == sizeof(v_gmt))      state_.gmt_off       = v_gmt;

    // Ring buffer: zero first so a short read leaves the tail clean.
    memset(ring_, 0, sizeof(ring_));
    const size_t got = storage_->getBytes(K_RING_BUF, ring_, sizeof(ring_));
    if (got > 0 && got < sizeof(ring_)) {
        memset((uint8_t *)ring_ + got, 0, sizeof(ring_) - got);
    }

    uint8_t v_head = 0, v_fill = 0;
    if (storage_->getBytes(K_RING_HEAD, &v_head, sizeof(v_head)) == sizeof(v_head)) {
        head_ = (uint8_t)(v_head % RING_SIZE);
    }
    if (storage_->getBytes(K_RING_FILL, &v_fill, sizeof(v_fill)) == sizeof(v_fill)) {
        ringFilled_ = (v_fill > (uint8_t)RING_SIZE) ? (uint8_t)RING_SIZE : v_fill;
    } else {
        // No fill key persisted (legacy/partial state): infer from non-zero
        // entries. A genuine zero-sample is indistinguishable from empty here,
        // but that's acceptable for first-boot recovery.
        uint8_t fill = 0;
        for (int i = 0; i < RING_SIZE; ++i) {
            if (ring_[i].inst_ppm_x100 != 0 || ring_[i].ema_ppm_x100 != 0) fill++;
        }
        ringFilled_ = fill;
    }
}

} // namespace wmt
