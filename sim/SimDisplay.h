#pragma once
// Desktop-simulator implementation of wmt::IDisplay.
//
// Renders into a 200x200 in-memory 1-bit framebuffer and writes the result
// to disk as a PNG on commit(). Font handling reuses the Adafruit-GFX
// `GFXfont` layout (see ArduinoShim.h) so the same font headers used on
// Watchy are binary-compatible here.
//
// The HAL's opaque `wmt::Font` pointer is reinterpret_cast'd to `GFXfont*`
// inside SimDisplay.cpp.

#include "../sketches/WatchyMultiTZ/src/hal/IDisplay.h"
#include "ArduinoShim.h"

#include <stdint.h>

namespace wmt {

class SimDisplay : public IDisplay {
public:
    static constexpr int16_t W = 200;
    static constexpr int16_t H = 200;
    static constexpr int    FB_BYTES = (W * H + 7) / 8;

    // `outPath` is the absolute/relative path the PNG is written to on
    // every commit(). Does not take ownership — the caller keeps the string
    // alive for the lifetime of SimDisplay.
    explicit SimDisplay(const char *outPath);
    ~SimDisplay() override = default;

    int16_t width()  const override { return W; }
    int16_t height() const override { return H; }

    void clear(Ink bg) override;

    void drawPixel(int16_t x, int16_t y, Ink c) override;
    void fillRect(Rect r, Ink c) override;
    void drawRect(Rect r, Ink c) override;
    void drawHLine(int16_t x, int16_t y, int16_t w, Ink c) override;
    void drawVLine(int16_t x, int16_t y, int16_t h, Ink c) override;
    void drawLine(int16_t x0, int16_t y0,
                  int16_t x1, int16_t y1, Ink c) override;

    void drawBitmap(int16_t x, int16_t y,
                    const uint8_t *bm,
                    int16_t w, int16_t h, Ink c) override;

    void setFont(const Font *f) override;
    void setTextColour(Ink c) override;
    void drawText(int16_t x, int16_t y, const char *s) override;
    void measureText(const char *s, int16_t &outW, int16_t &outH) override;

    void commit(bool partialRefresh) override;

    // Sim-only: change the output path at runtime (e.g. between frames).
    void setOutputPath(const char *p) { outPath_ = p; }

private:
    // Framebuffer: 1 bit per pixel, row-major, MSB-first inside each byte.
    // Bit value 0 → Ink::Fg (black in light mode). Bit value 1 → Ink::Bg.
    uint8_t fb_[FB_BYTES];

    const GFXfont *font_    = nullptr;
    Ink            textInk_ = Ink::Fg;
    const char    *outPath_ = nullptr;

    inline void setBit(int16_t x, int16_t y, Ink c);
};

} // namespace wmt
