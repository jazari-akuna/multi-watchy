#pragma once
// Desktop-simulator IPower.
//   * batteryVoltage() returns a stored float (default 3.9 V).
//   * deepSleep() flips `shouldExit` so the sim driver knows to stop.
//   * delayMs() sleeps the host thread.
//   * millisNow() returns elapsed ms since the SimPower was constructed.

#include "../sketches/WatchyMultiTZ/src/hal/IPower.h"

#include <chrono>
#include <stdint.h>

namespace wmt {

class SimPower : public IPower {
public:
    SimPower();
    ~SimPower() override = default;

    float    batteryVoltage() override { return battery_; }
    void     deepSleep() override      { shouldExit_ = true; }
    void     delayMs(uint32_t ms) override;
    uint32_t millisNow() override;

    // Sim-only.
    void setFakeBattery(float v)  { battery_ = v; }
    bool shouldExit() const       { return shouldExit_; }
    void clearShouldExit()        { shouldExit_ = false; }

private:
    float battery_    = 3.9f;
    bool  shouldExit_ = false;
    std::chrono::steady_clock::time_point start_;
};

} // namespace wmt
