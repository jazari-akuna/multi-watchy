#include "EventCard.h"
#include "../../hal/IDisplay.h"
#include "../../hal/IClock.h"
#include "../../hal/IEventProvider.h"
#include "../EventBar.h"
#include "../WatchFace.h"          // RenderCtx
#include "../../assets/icons.h"
#include <Fonts/FreeMonoBold9pt7b.h>
#include <stdio.h>

// Notes on design decisions:
//
// * TZ for event time formatting. The first pass uses the fixed POSIX string
//   "UTC0" — RenderCtx does not (yet) carry the main zone's posixTZ. A later
//   refinement should either add a `const char *mainTZ;` field to RenderCtx
//   or pass the main TimeZone reference through explicitly. Leaving the
//   scaffold compiling against the simplest shape for now.  TODO(mainTZ).
//
// * En-dash. The mockup shows "20:15 – 21:15" with a U+2013 en-dash. Neither
//   FreeMonoBold9pt7b nor Seven_Segment10pt7b (the two fonts used on this
//   watchface) include glyphs outside 0x20..0x7E, so we render ASCII hyphen
//   instead. This stays legible and avoids a tofu box on the panel.

namespace wmt {

void EventCard::render(IDisplay *d, Rect slot,
                       const RenderCtx &ctx, bool /*inverted_ignored*/) {
    // --- 2. Fetch up to 4 upcoming events (pre-determine inversion too) ---
    Event events[4];
    int n = ctx.events->nextEvents(ctx.nowUtc, events, 4);

    // Inversion rule: invert ONLY when the CURRENT time sits inside an
    // event's time range. (Earlier versions inverted based on the main
    // zone's night state; the user preferred the in-event rule.)
    bool inEvent = false;
    for (int i = 0; i < n; i++) {
        if (events[i].startUtc <= ctx.nowUtc && ctx.nowUtc < events[i].endUtc) {
            inEvent = true;
            break;
        }
    }
    const Ink bg = inEvent ? Ink::Fg : Ink::Bg;
    const Ink fg = inEvent ? Ink::Bg : Ink::Fg;

    // --- 1. Card background + 1-px outline -------------------------------
    d->fillRect(slot, bg);
    d->drawRect(slot, fg);

    // --- 3. 8-hour EventBar at the top -----------------------------------
    //   Inset 3 px on left/right/top; 10 px tall. The EventBar is drawn in
    //   LIGHT-MODE inks regardless of card inversion (same policy as the
    //   TZ day bars) so the graphic reads consistently across states.
    Rect barRect = {
        (int16_t)(slot.x + 3),
        (int16_t)(slot.y + 3),
        (int16_t)(slot.w - 6),
        (int16_t)10
    };
    // EventBar's outer border follows the card's inversion state; its
    // interior shows the MAIN zone's schedule (work white / off black /
    // lunch checker) over the next 8 hours, with events overlaid as gray
    // hatched blocks.
    const int16_t nowMainMin =
        (ctx.clock && ctx.mainTZ)
            ? (int16_t)ctx.clock->minuteOfDay(ctx.nowUtc, ctx.mainTZ)
            : (int16_t)0;
    const Schedule defaultSchedule{ 9*60, 17*60, 12*60, 13*60 };
    const Schedule &sched =
        ctx.mainSchedule ? *ctx.mainSchedule : defaultSchedule;
    EventBar::render(d, barRect, ctx.nowUtc, events, n,
                     nowMainMin, sched, fg, bg);

    // --- 4. Checkmark icon, bottom-left ----------------------------------
    //   Icon is 16x16. Sit it 6 px in from the left edge and 6 px up from
    //   the bottom edge — on a 75-tall slot that puts its top at y+53.
    constexpr int16_t ICON_SIZE = 16;
    const int16_t iconX = (int16_t)(slot.x + 6);
    const int16_t iconY = (int16_t)(slot.y + slot.h - ICON_SIZE - 6);
    drawCheckmark(d, iconX, iconY, fg, bg);

    // --- 5. Event title, centred in the body -----------------------------
    d->setTextColour(fg);
    d->setFont((const Font*)&FreeMonoBold9pt7b);

    const char *title = (n > 0) ? events[0].title : "No events";

    // Truncate overly long titles with a trailing ellipsis. The card is
    // 198 px wide; FreeMonoBold9pt7b is a fixed-width font at ~11 px per
    // glyph, so ~17 chars fit in the centred area once we account for the
    // checkmark. Keep this cheap — no heap, char buffer on stack.
    char titleBuf[EVENT_TITLE_MAX + 1];
    {
        constexpr int MAX_TITLE_CHARS = 17;
        int i = 0;
        while (i < MAX_TITLE_CHARS && title[i] != '\0') {
            titleBuf[i] = title[i];
            ++i;
        }
        if (title[i] != '\0' && i >= 3) {
            // Overflowed — replace the last 3 chars with "..."
            titleBuf[i - 3] = '.';
            titleBuf[i - 2] = '.';
            titleBuf[i - 1] = '.';
        }
        titleBuf[i] = '\0';
    }

    int16_t tw = 0, th = 0;
    d->measureText(titleBuf, tw, th);

    // Baseline placement: the body is y=slot.y+13..slot.y+75 (62 px). Put
    // the title baseline ~27 px below the bar bottom so the glyph cap sits
    // comfortably above vertical centre, leaving room for the time row.
    const int16_t titleBaselineY = (int16_t)(slot.y + 13 + 27);
    const int16_t titleX = (int16_t)(slot.x + (slot.w - tw) / 2);
    d->drawText(titleX, titleBaselineY, titleBuf);

    // --- 6. Time range "HH:MM - HH:MM" under the title -------------------
    if (n > 0) {
        // Event times render in the MAIN zone's wall-clock. RenderCtx carries
        // that zone's POSIX TZ string (populated by WatchFace).
        const char *posixTZ = (ctx.mainTZ != nullptr) ? ctx.mainTZ : "UTC0";

        LocalDateTime s = ctx.clock->toLocal(events[0].startUtc, posixTZ);
        LocalDateTime e = ctx.clock->toLocal(events[0].endUtc,   posixTZ);

        char buf[16];
        // ASCII hyphen intentional (see note at top of file re: en-dash).
        snprintf(buf, sizeof buf, "%02d:%02d - %02d:%02d",
                 (int)s.hour, (int)s.minute,
                 (int)e.hour, (int)e.minute);

        d->measureText(buf, tw, th);
        const int16_t rangeBaselineY = (int16_t)(titleBaselineY + 20);
        const int16_t rangeX = (int16_t)(slot.x + (slot.w - tw) / 2);
        d->drawText(rangeX, rangeBaselineY, buf);
    }
}

} // namespace wmt
