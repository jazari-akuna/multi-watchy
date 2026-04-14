#pragma once
#include "hal/Types.h"

namespace wmt {
class IDisplay;

// Checkmark inside a round-ish 18x18 badge. Used on the event card.
void drawCheckmark(IDisplay *d, int16_t x, int16_t y, Ink fg, Ink bg);

// Battery icon, 16x6 body + 2x3 cap. `bars` is 0..3.
void drawBatteryIcon(IDisplay *d, int16_t x, int16_t y, int bars, Ink fg, Ink bg);

// Vertical battery: 9 wide × 20 tall footprint (cap included). `fraction`
// is the charge level in [0.0, 1.0] — the body's interior is filled from
// the bottom up by fraction * innerHeight pixels. Reads as a continuous
// meter (not discrete segments).
void drawVerticalBattery(IDisplay *d, int16_t x, int16_t y, float fraction, Ink fg, Ink bg);

// WiFi-on indicator (small, ~10x10).
void drawWifiIcon(IDisplay *d, int16_t x, int16_t y, Ink fg, Ink bg);
}
