#include "WatchyDisplay.h"

// Pull in the full Watchy / GxEPD2 stack only in the .cpp so the header stays
// light and does not force every consumer to see the Arduino world.
#include <Watchy.h>
#include <Fonts/FreeMonoBold9pt7b.h> // include side-effect: GFXfont definition

namespace wmt {

// Logical -> physical ink. Light mode: foreground = black, background = white.
static inline uint16_t ink(Ink i) {
    return i == Ink::Fg ? GxEPD_BLACK : GxEPD_WHITE;
}

void WatchyDisplayHal::clear(Ink bg) {
    d_.fillScreen(ink(bg));
}

void WatchyDisplayHal::drawPixel(int16_t x, int16_t y, Ink c) {
    d_.drawPixel(x, y, ink(c));
}

void WatchyDisplayHal::fillRect(Rect r, Ink c) {
    d_.fillRect(r.x, r.y, r.w, r.h, ink(c));
}

void WatchyDisplayHal::drawRect(Rect r, Ink c) {
    d_.drawRect(r.x, r.y, r.w, r.h, ink(c));
}

void WatchyDisplayHal::drawHLine(int16_t x, int16_t y, int16_t w, Ink c) {
    d_.drawFastHLine(x, y, w, ink(c));
}

void WatchyDisplayHal::drawVLine(int16_t x, int16_t y, int16_t h, Ink c) {
    d_.drawFastVLine(x, y, h, ink(c));
}

void WatchyDisplayHal::drawLine(int16_t x0, int16_t y0,
                                int16_t x1, int16_t y1, Ink c) {
    d_.drawLine(x0, y0, x1, y1, ink(c));
}

void WatchyDisplayHal::drawBitmap(int16_t x, int16_t y,
                                  const uint8_t *bm,
                                  int16_t w, int16_t h, Ink c) {
    d_.drawBitmap(x, y, bm, w, h, ink(c));
}

void WatchyDisplayHal::setFont(const Font *f) {
    d_.setFont(reinterpret_cast<const GFXfont *>(f));
}

void WatchyDisplayHal::setTextColour(Ink c) {
    d_.setTextColor(ink(c));
}

void WatchyDisplayHal::drawText(int16_t x, int16_t y, const char *s) {
    d_.setCursor(x, y);
    d_.print(s);
}

void WatchyDisplayHal::measureText(const char *s, int16_t &outW, int16_t &outH) {
    int16_t x1 = 0, y1 = 0;
    uint16_t w = 0, h = 0;
    d_.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
    outW = static_cast<int16_t>(w);
    outH = static_cast<int16_t>(h);
}

void WatchyDisplayHal::commit(bool partialRefresh) {
    d_.display(partialRefresh);
}

void WatchyDisplayHal::setFullWindow() {
    d_.setFullWindow();
}

} // namespace wmt
