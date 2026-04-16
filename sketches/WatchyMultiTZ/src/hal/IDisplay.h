#pragma once
// 1-bit framebuffer display interface.
//
// The `Font` type is opaque by design — platforms plug in whatever glyph
// format they already support. On Watchy this is `const GFXfont*` from
// Adafruit_GFX. On the desktop simulator it's also a `GFXfont` (the sim
// #includes the same font headers and implements its own tiny walker).
//
// All coordinates are integer pixels; (0,0) is top-left.
// drawText positions the text BASELINE at (x, y) — same convention as
// Adafruit_GFX's setCursor + print.

#include "Types.h"

namespace wmt {

struct Font;  // opaque forward-decl — platforms alias it to their native type

class IDisplay {
public:
    virtual ~IDisplay() = default;

    virtual int16_t width()  const = 0;
    virtual int16_t height() const = 0;

    virtual void clear(Ink bg) = 0;

    virtual void drawPixel(int16_t x, int16_t y, Ink c) = 0;
    virtual void fillRect(Rect r, Ink c) = 0;
    virtual void drawRect(Rect r, Ink c) = 0;  // 1-px outline, no fill
    virtual void drawHLine(int16_t x, int16_t y, int16_t w, Ink c) = 0;
    virtual void drawVLine(int16_t x, int16_t y, int16_t h, Ink c) = 0;
    virtual void drawLine(int16_t x0, int16_t y0,
                          int16_t x1, int16_t y1, Ink c) = 0;

    // 1-bit packed bitmap — same layout as Adafruit_GFX `drawBitmap`.
    virtual void drawBitmap(int16_t x, int16_t y,
                            const uint8_t *bm,
                            int16_t w, int16_t h, Ink c) = 0;

    // Text
    virtual void setFont(const Font *f) = 0;
    virtual void setTextColour(Ink c) = 0;
    virtual void drawText(int16_t x, int16_t y, const char *s) = 0;
    virtual void measureText(const char *s,
                             int16_t &outW, int16_t &outH) = 0;

    // Commit the framebuffer to the panel. On Watchy this is partial vs full
    // e-ink refresh; on the simulator this writes the PNG to disk.
    virtual void commit(bool partialRefresh) = 0;

    // Optional: switch the underlying e-ink driver back to full-window mode
    // before the next commit(false). On GxEPD2 this clears the driver's
    // _using_partial_mode flag so writeImageForFullRefresh() starts from a
    // clean LUT state — harmless but occasionally load-bearing after a
    // run of partial refreshes. Default no-op on hardware that doesn't need
    // it (e.g. the simulator's raw framebuffer).
    virtual void setFullWindow() {}
};

} // namespace wmt
