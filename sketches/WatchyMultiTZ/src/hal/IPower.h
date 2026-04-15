#pragma once
// Power / sleep abstraction.

#include "Types.h"

namespace wmt {

class IPower {
public:
    virtual ~IPower() = default;

    // Battery voltage in volts (e.g. 3.80). Returns 0 on failure.
    virtual float batteryVoltage() = 0;

    // Enter deep sleep. Never returns on the Watchy platform. On the
    // simulator this is a no-op (the sim exits after one render).
    virtual void deepSleep() = 0;

    // Busy-wait for `ms` milliseconds. Used by the settle loop.
    virtual void delayMs(uint32_t ms) = 0;

    // Monotonic milliseconds since boot/start.
    virtual uint32_t millisNow() = 0;

    // Buzz the haptic motor `pulses` times, each pulse ~80 ms long with
    // ~120 ms gaps. Default impl is a no-op so sim/stub HALs don't need
    // to implement it.
    virtual void buzz(int /*pulses*/) {}
};

} // namespace wmt
