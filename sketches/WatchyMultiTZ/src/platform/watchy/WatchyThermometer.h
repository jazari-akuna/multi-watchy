#pragma once
// Watchy-specific IThermometer. Reads the BMA423 on-die temperature sensor
// already initialized by the Watchy library (`extern BMA423 sensor;`).
// Resolution is coarse (~±2 °C) but adequate for crystal tempco correction.

#include "../../hal/IThermometer.h"

namespace wmt {

// IThermometer impl that bridges to the Watchy library's BMA423 `sensor`
// global. Stateless; clamps and falls back to 25 °C on read failure so the
// tempco math doesn't produce a wild correction on an I2C glitch.
class WatchyThermometer final : public IThermometer {
public:
    WatchyThermometer() = default;

    int8_t readCelsius() override;
};

} // namespace wmt
