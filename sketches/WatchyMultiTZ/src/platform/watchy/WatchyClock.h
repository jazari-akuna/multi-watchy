#pragma once
// Watchy-specific IClock. Reads/writes the hardware RTC via WatchyRTC and
// converts between local-time (as the Watchy lib stores it) and UTC.
//
// nowUtc() applies a DriftTracker correction (when the tracker has learned
// an FC baseline); rawRtcUtc() exposes the uncorrected RTC reading for
// the WatchyNetwork bracket-measurement path.

#include "../../hal/IClock.h"
#include "../../hal/IThermometer.h"

class WatchyRTC; // forward-decl from Watchy lib

namespace wmt {

class DriftTracker; // forward-decl, defined in face/DriftTracker.h

// IClock impl that treats the Watchy RTC as "local-time at gmtOffset" and
// exposes everything to the face as UTC epochs.
class WatchyClock final : public IClock {
public:
    WatchyClock(WatchyRTC &rtc,
                DriftTracker *drift = nullptr,
                IThermometer *thermo = nullptr)
        : rtc_(rtc), drift_(drift), thermo_(thermo) {}

    int64_t nowUtc() override;                  // drift-corrected
    int64_t rawRtcUtc();                        // no correction
    void    setUtc(int64_t epoch) override;
    LocalDateTime toLocal(int64_t utc, const char *posixTZ) override;

private:
    WatchyRTC    &rtc_;
    DriftTracker *drift_;
    IThermometer *thermo_;
};

} // namespace wmt
