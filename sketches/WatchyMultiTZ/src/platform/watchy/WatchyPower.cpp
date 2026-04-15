#include "WatchyPower.h"

#include <Watchy.h>
#include <Arduino.h>

namespace wmt {

float WatchyPower::batteryVoltage() {
    return parent_->getBatteryVoltage();
}

void WatchyPower::deepSleep() {
    parent_->deepSleep(); // configures wake sources + esp_deep_sleep_start()
}

void WatchyPower::delayMs(uint32_t ms) {
    ::delay(ms);
}

uint32_t WatchyPower::millisNow() {
    return ::millis();
}

void WatchyPower::buzz(int pulses) {
    // 80 ms ON / 120 ms OFF per pulse — short, distinct from the
    // library's default 100 ms × 4 (which feels like one long buzz).
    if (pulses <= 0) return;
    for (int i = 0; i < pulses; ++i) {
        // vibMotor(intervalMs, length): toggles the pin `length` times,
        // delaying intervalMs between toggles. length=2 → on then off.
        parent_->vibMotor(80, 2);
        if (i + 1 < pulses) ::delay(120);
    }
}

} // namespace wmt
