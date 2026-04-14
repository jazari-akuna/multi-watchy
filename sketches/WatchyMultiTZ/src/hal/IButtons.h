#pragma once
// Button input. Two modes:
//   - `wakeButton()` : at the start of a wake cycle, returns which button
//                     caused ext1 wakeup (or None for timer/alarm wakes).
//   - `isPressed(b)` : synchronous poll of the GPIO during a "settle loop".

#include "Types.h"

namespace wmt {

class IButtons {
public:
    virtual ~IButtons() = default;
    virtual Button wakeButton() = 0;
    virtual bool   isPressed(Button b) = 0;
};

} // namespace wmt
