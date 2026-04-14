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

} // namespace wmt
