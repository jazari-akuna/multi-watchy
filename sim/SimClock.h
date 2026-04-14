#pragma once
// Desktop-simulator implementation of wmt::IClock.
//
// Holds a single UTC epoch that the sim driver can pin to a specific
// wall-clock moment (so PNG fixtures are deterministic). `toLocal` uses the
// host libc — POSIX TZ string → `setenv("TZ", ...) + tzset() + localtime_r`,
// matching the real Watchy implementation, then restores TZ=UTC0.

#include "../sketches/WatchyMultiTZ/src/hal/IClock.h"

#include <stdint.h>

namespace wmt {

class SimClock : public IClock {
public:
    SimClock() = default;
    ~SimClock() override = default;

    int64_t       nowUtc() override;
    void          setUtc(int64_t epoch) override;
    LocalDateTime toLocal(int64_t utc, const char *posixTZ) override;

    // Sim-only: parse an ISO-8601 string (e.g. "2026-04-14T09:41:00Z") and
    // store the UTC epoch. Returns true on success. Accepts trailing "Z"
    // or "+HH:MM" / "-HH:MM". Whitespace and fractional seconds are not
    // supported — inputs are expected to come from test fixtures.
    bool setFakeTime(const char *iso8601);

private:
    int64_t utc_ = 0;
};

} // namespace wmt
