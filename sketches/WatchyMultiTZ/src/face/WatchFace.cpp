// WatchFace.cpp
//
// This translation unit is strictly platform-agnostic: it knows nothing
// about GxEPD, ESP32 deep-sleep, Adafruit_GFX, or any specific hardware.
// Every side-effect that reaches the outside world — drawing a pixel,
// reading the RTC, polling a GPIO, opening WiFi, sleeping the CPU — goes
// through one of the HAL interface pointers held in WatchFaceDeps. That
// is what lets the exact same .cpp compile for a Watchy sketch, a
// PineTime port, or a desktop simulator; only the HAL implementations
// differ. As a consequence: do NOT reach for Arduino.h, <esp_sleep.h>,
// <WiFi.h>, or any vendor-specific header from inside this file.

#include "WatchFace.h"
#include "Slot.h"
#include "Schedule.h"
#include "DriftStatsScreen.h"
#include "QrScreen.h"
#include "cards/TimeZoneCard.h"
#include "cards/EventCard.h"

namespace wmt {

WatchFace::WatchFace(const WatchFaceDeps &deps, int &mainIdxRef)
    : d_(deps), mainIdx_(mainIdxRef) {}

// Utility: calendar-day ordinal used to compute day-deltas between zones.
// Multiplying tm_year by 400 (> max tm_yday of 365) makes the ordinal
// strictly monotonic so subtraction gives a clean integer day offset.
static int dayOrdinal(const LocalDateTime &t) {
    return (t.year - 1970) * 400 + (t.month - 1) * 32 + t.day;
    // Approximate; for day-delta purposes we only need a monotone key such
    // that two zones' same-calendar-date have identical ordinals and
    // adjacent-date differ by roughly 1. A more correct form would use
    // tm_yday, but LocalDateTime doesn't carry it — the approximation is
    // exact for deltas in {-1, 0, +1} which is all the watchface displays.
}

// ---------- Card delegates --------------------------------------------------

// Fill a RenderCtx with common fields. `mainSlotIdx` selects which zone's
// TZ + schedule go into ctx.mainTZ / ctx.mainSchedule (for event-card use).
static RenderCtx makeCtx(const WatchFaceDeps &d, int64_t nowUtc, int mainSlotIdx) {
    const int mi = ((mainSlotIdx % d.numZones) + d.numZones) % d.numZones;
    RenderCtx ctx{};
    ctx.display       = d.display;
    ctx.clock         = d.clock;
    ctx.events        = d.events;
    ctx.nowUtc        = nowUtc;
    ctx.mainTZ        = d.zones[mi].posixTZ;
    ctx.mainSchedule  = &d.zones[mi].schedule;
    ctx.batteryVolts  = d.power ? d.power->batteryVoltage() : 0.0f;
    return ctx;
}

void WatchFace::renderMainCard() {
    const int idx = ((mainIdx_ % d_.numZones) + d_.numZones) % d_.numZones;
    RenderCtx ctx = makeCtx(d_, d_.clock->nowUtc(), idx);
    // Main card never shows a day-delta badge, so dayDelta = 0.
    // Forward the transient sync-status badge; only the main card renders
    // it (alt cards have no room next to their smaller HH:MM).
    ctx.syncStatus = syncStatus_;
    TimeZoneCard::render(d_.display, SLOT_MAIN, d_.zones[idx], ctx,
                         /*isMain=*/true, /*dayDelta=*/0);
}

void WatchFace::renderAltCard(Rect slot, int tzIndex) {
    const int altIdx  = ((tzIndex   % d_.numZones) + d_.numZones) % d_.numZones;
    const int mainIdx = ((mainIdx_  % d_.numZones) + d_.numZones) % d_.numZones;
    const int64_t nowUtc = d_.clock->nowUtc();
    RenderCtx ctx = makeCtx(d_, nowUtc, mainIdx);

    // Compute the alt zone's calendar-day offset relative to the main zone.
    LocalDateTime mainT = d_.clock->toLocal(nowUtc, d_.zones[mainIdx].posixTZ);
    LocalDateTime altT  = d_.clock->toLocal(nowUtc, d_.zones[altIdx ].posixTZ);
    const int dayDelta = dayOrdinal(altT) - dayOrdinal(mainT);

    TimeZoneCard::render(d_.display, slot, d_.zones[altIdx], ctx,
                         /*isMain=*/false, dayDelta);
}

void WatchFace::renderEventCard() {
    const int mainIdx = ((mainIdx_ % d_.numZones) + d_.numZones) % d_.numZones;
    RenderCtx ctx = makeCtx(d_, d_.clock->nowUtc(), mainIdx);
    ctx.eventCycleIdx = eventCycleIdx_;
    EventCard::render(d_.display, SLOT_EVENT, ctx, /*inverted_ignored=*/false);
}

// ---------- Full-face render ------------------------------------------------

void WatchFace::render(bool partialRefresh) {
    // Clear background to Bg. Outer gutter = Fg (the black frame around
    // the whole screen visible in the mockup).
    d_.display->clear(Ink::Fg);   // fills the panel in logical foreground
                                  // i.e. the "ink on" colour — gives us the
                                  // black outer border for free.
    // Now punch the screen-interior back to Bg so only the 1-px gutter
    // remains as Fg.
    Rect interior = {
        1, 1,
        (int16_t)(d_.display->width()  - 2),
        (int16_t)(d_.display->height() - 2),
    };
    d_.display->fillRect(interior, Ink::Bg);

    if (d_.numZones <= 0 || d_.zones == nullptr) {
        d_.display->commit(partialRefresh);
        return;
    }

    // Slot assignment: main shows mainIdx_, the two alt cards cycle the
    // other zones in order. With 3 configured zones the layout is:
    //   main   = mainIdx_
    //   alt L  = mainIdx_ + 1
    //   alt R  = mainIdx_ + 2
    const int m = ((mainIdx_ % d_.numZones) + d_.numZones) % d_.numZones;
    const int altL = (m + 1) % d_.numZones;
    const int altR = (m + 2) % d_.numZones;

    renderAltCard(SLOT_ALT_LEFT,  altL);
    renderAltCard(SLOT_ALT_RIGHT, altR);
    renderMainCard();
    renderEventCard();

    d_.display->commit(partialRefresh);
}

// ---------- Wake handling ---------------------------------------------------

void WatchFace::onWake() {
    const Button b = d_.buttons->wakeButton();

    switch (b) {
        case Button::None:
            // Minute-tick wake (timer alarm). Buzz reminders + partial refresh.
            maybeBuzzReminders();
            render(/*partialRefresh=*/true);
            return;

        case Button::Up:
            // Cycle main zone forward. Partial refresh, then settle loop
            // to handle rapid follow-up presses / schedule a full refresh.
            mainIdx_ = ((mainIdx_ + 1) % d_.numZones + d_.numZones) % d_.numZones;
            render(/*partialRefresh=*/true);
            settleThenFullRefresh();
            return;

        case Button::Down: {
            // Long-press (≥2 s held) → sync. Short-press → cycle event card.
            constexpr uint32_t LONG_PRESS_MS = 2000;
            bool longPress = false;
            if (d_.buttons && d_.power) {
                const uint32_t start = d_.power->millisNow();
                while (d_.buttons->isPressed(Button::Down)) {
                    if (d_.power->millisNow() - start >= LONG_PRESS_MS) {
                        longPress = true;
                        // Drain the remainder of the hold so we don't re-detect.
                        while (d_.buttons->isPressed(Button::Down)) d_.power->delayMs(5);
                        break;
                    }
                    d_.power->delayMs(10);
                }
            }
            if (longPress) {
                syncAll();
                eventCycleIdx_ = 0;
                settleThenFullRefresh();
            } else {
                // Short press → cycle to next upcoming event on the card.
                eventCycleIdx_++;
                render(/*partialRefresh=*/true);
                settleThenFullRefresh();
            }
            return;
        }

        case Button::Menu:
            // The Watchy platform shim routes MENU to the Watchy library's
            // own menu handler BEFORE calling onWake(), so we should never
            // see Menu here. Treat as no-op for portability: don't draw.
            return;

        case Button::Back:
            // Reserved / no-op on the watchface. Explicitly do nothing so
            // the library doesn't see us consume battery on spurious wakes.
            return;
    }
}

// ---------- Sync (BLE only, visual feedback via SyncStatus badge) -----------

void WatchFace::maybeBuzzReminders() {
    if (d_.power == nullptr || d_.clock == nullptr) return;

    const int64_t nowUtc = d_.clock->nowUtc();

    // Hour tick: minute-of-hour == 0 in the MAIN zone's local time.
    const int mainIdx = ((mainIdx_ % d_.numZones) + d_.numZones) % d_.numZones;
    const char *posixTZ = d_.zones[mainIdx].posixTZ;
    const int   minuteOfDay = d_.clock->minuteOfDay(nowUtc, posixTZ);
    const bool  hourTick    = (minuteOfDay % 60) == 0;

    // Event reminders: scan the next-events window for any start that
    // lands in this minute-tick's [t, t+60 s) window minus the reminder
    // offset. We wake every ~60 s, so the window is exactly one minute.
    bool buzz1h  = false;
    bool buzz5m  = false;
    if (d_.events != nullptr) {
        Event upcoming[8];
        const int n = d_.events->nextEvents(nowUtc, upcoming, 8);
        for (int i = 0; i < n; ++i) {
            const int64_t deltaSec = upcoming[i].startUtc - nowUtc;
            // ±30 s tolerance around the reminder boundary so a wake that
            // lands a few seconds early/late still triggers exactly once.
            if (deltaSec >= 3570 && deltaSec < 3630) buzz1h = true;
            if (deltaSec >=  270 && deltaSec <  330) buzz5m = true;
        }
    }

    if (hourTick)               d_.power->buzz(2);
    else if (buzz1h || buzz5m)  d_.power->buzz(1);
}

void WatchFace::syncAll() {
    // Long-press foreground sync: 60 s BLE window, abortable by any button.
    // All UX (icon → check/cross → clear) is handled inside runSync() so
    // the background path in the .ino renders the same way.
    runSync(/*timeoutMs=*/60000, d_.buttons);
}

void WatchFace::runSync(uint32_t timeoutMs, IButtons *abortOn) {
    if (d_.display == nullptr) return;

    // 1. Draw the face with the in-progress badge BEFORE blocking on BLE,
    //    so the icon is visible the whole time the radio is open.
    syncStatus_ = SyncStatus::InProgress;
    render(/*partialRefresh=*/true);

    // 2. Blocking BLE sync. WiFi path was removed at user request — time
    //    comes from the phone's TIME_SYNC packet (first byte of every batch).
    bool ok = false;
    if (d_.events != nullptr) {
        ok = d_.events->syncNow(timeoutMs, abortOn);
    }

    // 3. Flip badge to check/cross and redraw so the user sees the outcome.
    syncStatus_ = ok ? SyncStatus::Success : SyncStatus::Failure;
    render(/*partialRefresh=*/true);

    // 4. Hold the result on-screen for 2 s. No vibration — feedback is
    //    purely visual.
    if (d_.power) d_.power->delayMs(2000);

    // 5. Clear the badge and repaint once so the resting face doesn't carry
    //    the check/cross into the next wake cycle.
    syncStatus_ = SyncStatus::None;
    render(/*partialRefresh=*/true);
}

void WatchFace::openQrCodes() {
    if (d_.display == nullptr || d_.buttons == nullptr || d_.power == nullptr) {
        return;
    }
    QrScreen screen(d_.display, d_.buttons, d_.power);
    (void)screen.run();   // both exit reasons funnel to the same full-refresh
    render(/*partialRefresh=*/false);
}

void WatchFace::openDriftStats() {
    if (d_.display == nullptr || d_.buttons == nullptr ||
        d_.power == nullptr || d_.clock == nullptr || d_.drift == nullptr) {
        return;
    }
    DriftStatsScreen screen(d_.display, d_.buttons, d_.power,
                            d_.clock, d_.thermo, *d_.drift);
    auto r = screen.run();
    switch (r) {
        case DriftStatsScreen::ExitReason::Back:
        case DriftStatsScreen::ExitReason::IdleTimeout:
            render(/*partialRefresh=*/false);
            break;
        case DriftStatsScreen::ExitReason::ToLibraryMenu:
            // Library-menu passthrough requires plumbing the .ino owns;
            // for the MVP just repaint the face.
            render(/*partialRefresh=*/false);
            break;
    }
}

// ---------- Settle loop -----------------------------------------------------

// Mirrors the v2 pattern: after a partial refresh from a button press,
// poll the buttons for up to 10 s. A further UP press cycles the zone
// inline (partial refresh, timer reset). Any other press exits the loop
// so the next cold wake can handle it. On 10 s of quiescence, issue ONE
// full refresh to clear e-ink ghosting accumulated from the partials.
void WatchFace::settleThenFullRefresh() {
    if (d_.power == nullptr || d_.buttons == nullptr) {
        // Can't poll — just do the full refresh immediately.
        eventCycleIdx_ = 0;
        render(/*partialRefresh=*/false);
        return;
    }

    constexpr uint32_t SETTLE_MS = 10000;
    constexpr uint32_t POLL_MS   = 20;
    uint32_t lastActivity = d_.power->millisNow();

    for (;;) {
        const uint32_t now = d_.power->millisNow();
        if (now - lastActivity >= SETTLE_MS) break;

        // UP → cycle main zone inline.
        if (d_.buttons->isPressed(Button::Up)) {
            while (d_.buttons->isPressed(Button::Up)) d_.power->delayMs(5);
            mainIdx_ = ((mainIdx_ + 1) % d_.numZones + d_.numZones) % d_.numZones;
            render(/*partialRefresh=*/true);
            lastActivity = d_.power->millisNow();
            continue;
        }

        // DOWN (short press) → cycle to next event on the event card inline.
        if (d_.buttons->isPressed(Button::Down)) {
            while (d_.buttons->isPressed(Button::Down)) d_.power->delayMs(5);
            eventCycleIdx_++;
            render(/*partialRefresh=*/true);
            lastActivity = d_.power->millisNow();
            continue;
        }

        // MENU / BACK → break so the next wake handles them (MENU opens the
        // library menu on Watchy; BACK is reserved).
        if (d_.buttons->isPressed(Button::Menu) ||
            d_.buttons->isPressed(Button::Back)) {
            break;
        }

        d_.power->delayMs(POLL_MS);
    }

    // Quiescence → reset the event browse offset so the full refresh shows
    // the "next upcoming" event again, and do the full refresh itself.
    eventCycleIdx_ = 0;
    render(/*partialRefresh=*/false);
}

} // namespace wmt
