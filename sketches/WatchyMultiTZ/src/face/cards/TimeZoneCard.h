#pragma once
// TimeZoneCard — renders a single timezone tile in either of two sizes:
//
//   * ALT   (isMain == false) : compact 99×49 tile used for the top-left /
//                               top-right secondary zones. Shows a day-bar,
//                               a flag, HH:MM in DSEG7-25, and an optional
//                               day-delta badge ("+1", "-1") if the zone is
//                               on a different calendar day from the main.
//   * MAIN  (isMain == true)  : big 198×70 tile used for the primary zone.
//                               Shows a thicker day-bar, flag, HH:MM in
//                               DSEG7-39, and a right-side WDAY / DD / MON
//                               date stack in FreeMonoBold9pt7b.
//
// Night inversion: if `isNight(localMinute, zone.schedule)` is true, the
// whole card inverts (black fill, white ink). During work hours the card
// uses the default light scheme (white fill, black ink).
//
// This type holds no state. All inputs flow through `render()` parameters
// so the caller (WatchFace) is fully in control of layout and ordering.

#include "hal/Types.h"

namespace wmt {

class IDisplay;
struct TimeZone;
struct RenderCtx;

class TimeZoneCard {
public:
    // `slot`     : outer rectangle in screen pixels. The card fills it.
    // `zone`     : timezone descriptor (label, posix TZ, schedule, flag fn).
    // `ctx`      : render context — used for `ctx.clock->toLocal()` and
    //              `ctx.nowUtc`.
    // `isMain`   : true -> big 198×70 layout; false -> compact 99×49 layout.
    // `dayDelta` : calendar-day offset from the main zone. Only rendered on
    //              alt cards; pass 0 for the main card (and for alt cards
    //              on the same day as main, which suppresses the badge).
    static void render(IDisplay *d, Rect slot,
                       const TimeZone &zone, const RenderCtx &ctx,
                       bool isMain, int dayDelta);
};

} // namespace wmt
