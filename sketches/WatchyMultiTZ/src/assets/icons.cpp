#include "assets/icons.h"
#include "hal/IDisplay.h"

namespace wmt {

// ---------------------------------------------------------------------------
// 14x14 square badge with a centred check. Same footprint as the ZRH
// (square Swiss) flag so the two visually match. Corners are HARD (no
// chamfer) per design direction. The check is drawn as two diagonal
// segments with a 2-px stroke weight for good legibility.
// ---------------------------------------------------------------------------
void drawCheckmark(IDisplay *d, int16_t x, int16_t y, Ink fg, Ink bg) {
    constexpr int16_t S = 14;
    // Solid fg square (outer border + fill — square corners, not rounded).
    d->fillRect({x, y, S, S}, fg);

    // Check: short down-right leg, then longer up-right leg. Elbow at
    // (x+5, y+10). Each segment drawn twice (offset in x by +1) for
    // 2-px stroke thickness.
    //
    //   . . . . . . . . . . . . . .
    //   . . . . . . . . . . . x x .
    //   . . . . . . . . . . x x . .
    //   . . . . . . . . . x x . . .
    //   . . . . . . . . x x . . . .
    //   . . . . . . . x x . . . . .
    //   . . . . . . x x . . . . . .
    //   . . . . . x x . . . . . . .
    //   . . x . x x . . . . . . . .    elbow-ish area
    //   . . . x x . . . . . . . . .
    //   . . . . . . . . . . . . . .
    //   ...
    //
    // Values below approximate this shape.
    const int16_t elbowX = (int16_t)(x + 4);
    const int16_t elbowY = (int16_t)(y + 9);

    // Left leg: from (x+2, y+7) down-right to elbow.
    d->drawLine((int16_t)(x + 2), (int16_t)(y + 7),
                elbowX,           elbowY, bg);
    d->drawLine((int16_t)(x + 3), (int16_t)(y + 7),
                (int16_t)(elbowX + 1), elbowY, bg);

    // Right leg: from elbow up-right to (x+11, y+3).
    d->drawLine(elbowX, elbowY,
                (int16_t)(x + 11), (int16_t)(y + 2), bg);
    d->drawLine((int16_t)(elbowX + 1), elbowY,
                (int16_t)(x + 12), (int16_t)(y + 2), bg);
}

// ---------------------------------------------------------------------------
// 16x6 battery body + 2x3 nipple cap, with up to three 4x4 fill segments.
// Layout (coordinates relative to x,y):
//   body   : (0..15,  0..5)   — 1-px fg outline
//   cap    : (16..17, 1..3)   — solid fg
//   bars[0]: (1..4,   1..4)
//   bars[1]: (6..9,   1..4)
//   bars[2]: (11..14, 1..4)
// ---------------------------------------------------------------------------
void drawBatteryIcon(IDisplay *d, int16_t x, int16_t y, int bars, Ink fg, Ink bg) {
    // Body outline.
    d->drawRect({x, y, 16, 6}, fg);

    // Cap (positive terminal).
    d->fillRect({(int16_t)(x + 16), (int16_t)(y + 1), 2, 3}, fg);

    // Clamp bar count to [0,3].
    if (bars < 0) bars = 0;
    if (bars > 3) bars = 3;

    const int16_t barX[3] = {
        (int16_t)(x + 1),
        (int16_t)(x + 6),
        (int16_t)(x + 11),
    };
    for (int i = 0; i < bars; ++i) {
        d->fillRect({barX[i], (int16_t)(y + 1), 4, 4}, fg);
    }

    // Silence unused-parameter warning if bg is ever introduced.
    (void)bg;
}

// ---------------------------------------------------------------------------
// Vertical battery. 9 wide × 20 tall footprint. Fills the body's interior
// proportionally from the bottom up — reads as a continuous meter rather
// than discrete segments.
//
// Layout relative to (x, y) = top-left of the whole icon:
//   cap   : (x+2, y)           — 5x2 fg rectangle (cathode nub)
//   body  : (x,   y+2, 9, 18)  — 1-px fg outline
//   fill  : (x+1, y+3, 7, 16)  — inner drawable region; filled from the
//           bottom up by round(fraction * 16) pixels.
// ---------------------------------------------------------------------------
void drawVerticalBattery(IDisplay *d, int16_t x, int16_t y, float fraction,
                         Ink fg, Ink bg) {
    // Cap + body outline.
    d->fillRect({(int16_t)(x + 2), y, 5, 2}, fg);
    d->drawRect({x, (int16_t)(y + 2), 9, 18}, fg);

    // Clamp fraction.
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;

    // Inner drawable area = 7 wide × 16 tall at (x+1, y+3).
    constexpr int16_t INNER_W = 7;
    constexpr int16_t INNER_H = 16;

    int16_t fillH = (int16_t)(fraction * (float)INNER_H + 0.5f);
    if (fillH > INNER_H) fillH = INNER_H;
    if (fillH > 0) {
        const int16_t fillY = (int16_t)(y + 3 + (INNER_H - fillH));
        d->fillRect({(int16_t)(x + 1), fillY, INNER_W, fillH}, fg);
    }

    (void)bg;
}

// ---------------------------------------------------------------------------
// 10x10 WiFi-on glyph. Three stacked horizontal "arcs" of increasing width
// plus a single-pixel dot at the bottom. Not a true arc, but it reads as
// a radio-wave stack at this size.
// ---------------------------------------------------------------------------
void drawWifiIcon(IDisplay *d, int16_t x, int16_t y, Ink fg, Ink bg) {
    // Top arc (widest).
    d->drawHLine((int16_t)(x + 2), (int16_t)(y + 2), 6, fg);
    // Middle arc.
    d->drawHLine((int16_t)(x + 3), (int16_t)(y + 5), 4, fg);
    // Bottom arc (shortest).
    d->drawHLine((int16_t)(x + 4), (int16_t)(y + 8), 2, fg);
    // Dot under the arcs.
    d->drawPixel((int16_t)(x + 4), (int16_t)(y + 9), fg);

    (void)bg;
}

} // namespace wmt
