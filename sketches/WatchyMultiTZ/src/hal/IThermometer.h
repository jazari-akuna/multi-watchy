#pragma once
// Ambient-temperature read for crystal tempco correction.
//
// On Watchy this wraps the BMA423's on-die temperature sensor
// (`sensor.readTemperature()`, returns °C as int8_t — coarse, ±2 °C).
// On the sim this returns a fixed value (25 °C by default) so the face
// renders deterministically.

#include "Types.h"

namespace wmt {

class IThermometer {
public:
    virtual ~IThermometer() = default;

    // Current ambient temperature in integer degrees Celsius.
    // Returns 25 (a reasonable room-temp default) on read failure so
    // the tempco math doesn't produce a wild "correction" on an I²C
    // glitch.
    virtual int8_t readCelsius() = 0;
};

} // namespace wmt
