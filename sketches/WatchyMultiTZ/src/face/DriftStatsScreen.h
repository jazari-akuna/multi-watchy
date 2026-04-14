#pragma once
// DriftStatsScreen — two-page MENU overlay showing what the DriftTracker
// has learned. Platform-agnostic; uses only the IDisplay HAL + the usual
// HAL interfaces for buttons / power / clock / thermometer.
//
// Page 1 (Overview): textual readouts of learned FC, sample count, time
//                    since last sync, current temperature, turnover T,
//                    profile, estimated residual drift per hour.
// Page 2 (Graph):    up to 24 most-recent instantaneous-ppm samples as
//                    1-px bars from a zero axis, with the running EMA
//                    overlaid as a 1-px polyline. Y auto-scales to the
//                    window's ±max (minimum ±5 ppm so early noise doesn't
//                    saturate the axis).
//
// Control flow:
//   auto r = screen.run();
//   switch (r) {
//     case ExitReason::Back:           caller re-renders the watchface
//     case ExitReason::ToLibraryMenu:  caller hands control to library menu
//     case ExitReason::IdleTimeout:    caller re-renders (full refresh)
//   }

#include "../hal/IDisplay.h"
#include "../hal/IButtons.h"
#include "../hal/IPower.h"
#include "../hal/IClock.h"
#include "../hal/IThermometer.h"

namespace wmt {

class DriftTracker;

class DriftStatsScreen {
public:
    enum class ExitReason : uint8_t {
        Back,            // user pressed BACK
        ToLibraryMenu,   // user pressed MENU — caller opens library menu
        IdleTimeout,     // 10 s of no input
    };

    DriftStatsScreen(IDisplay *display,
                     IButtons *buttons,
                     IPower   *power,
                     IClock   *clock,
                     IThermometer *thermo,
                     const DriftTracker &tracker);

    // Blocking loop. Renders once, then polls buttons. Returns on BACK /
    // MENU / 10 s idle. Caller is responsible for the post-exit repaint.
    ExitReason run();

    // Single-page render, no poll loop. Used by the desktop simulator.
    //   page:  0 = overview, 1 = graph
    //   nowUtc: clock value to annotate "last sync  HH:MM ago" with
    // Leaves the framebuffer populated but does NOT commit — caller does.
    static void renderPage(IDisplay *display,
                           IClock   *clock,
                           IThermometer *thermo,
                           const DriftTracker &tracker,
                           int page,
                           int64_t nowUtc);

private:
    IDisplay     *d_;
    IButtons     *b_;
    IPower       *p_;
    IClock       *c_;
    IThermometer *t_;
    const DriftTracker &dt_;

    int page_ = 0;    // 0 = overview, 1 = graph
    static constexpr int NUM_PAGES = 2;

    void repaint();
};

} // namespace wmt
