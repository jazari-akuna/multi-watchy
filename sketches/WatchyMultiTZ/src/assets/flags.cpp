#include "assets/flags.h"
#include "hal/IDisplay.h"

namespace wmt {

// ---------------------------------------------------------------------------
// SZX — People's Republic of China.
// 20x14 red field (fg), one 4x4 large star (bg) near the upper-left, and
// four single-pixel small stars (bg) scattered around it.
// ---------------------------------------------------------------------------
void drawFlagSzx(IDisplay *d, int16_t x, int16_t y, Ink fg, Ink bg) {
    // 1-px outline + filled red field.
    d->drawRect({x, y, 20, 14}, fg);
    d->fillRect({(int16_t)(x + 1), (int16_t)(y + 1), 18, 12}, fg);

    // Large star: 4x4 bg square in the upper-left area.
    d->fillRect({(int16_t)(x + 2), (int16_t)(y + 2), 4, 4}, bg);

    // Four small stars as single bg pixels.
    d->drawPixel((int16_t)(x + 8),  (int16_t)(y + 1), bg);
    d->drawPixel((int16_t)(x + 10), (int16_t)(y + 3), bg);
    d->drawPixel((int16_t)(x + 11), (int16_t)(y + 6), bg);
    d->drawPixel((int16_t)(x + 9),  (int16_t)(y + 8), bg);
}

// ---------------------------------------------------------------------------
// SFO — USA.
// 20x14. Seven 2-px stripes alternating fg/bg/fg/... to evoke the 13
// stripes. The canton is an 8x6 solid fg block in the top-left corner —
// stars would not render legibly at this size.
// ---------------------------------------------------------------------------
void drawFlagSfo(IDisplay *d, int16_t x, int16_t y, Ink fg, Ink bg) {
    // Outline + base field. Start with bg so the bg stripes don't need
    // explicit drawing; we paint only the fg stripes below.
    d->drawRect({x, y, 20, 14}, fg);
    d->fillRect({(int16_t)(x + 1), (int16_t)(y + 1), 18, 12}, bg);

    // 7 stripes, 2px each: fg, bg, fg, bg, fg, bg, fg.
    // (We only need to draw the fg ones since the interior is already bg.)
    d->fillRect({x, (int16_t)(y + 0),  20, 2}, fg);
    d->fillRect({x, (int16_t)(y + 4),  20, 2}, fg);
    d->fillRect({x, (int16_t)(y + 8),  20, 2}, fg);
    d->fillRect({x, (int16_t)(y + 12), 20, 2}, fg);

    // Canton: 8x6 solid fg block in the top-left (x..x+7, y..y+5).
    d->fillRect({x, y, 8, 6}, fg);
}

// ---------------------------------------------------------------------------
// ZRH — Switzerland.
// 14x14 red field (fg) with a centred white (bg) "+" cross. The real Swiss
// flag is square (1:1), so we draw a square icon here rather than the 20x14
// rectangle other zones use.
// ---------------------------------------------------------------------------
void drawFlagZrh(IDisplay *d, int16_t x, int16_t y, Ink fg, Ink bg) {
    d->drawRect({x, y, 14, 14}, fg);
    d->fillRect({(int16_t)(x + 1), (int16_t)(y + 1), 12, 12}, fg);

    // Vertical arm: 2 wide x 8 tall, columns x+6..x+7, rows y+3..y+10.
    d->fillRect({(int16_t)(x + 6), (int16_t)(y + 3), 2, 8}, bg);
    // Horizontal arm: 8 wide x 2 tall, columns x+3..x+10, rows y+6..y+7.
    d->fillRect({(int16_t)(x + 3), (int16_t)(y + 6), 8, 2}, bg);
}

} // namespace wmt
