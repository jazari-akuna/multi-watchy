#pragma once
// EventCard — renders the bottom 198x75 card of the watchface grid.
//
// Layout (see Watchy-26.04 interface.png, bottom card):
//   * An 8-hour EventBar strip at the top, showing blocks for the next few
//     upcoming events relative to nowUtc.
//   * A checkmark glyph at the bottom-left.
//   * The first upcoming event's title, centred in the card body.
//   * The first event's start-end time range, centred under the title.
//
// The card is "inversion-aware": the caller (WatchFace) passes the MAIN
// zone's current inversion state through so the event card tracks whichever
// ink the main card is currently using. Rationale: event times are rendered
// in the main zone's wall-clock, so the visual coupling to the main card
// reads as intentional rather than arbitrary.
//
// Stateless; all inputs flow through render() parameters.

#include "../../hal/Types.h"

namespace wmt {

class IDisplay;
struct RenderCtx;

class EventCard {
public:
    // `slot`      : outer rectangle in screen pixels. The card fills it.
    // `ctx`       : render context — used for `ctx.events->nextEvents()`,
    //               `ctx.clock->toLocal()`, and `ctx.nowUtc`.
    // `inverted`  : if true, swap fg/bg inks (to match main card's state).
    static void render(IDisplay *d, Rect slot,
                       const RenderCtx &ctx, bool inverted);
};

} // namespace wmt
