#pragma once
// Time / timezone abstraction.
//
// Rationale: platform libraries disagree on how the hardware RTC is read.
// Watchy stores local-time-for-last-synced-gmtOffset; an InfiniTime PineTime
// stores UTC natively; a desktop sim just returns a fake epoch. This HAL
// hides those differences by always exposing UTC epochs and doing the POSIX
// TZ → local conversion on demand.
//
// Platforms are expected to restore `TZ=UTC0` after `toLocal` so subsequent
// library code that may read TZ behaves predictably.

#include "Types.h"
#include <stdint.h>

namespace wmt {

class IClock {
public:
    virtual ~IClock() = default;

    // Current time as Unix epoch seconds (UTC).
    virtual int64_t nowUtc() = 0;

    // Manually set the RTC to the given UTC epoch (used by NTP-sync flows).
    virtual void setUtc(int64_t epoch) = 0;

    // Expand a UTC epoch into wall-clock fields for the given POSIX TZ.
    // Example posixTZ values:
    //   "CST-8"                        (Shenzhen; UTC+8, no DST)
    //   "PST8PDT,M3.2.0,M11.1.0"        (America/Los_Angeles)
    //   "CET-1CEST,M3.5.0,M10.5.0/3"   (Europe/Zurich)
    virtual LocalDateTime toLocal(int64_t utc, const char *posixTZ) = 0;

    // Convenience: minute-of-day (0..1439) for a given UTC + zone.
    // Default impl expands via toLocal; platforms can override if cheaper.
    virtual int minuteOfDay(int64_t utc, const char *posixTZ) {
        LocalDateTime t = toLocal(utc, posixTZ);
        return t.hour * 60 + t.minute;
    }
};

} // namespace wmt
