#include "TimeZoneCard.h"

#include "hal/IDisplay.h"
#include "hal/IClock.h"
#include "face/TimeZone.h"
#include "face/Schedule.h"
#include "face/DayBar.h"
#include "face/WatchFace.h"   // for RenderCtx

#include "fonts/DSEG7_Classic_Bold_25.h"
#include "fonts/DSEG7_Classic_Regular_15.h"
#include "fonts/DSEG7_Classic_Regular_39.h"
#include "fonts/Seven_Segment10pt7b.h"
#include "fonts/MyCompactFont.h"   // custom 4x7 — TUE / APR / +Nd badge
#include "assets/icons.h"         // drawVerticalBattery

#include <stdio.h>

namespace wmt {

namespace {

// wday is 0=Sun..6=Sat, matching LocalDateTime::wday convention.
const char *const WDAY3[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
const char *const MON3[]  = {"JAN","FEB","MAR","APR","MAY","JUN",
                             "JUL","AUG","SEP","OCT","NOV","DEC"};

// Layout constants for the two card sizes. Tuned by eye against the
// mockup; post-sim tuning by main Claude is expected.
//
//  ---- ALT card (99×49) ------------------------------------------
//   outer outline            : 1-px, full slot
//   day-bar                  : inset 3px left/right/top, height 8
//   flag                     : (slot.x + 5, slot.y + barH + 6)
//   HH:MM (DSEG7-25)         : baseline at (slot.x + 32, slot.y + 42)
//   day-delta badge          : bottom-right, baseline y = slot.y+h-5
//
//  ---- MAIN card (198×70) ----------------------------------------
//   outer outline            : 1-px, full slot
//   day-bar                  : inset 3px left/right/top, height 10
//   flag                     : (slot.x + 5, slot.y + barH + 8)
//   HH:MM (DSEG7-39)         : baseline at (slot.x + 34, slot.y + 60)
//   date stack (FreeMono9pt) : right-aligned ~28 px from slot.right,
//                              baselines at y = slot.y + 26/46/62

// TODO(HAL): drawRoundRect + rounded-pill helper would let us match the
// mockup's rounded-square aesthetic more closely; for now drawRect
// gives a clean minimal outline that's trivially portable.
constexpr int16_t ALT_BAR_H  = 8;
constexpr int16_t MAIN_BAR_H = 10;

// Draw a right-aligned string at baseline `(rightX, y)`.
// NOTE: `measureText` returns the BITMAP bounding box of the string, which
// for DSEG7-family fonts is much smaller than the cursor-advance width
// (the digits have a large `xOffset` inside their character cell). Passing
// that bbox-width to drawText places the CURSOR at (rightX - bbox), which
// makes the last glyph overshoot the right edge — e.g. "14" in Bold_25
// renders as "1" visible + "4" clipped. So we right-align by
// CURSOR-ADVANCE width instead of bitmap width.
inline void drawTextRight(IDisplay *d, int16_t rightX, int16_t y,
                          const char *s) {
    int16_t tw = 0, th = 0;
    d->measureText(s, tw, th);
    d->drawText((int16_t)(rightX - tw), y, s);
}

// Sum of xAdvance values for a string rendered with the given GFX font —
// matches the cursor position that Adafruit_GFX's print() would end at.
// Use this for RIGHT-ALIGNMENT of DSEG7-family text where drawTextRight
// would otherwise clip the last glyph.
static int16_t advanceWidth(const GFXfont &f, const char *s) {
    int16_t w = 0;
    for (; *s; ++s) {
        const uint8_t c = (uint8_t)*s;
        if (c < f.first || c > f.last) continue;
        w += (int16_t)pgm_read_byte(&f.glyph[c - f.first].xAdvance);
    }
    return w;
}

// Right-align by cursor-advance instead of bitmap bbox — correct for any
// GFX font including DSEG7 variants.
inline void drawTextRightAdv(IDisplay *d, const GFXfont &f,
                             int16_t rightX, int16_t y, const char *s) {
    const int16_t w = advanceWidth(f, s);
    d->drawText((int16_t)(rightX - w), y, s);
}

} // namespace

void TimeZoneCard::render(IDisplay *d, Rect slot,
                          const TimeZone &zone, const RenderCtx &ctx,
                          bool isMain, int dayDelta)
{
    // ---- 1. Resolve local time for this zone ----------------------------
    LocalDateTime lt = ctx.clock->toLocal(ctx.nowUtc, zone.posixTZ);
    const int16_t localMin = (int16_t)(lt.hour * 60 + lt.minute);

    // ---- 2. Pick ink polarity based on night/work schedule --------------
    // During NIGHT, swap: bg = Fg (black), fg = Bg (white).
    // During WORK HOURS, default: bg = Bg (white), fg = Fg (black).
    const bool night = isNight(localMin, zone.schedule);
    const Ink bg = night ? Ink::Fg : Ink::Bg;
    const Ink fg = night ? Ink::Bg : Ink::Fg;

    // ---- 3. Fill interior + draw 1-px outline ---------------------------
    d->fillRect(slot, bg);
    d->drawRect(slot, fg);

    // ---- 4. Day-bar along the top, inset 3 px each side -----------------
    // The day bar's OUTER BORDER follows the card's inversion (so the frame
    // sits naturally on either a white or black card). The INTERIOR segment
    // content (work / lunch / night stripes) always renders in light-mode
    // inks so the schedule reads consistently across cards. DayBar::render
    // handles that split internally — we pass the card's fg/bg through.
    const int16_t barH = isMain ? MAIN_BAR_H : ALT_BAR_H;
    const Rect barRect = {
        (int16_t)(slot.x + 3),
        (int16_t)(slot.y + 3),
        (int16_t)(slot.w - 6),
        barH,
    };
    DayBar::render(d, barRect, zone.schedule, localMin, fg, bg);

    // ---- 5. Flag bitmap, below the bar on the left ----------------------
    // TimeZone::drawFlag signature (per spec):
    //   void (*drawFlag)(IDisplay *, int16_t x, int16_t y, Ink fg, Ink bg)
    // Flag sits 6 px below the day-bar on both card sizes — the +Nd badge
    // now lives at the bottom-right of the alt card rather than under the
    // flag, so we no longer need to hoist the flag up to create room.
    const int16_t flagX = (int16_t)(slot.x + 5);
    const int16_t flagY = (int16_t)(slot.y + 3 + barH + 6);
    if (zone.drawFlag) {
        zone.drawFlag(d, flagX, flagY, fg, bg);
    }

    // Main card only: a vertical battery gauge with a CONTINUOUS fill
    // (not discrete segments) tucked under the flag.
    if (isMain) {
        // Map voltage to [0..1]. Usable LiPo range on Watchy is roughly
        // 3.3 V (low-battery cutoff-ish) to 4.2 V (full). The curve is
        // non-linear in reality but a linear mapping is good enough for
        // an at-a-glance indicator.
        constexpr float V_EMPTY = 3.30f;
        constexpr float V_FULL  = 4.20f;
        float fraction = (ctx.batteryVolts - V_EMPTY) / (V_FULL - V_EMPTY);
        if (fraction < 0.0f) fraction = 0.0f;
        if (fraction > 1.0f) fraction = 1.0f;

        // Compact 9 × 20 icon, centered horizontally below the flag.
        // Vertical placement splits the space between the flag bottom and
        // the card bottom roughly evenly (9-px gap above, ~8-px below)
        // so the icon sits visually centered in the column.
        constexpr int16_t BAT_W = 9;
        const int16_t batX = (int16_t)(flagX + (zone.flagW - BAT_W) / 2);
        const int16_t batY = (int16_t)(flagY + zone.flagH + 9);
        drawVerticalBattery(d, batX, batY, fraction, fg, bg);
    }

    // ---- 6. HH:MM clock -------------------------------------------------
    char clockBuf[6];
    snprintf(clockBuf, sizeof clockBuf, "%02d:%02d",
             (int)lt.hour, (int)lt.minute);

    d->setTextColour(fg);
    if (isMain) {
        // DSEG7_Regular_39 is too wide (165 px for HH:MM) to coexist with the
        // date stack on a 198-px card — it collides visibly on real hardware.
        // Bold_25 renders HH:MM in 90 px, leaving ~70 px for the date stack
        // and looking closer to the mockup's proportions.
        d->setFont((const Font *)&DSEG7_Classic_Bold_25);
        // Cursor x = 54 puts the colon's visible centre at x≈100 (screen
        // mid-line). DSEG7 Bold_25 glyph xAdvances: digits 21, colon 6, so
        // "HH" occupies cursor 54..95, then ":" at 96..101 with bitmap
        // x-offset 1 putting the visible 4-px colon at x 97..100.
        d->drawText((int16_t)(slot.x + 54),
                    (int16_t)(slot.y + 52),
                    clockBuf);
    } else {
        // Alt card is only 99 px wide. DSEG7_Bold_25 "HH:MM" is ~70 px so
        // starting at x=32 clips the last digit. Use DSEG7_Regular_15 (~40 px
        // wide for HH:MM) so the clock fits with flag on left + room for the
        // day-delta badge in the bottom-right.
        d->setFont((const Font *)&DSEG7_Classic_Regular_15);
        // Clock baseline: positioned to be centred between the day bar
        // (y=slot.y+11) and the bottom of the card (y=slot.y+49). Glyph is
        // ~15 px tall; baseline at slot.y+35 puts top at slot.y+20.
        d->drawText((int16_t)(slot.x + 30),
                    (int16_t)(slot.y + 35),
                    clockBuf);
    }

    // ---- 7. Date stack (main) or day-delta badge (alt) ------------------
    if (isMain) {
        // Right-aligned stack.
        //   TUE / APR : MyCompactFont7pt — in-between size between
        //               Picopixel (too small) and FreeSans9pt7b (too big).
        //   14        : DSEG7_Classic_Regular_15 — smaller than Bold_25,
        //               right-aligned by CURSOR-ADVANCE to avoid the
        //               DSEG7 per-glyph-xOffset clipping gotcha.
        const int16_t rightX = (int16_t)(slot.x + slot.w - 5);

        char ddBuf[4];
        snprintf(ddBuf, sizeof ddBuf, "%02d", (int)lt.day);

        const int wIdx = (lt.wday >= 0 && lt.wday <= 6) ? lt.wday : 0;
        const int mIdx = (lt.month >= 1 && lt.month <= 12)
                             ? (lt.month - 1) : 0;

        d->setFont((const Font *)&MyCompactFont7pt);
        // Baselines tuned so TUE sits just below the bar, APR sits just
        // above the card bottom, and DSEG7 "14" is sandwiched in between.
        drawTextRightAdv(d, MyCompactFont7pt, rightX,
                         (int16_t)(slot.y + 26), WDAY3[wIdx]);
        drawTextRightAdv(d, MyCompactFont7pt, rightX,
                         (int16_t)(slot.y + 64), MON3[mIdx]);

        d->setFont((const Font *)&DSEG7_Classic_Regular_15);
        drawTextRightAdv(d, DSEG7_Classic_Regular_15, rightX,
                         (int16_t)(slot.y + 47), ddBuf);
    } else if (dayDelta != 0) {
        // +Nd badge UNDER the flag, with its box width matching the flag's
        // width so it visually groups with the flag as a single column.
        // The "+" would look cramped in a narrow box, so the text is
        // centred horizontally inside a flag-wide pill.
        char buf[8];
        snprintf(buf, sizeof buf, "%+d", dayDelta);

        d->setFont((const Font *)&MyCompactFont7pt);
        const int16_t textW = advanceWidth(MyCompactFont7pt, buf);

        constexpr int16_t PAD_Y  = 2;
        constexpr int16_t TEXT_H = 7;
        const int16_t badgeW = zone.flagW;            // match flag width
        const int16_t badgeH = (int16_t)(TEXT_H + PAD_Y * 2);   // 11 px
        const int16_t badgeX = flagX;
        const int16_t badgeY = (int16_t)(flagY + zone.flagH + 2);  // 2-px gap

        d->drawRect({badgeX, badgeY, badgeW, badgeH}, fg);

        // Centre the text horizontally inside the badge.
        const int16_t tx = (int16_t)(badgeX + (badgeW - textW) / 2);
        // MyCompactFont7pt baseline sits at bottom of glyph (yOffset=-7).
        const int16_t ty = (int16_t)(badgeY + PAD_Y + TEXT_H);
        d->drawText(tx, ty, buf);
    }
}

} // namespace wmt
