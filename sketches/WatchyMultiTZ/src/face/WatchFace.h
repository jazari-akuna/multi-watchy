#pragma once
// Platform-agnostic orchestrator for the multi-timezone watchface.
//
// Owns no hardware. All device-specific behaviour is delegated through the
// HAL interface pointers in WatchFaceDeps. A platform shim (e.g.
// sketches/WatchyMultiTZ/src/platform/watchy/*) creates the concrete HAL
// impls and the `WatchFace` instance, forwards wake events via onWake(),
// and everything in this translation unit is reusable on any other 1-bit
// display hardware that implements the same HAL.
//
// Card renderers (TimeZoneCard, EventCard) live in `face/cards/`.

#include "../hal/IDisplay.h"
#include "../hal/IClock.h"
#include "../hal/IButtons.h"
#include "../hal/IPower.h"
#include "../hal/INetwork.h"
#include "../hal/IEventProvider.h"
#include "../hal/IThermometer.h"
#include "TimeZone.h"

namespace wmt {

class DriftTracker;
class DriftStatsScreen;

// Transient badge state shown next to the main clock during a sync cycle.
// None is the resting state (no badge). The other three map to the icon
// helpers in assets/icons.h (sync = "..."; check = ok; cross = fail).
enum class SyncStatus : uint8_t {
    None       = 0,
    InProgress = 1,
    Success    = 2,
    Failure    = 3,
};

// Shared render context passed to the individual card renderers so they
// don't each have to rummage around in WatchFaceDeps.
struct RenderCtx {
    IDisplay       *display;
    IClock         *clock;
    IEventProvider *events;
    int64_t         nowUtc;
    const char     *mainTZ;          // POSIX TZ of the currently-main zone
                                     // (used to format event times in the
                                     // user's primary wall-clock)
    const Schedule *mainSchedule;    // work/lunch ranges of the main zone
                                     // (used by EventBar to shade the 8-hour
                                     // window by work/off status)
    float           batteryVolts;    // current battery voltage (0 if unknown)
    int             eventCycleIdx = 0;  // offset into the upcoming-events list
                                        // that the event card should display
                                        // (advanced by short DOWN presses,
                                        // reset to 0 on full-refresh settle).
    SyncStatus      syncStatus = SyncStatus::None;  // main card only; drawn
                                                    // just left of the clock
                                                    // digits.
};

struct WatchFaceDeps {
    IDisplay       *display;
    IClock         *clock;
    IButtons       *buttons;
    IPower         *power;
    INetwork       *network;
    IEventProvider *events;
    DriftTracker   *drift  = nullptr;
    IThermometer   *thermo = nullptr;
    const TimeZone *zones;     // caller-owned array
    int             numZones;
};

class WatchFace {
public:
    // `mainIdxRef` is a reference to an RTC_DATA_ATTR int owned by the
    // platform shim — this class mutates it when UP cycles zones. Keeping
    // the storage outside the class lets the platform decide how the
    // counter survives deep sleep (RTC memory, flash, nothing).
    WatchFace(const WatchFaceDeps &deps, int &mainIdxRef);

    // Called by the platform glue after each wake. Inspects the HAL to
    // decide whether this was a minute-tick, a button press, or a forced
    // sync request and acts accordingly.
    void onWake();

    // Render the full face (4 cards) into the display and commit.
    void render(bool partialRefresh);

    // BLE-first + WiFi-NTP-fallback sync. Called by the double-press handler.
    void syncAll();

    // Run one BLE sync cycle with in-face visual feedback:
    //   1. Render with a sync-in-progress badge next to the main clock
    //   2. Block on IEventProvider::syncNow(timeoutMs, abortOn)
    //   3. Render with a check (ok) or cross (fail) badge
    //   4. Hold that status for ~2 s so the user can read it
    //   5. Render one clean frame (badge cleared) and return
    // No vibration is fired as part of this flow — all feedback is visual,
    // per the product direction ("just add a sync icon… for 2 seconds").
    // Used by syncAll() (long-press) and the .ino's periodic silent sync.
    void runSync(uint32_t timeoutMs, IButtons *abortOn);

    // Open the drift-stats overlay, poll buttons, return after BACK /
    // library-menu passthrough / 10 s idle. Caller must then repaint
    // the watchface (or let the library handle it if we exited to menu).
    void openDriftStats();

    // Open the QR-cycle overlay (pre-baked codes from assets/qr_codes.h).
    // Each BACK press advances; pressing past the last code or 30 s of
    // idle returns to the watchface with a full refresh.
    void openQrCodes();

    // Sim/debug hook: force a badge state without running an actual sync.
    // The firmware path should drive status transitions through runSync();
    // this setter exists so the simulator can render each state for visual
    // review without BLE plumbing.
    void       setSyncStatus(SyncStatus s) { syncStatus_ = s; }
    SyncStatus syncStatus() const          { return syncStatus_; }

private:
    void renderMainCard();
    void renderAltCard(Rect slot, int tzIndex);
    void renderEventCard();

    // Poll buttons for 10 s after a button-driven partial refresh.
    // Handles rapid UP cycling inline; breaks early on other presses.
    // On 10 s of quiescence, commits one full refresh to clear ghosting.
    void settleThenFullRefresh();

    // Called on each minute-tick wake. Buzzes the haptic motor for:
    //   - top-of-the-hour (minute-of-hour == 0): 2 short pulses
    //   - 1 h before any upcoming event start:  1 short pulse
    //   - 5 min before any upcoming event start: 1 short pulse
    void maybeBuzzReminders();

    WatchFaceDeps d_;
    int          &mainIdx_;   // index into d_.zones
    int           eventCycleIdx_ = 0;   // event-card browsing offset
    SyncStatus    syncStatus_   = SyncStatus::None;
};

} // namespace wmt
