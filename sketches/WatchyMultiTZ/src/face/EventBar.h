#pragma once
// Event-card horizontal bar.
//
// Spans the next 8 hours starting "now" (left edge = nowUtc, right edge =
// nowUtc + 8h). Each event that overlaps the range is drawn as a grey
// (2x2-hatched) rectangle at its proportional x-position. The "now" pin
// always sits at the left edge of the bar, so the bar doubles as a
// horizontal "agenda at a glance".

#include "../hal/IDisplay.h"
#include "../hal/Types.h"
#include "Schedule.h"

namespace wmt {

class EventBar {
public:
    // Render the bar. `nowUtc` in seconds since Unix epoch. `events` is a
    // caller-owned array of `n` events (may include events outside the
    // window; those are clipped to the bar's range or skipped entirely).
    //
    // The BAR BACKGROUND tracks the main zone's work/night/lunch schedule
    // for the next 8 hours:
    //   work  -> white  (Ink::Bg)
    //   off   -> black  (Ink::Fg)
    //   lunch -> 2x2 checker (reads as grey)
    // Events overlay as 2x2-hatched blocks on top of that background. Pin
    // always sits at the left edge.
    //
    // `nowMainMin` is the main zone's local minute-of-day NOW; the bar
    // advances linearly from there. (DST transitions inside the 8-hour
    // window are rare and ignored for simplicity.)
    //
    // `borderFg`/`borderBg` are the CARD's inks — used only for the outer
    // 1-px border so it adapts to card inversion. The interior always
    // renders in light-mode inks.
    static void render(IDisplay *d, Rect bar,
                       int64_t nowUtc, const Event *events, int n,
                       int16_t nowMainMin, const Schedule &mainSchedule,
                       Ink borderFg, Ink borderBg);
};

} // namespace wmt
