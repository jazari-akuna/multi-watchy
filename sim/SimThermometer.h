#pragma once
// Desktop-simulator IThermometer. Returns a fixed °C value (configurable
// via the constructor or setCelsius) so the tempco math in DriftTracker is
// fully deterministic for screenshots and tests.

#include "hal/IThermometer.h"

#include <cstdint>

namespace wmt {

class SimThermometer final : public IThermometer {
public:
    explicit SimThermometer(int8_t defaultC = 25) : t_(defaultC) {}

    int8_t readCelsius() override { return t_; }

    // Sim-only: change the reading between renders.
    void setCelsius(int8_t t) { t_ = t; }

private:
    int8_t t_;
};

} // namespace wmt
