#include "DayBar.h"

namespace wmt {

// Defaults span a "typical workday" — configure() overwrites at startup.
int16_t DayBar::barStart_ =  7 * 60;   // 07:00
int16_t DayBar::barEnd_   = 22 * 60;   // 22:00

void DayBar::configure(int16_t barStartMin, int16_t barEndMin) {
    barStart_ = barStartMin;
    barEnd_   = barEndMin;
}

// Linear minute → x within the bar's inner drawable range. Clamps at edges
// when `minute` lies outside [barStart_, barEnd_]. Returns the left x of a
// 1-pixel-wide column (which leaves room for the pin's 2-px width when the
// caller adds 1).
int16_t DayBar::minuteToX(int16_t minute, Rect bar) {
    if (barEnd_ <= barStart_) return bar.x;
    const int16_t innerX = (int16_t)(bar.x + 1);
    const int16_t innerW = (int16_t)(bar.w - 2);
    if (innerW <= 0) return bar.x;

    if (minute <= barStart_) return innerX;
    if (minute >= barEnd_)   return (int16_t)(innerX + innerW - 1);

    const int32_t span = (int32_t)barEnd_ - (int32_t)barStart_;
    const int32_t off  = (int32_t)minute  - (int32_t)barStart_;
    // Floor division so each minute maps deterministically to the nearest
    // column; fine-grained enough given typical bar widths (~100-200 px).
    int32_t x = innerX + (off * (int32_t)(innerW - 1)) / span;
    if (x < innerX)                return innerX;
    if (x > innerX + innerW - 1)   return (int16_t)(innerX + innerW - 1);
    return (int16_t)x;
}

// Paint one column inside the bar's interior. `col` is the absolute x of
// the column; `bar` provides the y-range of the interior (1 px inset from
// the outer border on top and bottom).
static void paintColumn(IDisplay *d, int16_t col, Rect bar, Ink c) {
    const int16_t topY    = (int16_t)(bar.y + 1);
    const int16_t innerH  = (int16_t)(bar.h - 2);
    if (innerH <= 0) return;
    d->drawVLine(col, topY, innerH, c);
}

// Paint a single interior column according to the minute-of-day it maps to.
// Returns the logical Ink dominating the column (used by the pin to invert).
//
// Per user spec:
//   WORK  : white bar interior = bg  (leave as fillRect already did)
//   LUNCH : hatched (2x2 checker, reads as grey)
//   NIGHT : filled black = fg        (solid fg column)
// The OUTER border is always fg (handled by drawRect around the bar).
static Ink paintMinuteColumn(IDisplay *d, int16_t col, int16_t minute,
                             const Schedule &s, Rect bar,
                             Ink fg, Ink bg)
{
    const bool inLunch = (minute >= s.lunchStartMin && minute < s.lunchEndMin);
    const bool inWork  = (minute >= s.workStartMin  && minute < s.workEndMin);
    // Night-within-bar = inside [barStart, barEnd] but outside [workStart,
    // workEnd]. The DayBar's column walk only visits minutes in the bar
    // range, so `!inWork` here already implies "night-within-bar".

    if (inLunch) {
        // 2x2 checker. Reports `bg` back so the pin inverts to fg (best
        // contrast over the mostly-white-looking lunch stripes).
        const int16_t topY    = (int16_t)(bar.y + 1);
        const int16_t innerH  = (int16_t)(bar.h - 2);
        const bool    colEven = ((col - bar.x) & 1) == 0;
        for (int16_t i = 0; i < innerH; i++) {
            const bool rowEven = (i & 1) == 0;
            const bool useFg   = (colEven == rowEven);
            d->drawPixel(col, (int16_t)(topY + i), useFg ? fg : bg);
        }
        return bg;
    }

    if (!inWork) {
        // Night-within-bar: paint solid fg (black in light mode).
        paintColumn(d, col, bar, fg);
        return fg;
    }

    // Work column: leave the column as the already-drawn bg fill. No-op.
    return bg;
}

void DayBar::render(IDisplay *d, Rect bar,
                    const Schedule &s, int16_t nowMinute,
                    Ink borderFg, Ink borderBg)
{
    if (bar.w < 3 || bar.h < 3) return;

    // The BORDER follows the card's inversion state (caller passes the
    // card's fg/bg). The INTERIOR (work/lunch/night segments + pin) always
    // paints in LIGHT-MODE inks so the schedule reads consistently across
    // inverted and non-inverted cards.
    constexpr Ink iFg = Ink::Fg;   // interior "ink-on"  (dark pixel)
    constexpr Ink iBg = Ink::Bg;   // interior "ink-off" (light pixel)

    // 1. Outer 1-px border in the caller's (card's) fg.
    d->drawRect(bar, borderFg);

    // 2. Interior filled with light-mode bg first, then painted per-minute.
    Rect inner = { (int16_t)(bar.x + 1), (int16_t)(bar.y + 1),
                   (int16_t)(bar.w - 2), (int16_t)(bar.h - 2) };
    if (inner.w > 0 && inner.h > 0) {
        d->fillRect(inner, iBg);
    }

    // 3. Walk every interior column and paint according to the minute it
    //    represents. Inverse of minuteToX, clamped.
    if (barEnd_ > barStart_ && inner.w > 0) {
        const int32_t span = (int32_t)barEnd_ - (int32_t)barStart_;
        for (int16_t col = inner.x; col <= inner.x + inner.w - 1; col++) {
            const int32_t off = (int32_t)(col - inner.x);
            int32_t minute = (int32_t)barStart_ +
                             (off * span) / (int32_t)(inner.w - 1 > 0 ? inner.w - 1 : 1);
            if (minute < 0)    minute = 0;
            if (minute > 1439) minute = 1439;
            paintMinuteColumn(d, col, (int16_t)minute, s, bar, iFg, iBg);
        }
    }
    // Silence unused-parameter hint when caller-bg differs from interior-bg.
    (void)borderBg;

    // 4. Pin. 2-px vertical line spanning the full bar height (including
    //    the outer border rows). Colour is inverted against the underlying
    //    column segment at that x.
    int16_t pinMin = nowMinute;
    if (nowMinute < barStart_) pinMin = barStart_;
    if (nowMinute >= barEnd_)  pinMin = (int16_t)(barEnd_ - 1);

    int16_t pinX;
    if (nowMinute < barStart_) {
        pinX = bar.x;
    } else if (nowMinute >= barEnd_) {
        pinX = (int16_t)(bar.x + bar.w - 2);
    } else {
        pinX = minuteToX(pinMin, bar);
        if (pinX > bar.x + bar.w - 2) pinX = (int16_t)(bar.x + bar.w - 2);
        if (pinX < bar.x)             pinX = bar.x;
    }

    // Pin colour is derived from the LIGHT-MODE interior inks (not the
    // caller's fg/bg) because the interior itself is always painted in
    // light-mode. Work columns read as iBg, night columns as iFg, lunch
    // as iBg (checker). Invert for contrast.
    constexpr Ink piFg = Ink::Fg;
    constexpr Ink piBg = Ink::Bg;
    const bool inLunch = (pinMin >= s.lunchStartMin && pinMin < s.lunchEndMin);
    const bool inWork  = (pinMin >= s.workStartMin  && pinMin < s.workEndMin);
    Ink underlying;
    if (inLunch)      underlying = piBg;
    else if (!inWork) underlying = piFg;
    else              underlying = piBg;
    const Ink pinInk  = invert(underlying);

    d->drawVLine((int16_t)(pinX),     bar.y, bar.h, pinInk);
    d->drawVLine((int16_t)(pinX + 1), bar.y, bar.h, pinInk);
}

} // namespace wmt
