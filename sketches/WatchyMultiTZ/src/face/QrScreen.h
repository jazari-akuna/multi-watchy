#pragma once
// Full-screen overlay that shows one pre-baked QR code at a time and
// cycles through the set on each BACK press. 30 s of inactivity or
// pressing past the last code returns to the watchface.
//
// The actual QR matrices live in `assets/qr_codes.h`, generated from
// source images in tools/qr_sources/ by tools/gen_qr_codes.py. This
// class owns only the rendering and the polling loop.

#include "../hal/IDisplay.h"
#include "../hal/IButtons.h"
#include "../hal/IPower.h"

namespace wmt {

class QrScreen {
public:
    enum class ExitReason : uint8_t {
        CycledPastEnd,   // user pressed BACK past the last code
        IdleTimeout,     // 30 s of no input
    };

    QrScreen(IDisplay *display, IButtons *buttons, IPower *power)
        : d_(display), b_(buttons), p_(power) {}

    // Blocking loop. Renders code 0 (partial refresh) and polls BACK
    // until the cycle finishes or the idle timer expires. Caller is
    // responsible for the full-refresh back to the watchface on exit.
    ExitReason run();

    // Sim-facing: render a single QR (by table index) into the display's
    // framebuffer without committing. Used by `sim/main.cpp`.
    static void renderOne(IDisplay *d, int qrIndex);

private:
    IDisplay *d_;
    IButtons *b_;
    IPower   *p_;
    int       index_ = 0;
    static constexpr uint32_t IDLE_MS = 30000;

    void repaint();
};

} // namespace wmt
