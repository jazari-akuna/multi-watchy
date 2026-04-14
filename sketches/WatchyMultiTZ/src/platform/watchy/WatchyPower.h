#pragma once
// Watchy-specific IPower. Thin wrapper over Watchy base-class helpers plus
// Arduino delay/millis.

#include "../../hal/IPower.h"

class Watchy; // forward-decl from Watchy lib

namespace wmt {

// Delegates battery / sleep calls to the Watchy library's orchestrator
// (which owns board-specific ADC and ext1 wake-source setup).
class WatchyPower final : public IPower {
public:
    explicit WatchyPower(::Watchy *parent) : parent_(parent) {}

    float    batteryVoltage() override;
    void     deepSleep() override;        // never returns
    void     delayMs(uint32_t ms) override;
    uint32_t millisNow() override;

private:
    ::Watchy *parent_;
};

} // namespace wmt
