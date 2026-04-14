// SimDisplay — desktop-simulator IDisplay.
//
// 1-bit framebuffer packed MSB-first, row-major. On commit() we expand to
// an 8-bit grayscale buffer (light mode: Fg=black, Bg=white) and emit a
// PNG via stb_image_write.
//
// Drawing primitives are deliberately straightforward and allocation-free —
// this runs on the host but mirrors the real Watchy path where we want
// predictable behaviour and no surprises.

#include "SimDisplay.h"

// stb_image_write is header-only; exactly one TU in the sim must define
// STB_IMAGE_WRITE_IMPLEMENTATION. We centralise that here. If another TU
// also needs stb, it should include the header WITHOUT the impl macro.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <stdint.h>
#include <string.h>

namespace wmt {

// ---------------------------------------------------------------------------
// Ctor / framebuffer helpers
// ---------------------------------------------------------------------------

SimDisplay::SimDisplay(const char *outPath)
    : outPath_(outPath) {
    // Initial fill = background (white in light mode).
    memset(fb_, 0xFF, FB_BYTES);
}

inline void SimDisplay::setBit(int16_t x, int16_t y, Ink c) {
    if (x < 0 || x >= W || y < 0 || y >= H) return;
    const int idx = y * W + x;
    const int byte = idx >> 3;
    const uint8_t mask = (uint8_t)(0x80u >> (idx & 7));
    if (c == Ink::Fg) {
        fb_[byte] &= (uint8_t)~mask;   // 0 = Fg
    } else {
        fb_[byte] |= mask;             // 1 = Bg
    }
}

// ---------------------------------------------------------------------------
// Primitives
// ---------------------------------------------------------------------------

void SimDisplay::clear(Ink bg) {
    memset(fb_, bg == Ink::Fg ? 0x00 : 0xFF, FB_BYTES);
}

void SimDisplay::drawPixel(int16_t x, int16_t y, Ink c) {
    setBit(x, y, c);
}

void SimDisplay::fillRect(Rect r, Ink c) {
    const int16_t x0 = r.x < 0 ? 0 : r.x;
    const int16_t y0 = r.y < 0 ? 0 : r.y;
    const int16_t x1 = (r.x + r.w) > W ? W : (int16_t)(r.x + r.w);
    const int16_t y1 = (r.y + r.h) > H ? H : (int16_t)(r.y + r.h);
    for (int16_t y = y0; y < y1; ++y) {
        for (int16_t x = x0; x < x1; ++x) {
            setBit(x, y, c);
        }
    }
}

void SimDisplay::drawRect(Rect r, Ink c) {
    if (r.w <= 0 || r.h <= 0) return;
    drawHLine(r.x, r.y, r.w, c);
    drawHLine(r.x, (int16_t)(r.y + r.h - 1), r.w, c);
    drawVLine(r.x, r.y, r.h, c);
    drawVLine((int16_t)(r.x + r.w - 1), r.y, r.h, c);
}

void SimDisplay::drawHLine(int16_t x, int16_t y, int16_t w, Ink c) {
    for (int16_t i = 0; i < w; ++i) setBit((int16_t)(x + i), y, c);
}

void SimDisplay::drawVLine(int16_t x, int16_t y, int16_t h, Ink c) {
    for (int16_t i = 0; i < h; ++i) setBit(x, (int16_t)(y + i), c);
}

void SimDisplay::drawLine(int16_t x0, int16_t y0,
                          int16_t x1, int16_t y1, Ink c) {
    // Classic integer Bresenham.
    int16_t dx = (int16_t)(x1 - x0); if (dx < 0) dx = (int16_t)-dx;
    int16_t dy = (int16_t)(y1 - y0); if (dy < 0) dy = (int16_t)-dy;
    int16_t sx = x0 < x1 ? 1 : -1;
    int16_t sy = y0 < y1 ? 1 : -1;
    int16_t err = (int16_t)(dx - dy);
    for (;;) {
        setBit(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = (int16_t)(err * 2);
        if (e2 > -dy) { err = (int16_t)(err - dy); x0 = (int16_t)(x0 + sx); }
        if (e2 <  dx) { err = (int16_t)(err + dx); y0 = (int16_t)(y0 + sy); }
    }
}

// Adafruit_GFX drawBitmap: 1 bit per pixel, MSB-first, rows padded to
// byte boundary. Only "on" bits are drawn in colour `c` (background is left
// untouched). This matches the real device behaviour.
void SimDisplay::drawBitmap(int16_t x, int16_t y,
                            const uint8_t *bm,
                            int16_t w, int16_t h, Ink c) {
    if (!bm || w <= 0 || h <= 0) return;
    const int16_t bytesPerRow = (int16_t)((w + 7) >> 3);
    for (int16_t row = 0; row < h; ++row) {
        const uint8_t *rowP = bm + row * bytesPerRow;
        for (int16_t col = 0; col < w; ++col) {
            const uint8_t b = pgm_read_byte(rowP + (col >> 3));
            if (b & (0x80u >> (col & 7))) {
                setBit((int16_t)(x + col), (int16_t)(y + row), c);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Text
// ---------------------------------------------------------------------------

void SimDisplay::setFont(const Font *f) {
    font_ = reinterpret_cast<const GFXfont *>(f);
}

void SimDisplay::setTextColour(Ink c) {
    textInk_ = c;
}

// Internal helper: draw one glyph with baseline at (cx, cy).
// Returns the glyph's xAdvance.
static int drawGlyph(SimDisplay &d, const GFXfont *font,
                     int16_t cx, int16_t cy, uint8_t ch, Ink ink) {
    if (!font) return 0;
    if (ch < font->first || ch > font->last) return 0;
    const GFXglyph *g = &font->glyph[ch - font->first];
    const uint8_t gw   = g->width;
    const uint8_t gh   = g->height;
    const uint8_t adv  = g->xAdvance;
    const int8_t  xoff = g->xOffset;
    const int8_t  yoff = g->yOffset;

    if (gw == 0 || gh == 0) return adv;  // e.g. space

    const uint8_t *bits = font->bitmap + g->bitmapOffset;
    uint16_t bitIdx = 0;
    for (uint8_t row = 0; row < gh; ++row) {
        for (uint8_t col = 0; col < gw; ++col) {
            const uint8_t byte = pgm_read_byte(bits + (bitIdx >> 3));
            if (byte & (0x80u >> (bitIdx & 7))) {
                d.drawPixel((int16_t)(cx + xoff + col),
                            (int16_t)(cy + yoff + row),
                            ink);
            }
            ++bitIdx;
        }
    }
    return adv;
}

void SimDisplay::drawText(int16_t x, int16_t y, const char *s) {
    if (!s || !font_) return;
    int16_t cx = x;
    for (const char *p = s; *p; ++p) {
        const uint8_t ch = (uint8_t)*p;
        if (ch == '\n' || ch == '\r') continue;
        cx = (int16_t)(cx + drawGlyph(*this, font_, cx, y, ch, textInk_));
    }
}

void SimDisplay::measureText(const char *s, int16_t &outW, int16_t &outH) {
    outW = 0;
    outH = 0;
    if (!s || !font_) return;
    int16_t totalAdv = 0;
    int16_t tallest  = 0;
    for (const char *p = s; *p; ++p) {
        const uint8_t ch = (uint8_t)*p;
        if (ch < font_->first || ch > font_->last) continue;
        const GFXglyph *g = &font_->glyph[ch - font_->first];
        totalAdv = (int16_t)(totalAdv + g->xAdvance);
        if (g->height > tallest) tallest = g->height;
    }
    outW = totalAdv;
    outH = tallest;
}

// ---------------------------------------------------------------------------
// PNG commit
// ---------------------------------------------------------------------------

void SimDisplay::commit(bool /*partialRefresh*/) {
    if (!outPath_) return;

    // Expand 1-bit → 8-bit grayscale. Bit 0 (Fg) → 0x00 black.
    // Bit 1 (Bg) → 0xFF white. Light-mode rendering.
    static uint8_t gray[W * H];
    for (int i = 0; i < W * H; ++i) {
        const uint8_t b = fb_[i >> 3];
        const uint8_t mask = (uint8_t)(0x80u >> (i & 7));
        gray[i] = (b & mask) ? 0xFF : 0x00;
    }
    stbi_write_png(outPath_, W, H, /*comp=*/1, gray, /*stride=*/W);
}

} // namespace wmt
