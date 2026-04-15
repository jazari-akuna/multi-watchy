#include "EventCard.h"
#include "../../hal/IDisplay.h"
#include "../../hal/IClock.h"
#include "../../hal/IEventProvider.h"
#include "../EventBar.h"
#include "../WatchFace.h"          // RenderCtx
#include "../../assets/icons.h"
#include "fonts/RobotoCondBold13.h"
#include "fonts/RobotoCondRegular13.h"
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

// Write the magnitude part of a countdown — "7 min", "2h", "2h30",
// "1d 3h", "3d" — into `out`. No prefix; caller supplies "In " or " left".
static void formatDuration(char *out, size_t n, int64_t sec) {
    if (sec < 60) { snprintf(out, n, "<1 min"); return; }
    if (sec < 3600) { snprintf(out, n, "%d min", (int)(sec / 60)); return; }
    const int64_t days   = sec / 86400;
    const int64_t remH   = (sec % 86400) / 3600;
    const int64_t remMin = (sec % 3600)  / 60;
    if (days >= 1) {
        if (remH > 0) snprintf(out, n, "%dd %dh", (int)days, (int)remH);
        else          snprintf(out, n, "%dd",     (int)days);
        return;
    }
    if (remMin == 0) snprintf(out, n, "%dh",     (int)remH);
    else             snprintf(out, n, "%dh%02d", (int)remH, (int)remMin);
}

// Format the top-line countdown based on where `nowUtc` sits relative to
// the event. Before start → "In 2h30". During → "2h10 left". After →
// empty (caller shouldn't reach this since the event is filtered out).
static void formatCountdown(char *out, size_t n,
                            int64_t nowUtc, int64_t startUtc, int64_t endUtc) {
    char mag[16];
    if (nowUtc < startUtc) {
        formatDuration(mag, sizeof mag, startUtc - nowUtc);
        snprintf(out, n, "In %s", mag);
    } else if (nowUtc < endUtc) {
        formatDuration(mag, sizeof mag, endUtc - nowUtc);
        snprintf(out, n, "%s left", mag);
    } else {
        out[0] = '\0';
    }
}

