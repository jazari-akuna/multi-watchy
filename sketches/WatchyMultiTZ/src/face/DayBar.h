#pragma once
// Horizontal day-bar renderer.
//
// Each TZ card hosts one DayBar at the top showing the zone's
// work/lunch/night segments between `barStartMin` and `barEndMin`, with a
// 2-px vertical "now" pin. Noon is always centred in the bar's x-range,
// which keeps the visual identity of the bar stable across zones whose
// schedules differ.
//
// Visual spec (see Watchy-26.04 interface.png):
//   - 1-px border around the full bar rect (in fg ink)
//   - Work minutes AND night-within-range: solid fg
//   - Lunch minutes: 2x2 checker of fg/bg (reads as grey on the panel)
//   - 2-px pin at current local minute, inverted against underlying pixels
//
// The start/end of the bar axis are set once (at startup) via configure() so
// every zone shares the same minute-axis regardless of its own schedule.

#include "../hal/IDisplay.h"
#include "../hal/Types.h"
#include "Schedule.h"

namespace wmt {

class DayBar {
public:
    // Configure the global bar minute-axis. Call once at startup — typically
    // with values derived via wmt::earliestStart / latestEnd / halfWidth
    // applied over the compile-time zone array in settings.h.
    static void configure(int16_t barStartMin, int16_t barEndMin);

    // Draw a TZ day bar into `bar`. `nowMinute` is the zone's current
    // minute-of-day (0..1439). `fg`/`bg` are the card's inks — swap them
    // to draw the bar inverted.
    static void render(IDisplay *d, Rect bar,
                       const Schedule &s, int16_t nowMinute,
                       Ink fg, Ink bg);

private:
    static int16_t barStart_;   // minute-of-day mapped to bar's left edge
    static int16_t barEnd_;     // minute-of-day mapped to bar's right edge

    // Map `minute` (0..1439) to an x-coordinate inside `bar`. Values outside
    // [barStart_, barEnd_] clamp to the corresponding edge.
    static int16_t minuteToX(int16_t minute, Rect bar);
};

} // namespace wmt
