#pragma once
// Desktop-simulator IButtons. Trivial: holds a single "last wake button"
// that the sim driver can set before rendering a frame. `isPressed` always
// returns false — the sim doesn't have a settle loop.

#include "../sketches/WatchyMultiTZ/src/hal/IButtons.h"

namespace wmt {

class SimButtons : public IButtons {
public:
    SimButtons() = default;
    ~SimButtons() override = default;

    Button wakeButton() override      { return wake_; }
    bool   isPressed(Button) override { return false; }

    // Sim-only: pre-load the next wakeButton() result.
    void setFakeWake(Button b) { wake_ = b; }

private:
    Button wake_ = Button::None;
};

} // namespace wmt
