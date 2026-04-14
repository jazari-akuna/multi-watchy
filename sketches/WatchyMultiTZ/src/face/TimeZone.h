#pragma once
// A TimeZone is pure data: a short label, a human-readable city name, a
// POSIX TZ string (passed through IClock::toLocal for wall-clock conversion),
// the working-day schedule used by the day-bar, and a procedural flag drawer.
//
// The flag is rendered procedurally (not a bitmap) so each zone can own its
// icon in a few lines of drawRect/fillRect calls — keeping binary size small
// and letting the icon adapt to inverted cards by simply swapping fg/bg.

#include "../hal/Types.h"
#include "Schedule.h"

namespace wmt {

// Forward-decl so this header does not pull in IDisplay.h.
class IDisplay;

struct TimeZone {
    const char *label;      // 3-letter airport code, e.g. "SZX", "SFO", "ZRH"
    const char *city;       // human-readable, e.g. "Shenzhen"
    const char *posixTZ;    // POSIX TZ string (see IClock::toLocal docs)
    Schedule    schedule;   // work / lunch minute-of-day ranges
    // Procedural flag drawer. `(x, y)` is the top-left of the drawable cell;
    // `fg`/`bg` are the CURRENT card's inks (already swapped if inverted).
    void (*drawFlag)(IDisplay *d, int16_t x, int16_t y, Ink fg, Ink bg);
    int8_t      flagW;      // pixel width of the drawn flag (20 rectangular / 14 square)
    int8_t      flagH;      // pixel height of the drawn flag (always 14 at time of writing)
};

} // namespace wmt
