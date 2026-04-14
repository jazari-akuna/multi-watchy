#pragma once
#include "hal/Types.h"

namespace wmt {
class IDisplay;

// Each flag is a 20x14 px icon, designed for 1-bit readability.
void drawFlagSzx(IDisplay *d, int16_t x, int16_t y, Ink fg, Ink bg);
void drawFlagSfo(IDisplay *d, int16_t x, int16_t y, Ink fg, Ink bg);
void drawFlagZrh(IDisplay *d, int16_t x, int16_t y, Ink fg, Ink bg);
}
