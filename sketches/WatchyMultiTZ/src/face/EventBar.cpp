#include "EventBar.h"

namespace wmt {

// Window width in seconds: 8 hours.
static constexpr int64_t WINDOW_SECS = 8 * 3600;

// Fill a rectangle with a 2x2 checker of fg/bg pixels (reads as grey).
// Used only for the "lunch" schedule shading.
static void fillHatch(IDisplay *d, Rect r, Ink fg, Ink bg) {
    for (int16_t yy = 0; yy < r.h; yy++) {
        for (int16_t xx = 0; xx < r.w; xx++) {
            const bool even = (((xx) ^ (yy)) & 1) == 0;
            d->drawPixel((int16_t)(r.x + xx), (int16_t)(r.y + yy),
                         even ? fg : bg);
        }
    }
}

// Per-column schedule category — used to pick event-border ink that
// contrasts with the bar background adjacent to each event edge.
enum ColBg : uint8_t { BG_WORK = 0, BG_NIGHT = 1, BG_LUNCH = 2 };

// Event block — exactly as the user specified:
//   Body: solid bg (white) rectangle half the bar height, vertically
//         centred, inset 1 px horizontally from the event's full rect
//         so there's room for left + right borders.
//   Border: 1-px frame around the body. Each border pixel is the
//           INVERT of the schedule bg at that pixel's column, so the
//           frame is always visible against work-white, night-black,
//           lunch-checker, and every transition between them.
static void fillEventBlock(IDisplay *d, Rect r, Ink fg, Ink bg,
                           Rect inner, const uint8_t *colBg,
                           int16_t innerW) {
    if (r.w <= 0 || r.h <= 0) return;

    auto schedInkAt = [&](int16_t absX) -> Ink {
        const int idx = absX - inner.x;
        if (idx < 0 || idx >= innerW) return bg;
        switch (colBg[idx]) {
            case BG_NIGHT: return fg;
            case BG_WORK:  return bg;
            case BG_LUNCH: return bg;   // checker — treat as light
            default:       return bg;
        }
    };
    auto edgeInkAt = [&](int16_t absX) -> Ink {
        return (schedInkAt(absX) == fg) ? bg : fg;
    };

    // Frame rect: height = bar.h / 2 + 2 (for top + bottom border rows),
    // vertically centred. Full event width.
    const int16_t bodyH  = (int16_t)(r.h / 2);
    const int16_t frameH = (int16_t)(bodyH + 2);
    const int16_t frameY = (int16_t)(r.y + (r.h - frameH) / 2);
    const int16_t frameBottomY = (int16_t)(frameY + frameH - 1);

    // Body: inset 1 px horizontally from r, inset 1 px vertically from frame.
    Rect body;
    body.y = (int16_t)(frameY + 1);
    body.h = bodyH;
    if (r.w >= 3) {
        body.x = (int16_t)(r.x + 1);
        body.w = (int16_t)(r.w - 2);
    } else {
        body.x = r.x;
        body.w = 0;
    }

    // Solid white body.
    if (body.w > 0 && body.h > 0) {
        d->fillRect(body, bg);
    }

    // Top + bottom border rows — drawn only where the schedule bg is NOT
    // night. On night columns the would-be "white" border pixel would
    // just blend with the white body anyway, so skipping it lets the
    // schedule's own black show through and the event reads as a bare
    // white rectangle against the night background.
    for (int16_t xx = 0; xx < r.w; xx++) {
        const int16_t absX = (int16_t)(r.x + xx);
        if (schedInkAt(absX) == fg) continue;   // night → no border
        const Ink edge = edgeInkAt(absX);
        d->drawPixel(absX, frameY,        edge);
        d->drawPixel(absX, frameBottomY,  edge);
    }

    // Left + right border columns — same rule: skip when adjacent
    // schedule column is night.
    const int16_t rightAbs = (int16_t)(r.x + r.w - 1);
    const bool    leftIsNight  = (schedInkAt(r.x)      == fg);
    const bool    rightIsNight = (schedInkAt(rightAbs) == fg);
    const Ink     leftInk  = edgeInkAt(r.x);
    const Ink     rightInk = edgeInkAt(rightAbs);
    for (int16_t yy = (int16_t)(frameY + 1); yy < frameBottomY; yy++) {
        if (!leftIsNight)             d->drawPixel(r.x,      yy, leftInk);
        if (r.w >= 2 && !rightIsNight) d->drawPixel(rightAbs, yy, rightInk);
    }
}

// Map a UTC epoch to an x inside the bar. Clamps to [bar.x, bar.x+bar.w-1].
static int16_t utcToX(int64_t utc, int64_t start, Rect bar) {
    if (bar.w <= 0) return bar.x;
    if (utc <= start)                      return bar.x;
    if (utc >= start + WINDOW_SECS)        return (int16_t)(bar.x + bar.w - 1);
    const int64_t off = utc - start;
    int64_t px = bar.x + (off * (int64_t)(bar.w - 1)) / WINDOW_SECS;
    if (px < bar.x)                 return bar.x;
    if (px > bar.x + bar.w - 1)     return (int16_t)(bar.x + bar.w - 1);
    return (int16_t)px;
}

void EventBar::render(IDisplay *d, Rect bar,
                      int64_t nowUtc, const Event *events, int n,
                      int16_t nowMainMin, const Schedule &mainSchedule,
                      Ink borderFg, Ink borderBg)
{
    if (bar.w < 3 || bar.h < 3) return;

    // Outer border follows the card's inversion (passed in via borderFg).
    // Interior (schedule shading + event blocks + pin) always uses
    // LIGHT-MODE inks so the bar reads consistently on inverted/normal.
    constexpr Ink iFg = Ink::Fg;
    constexpr Ink iBg = Ink::Bg;
    (void)borderBg;  // retained in the signature for API symmetry

    // 1. Outer border.
    d->drawRect(bar, borderFg);

    Rect inner = { (int16_t)(bar.x + 1), (int16_t)(bar.y + 1),
                   (int16_t)(bar.w - 2), (int16_t)(bar.h - 2) };
    if (inner.w <= 0 || inner.h <= 0) return;

    // 2. Shade the 8-hour window by the main zone's schedule. Per column:
    //    minuteAtCol = (nowMainMin + col_seconds/60) mod 1440
    //    where col_seconds = col_frac * 8 * 3600.
    //    work → iBg (white), off → iFg (black), lunch → 2x2 checker.
    // Record which category each column is in so step 3 can pick border
    // colours for event blocks that invert the adjacent schedule bg.
    uint8_t colBg[256];
    const int16_t innerW = inner.w > 256 ? 256 : inner.w;
    for (int16_t col = 0; col < inner.w; col++) {
        const int64_t offSec = ((int64_t)col * WINDOW_SECS) / (inner.w - 1 > 0 ? inner.w - 1 : 1);
        int32_t m = (int32_t)nowMainMin + (int32_t)(offSec / 60);
        m = ((m % 1440) + 1440) % 1440;

        const bool inLunch = (m >= mainSchedule.lunchStartMin &&
                              m <  mainSchedule.lunchEndMin);
        const bool inWork  = (m >= mainSchedule.workStartMin  &&
                              m <  mainSchedule.workEndMin);
        if (col < innerW) {
            colBg[col] = inLunch ? BG_LUNCH : (inWork ? BG_WORK : BG_NIGHT);
        }

        const int16_t xAbs = (int16_t)(inner.x + col);
        if (inLunch) {
            // 2x2 checker vertically.
            const bool colEven = (col & 1) == 0;
            for (int16_t row = 0; row < inner.h; row++) {
                const bool rowEven = (row & 1) == 0;
                const bool useFg   = (colEven == rowEven);
                d->drawPixel(xAbs, (int16_t)(inner.y + row), useFg ? iFg : iBg);
            }
        } else if (inWork) {
            d->drawVLine(xAbs, inner.y, inner.h, iBg);
        } else {
            d->drawVLine(xAbs, inner.y, inner.h, iFg);
        }
    }

    // 3. Each event within range → hatched block inside `inner`, overlaid
    //    on top of the schedule shading so it reads as "something happens
    //    during this time window".
    const int64_t winEnd = nowUtc + WINDOW_SECS;
    for (int i = 0; i < n; i++) {
        const Event &e = events[i];
        if (e.endUtc   <= nowUtc) continue;   // already past
        if (e.startUtc >= winEnd) continue;   // outside window on the right

        const int16_t x0 = utcToX(e.startUtc, nowUtc, inner);
        const int16_t x1 = utcToX(e.endUtc,   nowUtc, inner);
        int16_t w  = (int16_t)(x1 - x0 + 1);
        if (w < 1) w = 1;

        Rect block = { x0, inner.y, w, inner.h };
        if (block.x < inner.x) {
            block.w = (int16_t)(block.w - (inner.x - block.x));
            block.x = inner.x;
        }
        if (block.x + block.w > inner.x + inner.w) {
            block.w = (int16_t)(inner.x + inner.w - block.x);
        }
        if (block.w <= 0 || block.h <= 0) continue;

        fillEventBlock(d, block, iFg, iBg, inner, colBg, innerW);
    }

    // 4. Pin at left edge — 2 px wide, full bar height, in iFg so it stands
    //    out against either schedule background (white work / black night).
    d->drawVLine(bar.x,                bar.y, bar.h, iFg);
    d->drawVLine((int16_t)(bar.x + 1), bar.y, bar.h, iFg);
}

} // namespace wmt
