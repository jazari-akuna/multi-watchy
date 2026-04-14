#include "WatchyThermometer.h"

#include <Watchy.h>      // brings in `extern RTC_DATA_ATTR BMA423 sensor;`
#include <math.h>

namespace wmt {

int8_t WatchyThermometer::readCelsius() {
    // BMA423::readTemperature() returns °C as float. The driver returns 0
    // when the sensor reports "no valid data" (register byte 0x80), and
    // can return NaN if the I2C read itself fails -- in either case we
    // fall back to 25 °C so the tempco correction stays sane.
    float t = sensor.readTemperature();
    if (isnan(t) || t == 0.0f) return 25;
    if (t < -40.0f) return -40;
    if (t >  85.0f) return  85;
    return static_cast<int8_t>(lroundf(t));
}

} // namespace wmt
