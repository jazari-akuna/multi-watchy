#pragma once
// Watchy-specific IDisplay implementation. Delegates every call to the
// GxEPD2_BW driver held by the Watchy library; does not own the driver.

#include "../../hal/IDisplay.h"

// Forward-declare the GxEPD2 driver classes so this header does not have
// to include <Watchy.h> / <GxEPD2_BW.h>.
class WatchyDisplay;                    // panel driver (from Watchy lib)
template <typename GxEPD2_Type, const uint16_t page_height>
class GxEPD2_BW;

namespace wmt {

// Concrete IDisplay that forwards to the Watchy library's GxEPD2_BW driver.
// Ink::Fg -> GxEPD_BLACK, Ink::Bg -> GxEPD_WHITE (light-mode default).
class WatchyDisplayHal final : public IDisplay {
public:
    // HEIGHT template parameter mirrors Watchy's `GxEPD2_BW<WatchyDisplay, WatchyDisplay::HEIGHT>`.
    // 200 is the physical panel height; we keep it as a compile-time literal
    // so the header stays independent of GxEPD2 internals.
    using DriverT = GxEPD2_BW<WatchyDisplay, 200>;

    explicit WatchyDisplayHal(DriverT &driver) : d_(driver) {}

    int16_t width()  const override { return 200; }
    int16_t height() const override { return 200; }

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

    // Override the IDisplay hook — see IDisplay::setFullWindow for rationale.
    // Delegates to GxEPD2_BW::setFullWindow(), which resets _using_partial_mode
    // and the _pw_* rect to the full panel.
    void setFullWindow() override;

private:
    DriverT &d_;
};

} // namespace wmt