void EventCard::render(IDisplay *d, Rect slot,
                       const RenderCtx &ctx, bool /*inverted_ignored*/) {
    // --- 2. Fetch a window of upcoming events; pick one at cycleIdx ----
    constexpr int MAX_FETCH = 8;
    Event events[MAX_FETCH];
    int n = ctx.events->nextEvents(ctx.nowUtc, events, MAX_FETCH);
    const int cycle = (n > 0) ? (((ctx.eventCycleIdx % n) + n) % n) : 0;

    // Inversion rule: invert when ANY displayed event is currently in-progress
    // — the "selected" event for cycling, specifically.
    bool inEvent = (n > 0)
        && events[cycle].startUtc <= ctx.nowUtc
        && ctx.nowUtc < events[cycle].endUtc;
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
        (int16_t)14
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

    // Body line baselines are tuned to visually centre the 3-line stack in
    // the 62 px body. Countdown + time render in regular weight, only the
    // event title uses bold so the title "pops" against the two framing
    // lines.
    constexpr int16_t BODY_TOP       = 17;   // just below the (taller) EventBar
    constexpr int16_t COUNTDOWN_Y    = 15;
    constexpr int16_t TITLE_Y        = 33;
    constexpr int16_t TIME_Y         = 51;

    // --- 3b. Countdown line ("In 2h30", etc.) ---------------------------
    d->setTextColour(fg);
    d->setFont((const Font*)&RobotoCondRegular13);
    char cdBuf[20];
    if (n > 0) {
        formatCountdown(cdBuf, sizeof cdBuf, ctx.nowUtc,
                        events[cycle].startUtc, events[cycle].endUtc);
    } else {
        cdBuf[0] = '\0';
    }
    int16_t cdw = 0, cdh = 0;
    d->measureText(cdBuf, cdw, cdh);
    const int16_t cdBaselineY = (int16_t)(slot.y + BODY_TOP + COUNTDOWN_Y);
    const int16_t cdX = (int16_t)(slot.x + (slot.w - cdw) / 2);
    d->drawText(cdX, cdBaselineY, cdBuf);

    // --- 4. Checkmark icon, bottom-left ----------------------------------
    //   Icon is 16x16. Sit it 6 px in from the left edge and 6 px up from
    //   the bottom edge — on a 75-tall slot that puts its top at y+53.
    constexpr int16_t ICON_SIZE = 16;
    const int16_t iconX = (int16_t)(slot.x + 6);
    const int16_t iconY = (int16_t)(slot.y + slot.h - ICON_SIZE - 6);
    drawCheckmark(d, iconX, iconY, fg, bg);

    // --- 5. Event title (bold), centred in the body ----------------------
    d->setFont((const Font*)&RobotoCondBold13);
    const char *title = (n > 0) ? events[cycle].title : "No events";

    // Decode UTF-8 → Latin-1 in place. The font is indexed by Latin-1
    // codepoints (0x20..0xFF); titles arrive as UTF-8 (e.g. 'é' = 0xC3 0xA9),
    // so rendering raw would show "À©" instead of "é". Codepoints above
    // U+00FF are replaced with '?'. Max 24 output glyphs fits the centred
    // body area (~170 px after the checkmark).
    char titleBuf[EVENT_TITLE_MAX + 1];
    {
        constexpr int MAX_TITLE_CHARS = 24;
        int out = 0;
        int in  = 0;
        while (out < MAX_TITLE_CHARS && title[in] != '\0') {
            unsigned char c = (unsigned char)title[in];
            unsigned int cp;
            int adv;
            if (c < 0x80) {
                cp = c; adv = 1;
            } else if ((c & 0xE0) == 0xC0 && (unsigned char)title[in + 1] != 0) {
                cp = ((c & 0x1F) << 6) | ((unsigned char)title[in + 1] & 0x3F);
                adv = 2;
            } else if ((c & 0xF0) == 0xE0 &&
                       (unsigned char)title[in + 1] != 0 &&
                       (unsigned char)title[in + 2] != 0) {
                cp = 0xFFFF; adv = 3;   // force "?"
            } else {
                cp = 0xFFFF; adv = 1;
            }
            titleBuf[out++] = (cp <= 0xFF) ? (char)cp : '?';
            in += adv;
        }
        if (title[in] != '\0' && out >= 3) {
            titleBuf[out - 3] = '.';
            titleBuf[out - 2] = '.';
            titleBuf[out - 1] = '.';
        }
        titleBuf[out] = '\0';
    }

    int16_t tw = 0, th = 0;
    d->measureText(titleBuf, tw, th);

    const int16_t titleBaselineY = (int16_t)(slot.y + BODY_TOP + TITLE_Y);
    const int16_t titleX = (int16_t)(slot.x + (slot.w - tw) / 2);
    d->drawText(titleX, titleBaselineY, titleBuf);

    // --- 6. Time range "HH:MM - HH:MM" under the title -------------------
    if (n > 0) {
        d->setFont((const Font*)&RobotoCondRegular13);
        const char *posixTZ = (ctx.mainTZ != nullptr) ? ctx.mainTZ : "UTC0";
        LocalDateTime s = ctx.clock->toLocal(events[cycle].startUtc, posixTZ);
        LocalDateTime e = ctx.clock->toLocal(events[cycle].endUtc,   posixTZ);

        char buf[16];
        snprintf(buf, sizeof buf, "%02d:%02d - %02d:%02d",
                 (int)s.hour, (int)s.minute,
                 (int)e.hour, (int)e.minute);

        d->measureText(buf, tw, th);
        const int16_t rangeBaselineY = (int16_t)(slot.y + BODY_TOP + TIME_Y);
        const int16_t rangeX = (int16_t)(slot.x + (slot.w - tw) / 2);
        d->drawText(rangeX, rangeBaselineY, buf);
    }
}

} // namespace wmt
