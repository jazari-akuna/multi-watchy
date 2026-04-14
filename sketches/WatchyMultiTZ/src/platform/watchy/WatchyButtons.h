#pragma once
// Watchy-specific IButtons. Wraps ESP32 ext1 wake status + digitalRead polls.

#include "../../hal/IButtons.h"

namespace wmt {

// Reads the four physical buttons on the Watchy. Wake-cause comes from the
// ESP32 ext1 wake-status mask; sync polling uses digitalRead().
class WatchyButtons final : public IButtons {
public:
    WatchyButtons(); // configures pinMode() for all four button pins
    Button wakeButton() override;
    bool   isPressed(Button b) override;
};

} // namespace wmt
