// WatchyMultiTZ v3 — Watchy sketch entry point.
//
// Thin glue: instantiates the Watchy-specific HAL adapters (which live under
// src/platform/watchy/), builds a WatchFaceDeps bundle, and hands control to
// the platform-agnostic WatchFace on every wake. Subclasses Watchy so the
// library's init() still handles menus and deep-sleep scheduling.

#include <Watchy.h>
#include "settings.h"

#include "src/face/WatchFace.h"
#include "src/face/DayBar.h"
#include "src/face/DriftTracker.h"
#include "src/platform/watchy/WatchyDisplay.h"
#include "src/platform/watchy/WatchyClock.h"
#include "src/platform/watchy/WatchyButtons.h"
#include "src/platform/watchy/WatchyPower.h"
#include "src/platform/watchy/WatchyNetwork.h"
#include "src/platform/watchy/WatchyStorage.h"
#include "src/platform/watchy/WatchyThermometer.h"
#include "src/platform/watchy/BleEventProvider.h"

// [WMT-DBG] build fingerprint defined in WatchFace.cpp.
extern "C" const char WMT_BUILD_ID[];

// -----------------------------------------------------------------------------
// Persistent state that must survive deep-sleep. Lives in RTC slow memory;
// lost on battery pull (defaults to 0 on cold boot, which means "SZX main").
// -----------------------------------------------------------------------------
RTC_DATA_ATTR int g_mainIdx = 2;   // default: ZRH as main (matches mockup)
// Minute-tick counter. Every 30 minutes the watchface opens a short silent
// BLE advertise window so Gadgetbridge can reconnect and push fresh events.
RTC_DATA_ATTR uint16_t g_minutesSinceBleSync = 0;
// Buzz-suppression window (in minute-ticks). Set by WatchFace::runSync()
// after every manual or auto sync; each subsequent minute-tick decrements
// it and skips maybeBuzzReminders() until it reaches 0. See WatchFace.cpp.
//
// Why this exists: a BLE sync can land the deep-sleep return very close to
// an HH:00 boundary in the main zone. clearAlarm() then arms the RTC for
// the very next minute, and the first timer wake after the sync hits
// maybeBuzzReminders() with minute-of-hour==0, firing the 2-pulse hour
// chime. The user feels "double buzz right after sync completes" even
// though the sync path itself never touches the motor. Skipping the
// reminder scan for a couple of ticks after any sync removes the
// false-positive; a real HH:00 on a normal tick is untouched.
RTC_DATA_ATTR uint8_t g_suppressBuzzTicks = 0;

// [WMT-DBG] armed in setup() if any serial byte arrives within the boot
// window. Read by WatchyMultiTZ::deepSleep() to route into the REPL.
// NOT in RTC memory — resets every boot, must be re-armed each time.
static bool g_debugReplArmed = false;

// The Watchy library uses its own watchySettings struct (cityID, weather
// settings, NTP server, etc.). We build a light instance so the library's
// built-in menu options (Sync NTP, Setup WiFi) keep working without us
// having to replicate them. Values come from namespace wmt:: in settings.h.
static watchySettings libSettings{
    /*cityID=*/               String(wmt::CITY_ID),
    /*lat=*/                  String(""),
    /*lon=*/                  String(""),
    /*weatherAPIKey=*/        String(wmt::OPENWEATHERMAP_APIKEY),
    /*weatherURL=*/           String("http://api.openweathermap.org/data/2.5/weather?id={cityID}&lang={lang}&units={units}&appid={apiKey}"),
    /*weatherUnit=*/          String("metric"),
    /*weatherLang=*/          String("en"),
    /*weatherUpdateInterval=*/(int8_t)wmt::WEATHER_UPDATE_MIN,
    /*ntpServer=*/            String(wmt::NTP_SERVER),
    /*gmtOffset=*/            wmt::GMT_OFFSET_SEC,
    // Disable the library's built-in hour-tick buzz. It fires from the
    // minute-tick wake AFTER showWatchFace() returns, so if a manual sync
    // straddles an HH:00 boundary the next wake's chime looks like a
    // "vibration after sync" even though nothing in our sync path buzzes.
    // Hour + event-reminder haptics are handled in-face by
    // WatchFace::maybeBuzzReminders() instead.
    /*vibrateOClock=*/        false,
};

// -----------------------------------------------------------------------------
// Our Watchy subclass. Overrides drawWatchFace (minute-tick render) and
// handleButtonPress (wake dispatch) to route through the platform-agnostic
// WatchFace. Menu button still goes through the library's built-in flow.
// -----------------------------------------------------------------------------
class WatchyMultiTZ final : public Watchy {
public:
    WatchyMultiTZ() : Watchy(libSettings) {}

    void drawWatchFace() override {
        ensureFace();
        const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
        // Periodic silent sync: every 30 minute-tick wakes, open a short
        // BLE advertise window so a nearby Gadgetbridge can push updates.
        // Skipped on cold boot (wakeup cause == UNDEFINED) per user directive.
        //
        // runSync() renders the face with the sync badge (dots → check/cross)
        // instead of going through a raw events_->syncNow(); this gives the
        // same visual feedback as the long-press foreground sync. nullptr
        // for abortOn so a casual button tap doesn't kill a passive sync.
        if (cause != ESP_SLEEP_WAKEUP_UNDEFINED) {
            if (g_minutesSinceBleSync < 65535) ++g_minutesSinceBleSync;
            if (g_minutesSinceBleSync >= 30) {
                g_minutesSinceBleSync = 0;
                face_->runSync(/*timeoutMs=*/10000, /*abortOn=*/nullptr);
                return;   // runSync() already repainted the resting face
                          // and bumped g_suppressBuzzTicks so the next tick
                          // or two skips maybeBuzzReminders (prevents the
                          // false "buzz after sync" near HH:00).
            }
        }
        // Regular minute-tick render. Skip maybeBuzzReminders entirely when
        // we're inside the post-sync suppression window (set by runSync in
        // WatchFace.cpp) OR when this wake isn't a bona-fide timer/alarm
        // tick — the watchface also gets drawn on reset/power-on, where the
        // minute value is basically random w.r.t. the wall clock.
        const bool isTick = (cause == ESP_SLEEP_WAKEUP_TIMER ||
                             cause == ESP_SLEEP_WAKEUP_EXT0);
        if (g_suppressBuzzTicks > 0) {
            --g_suppressBuzzTicks;
        } else if (isTick) {
            face_->maybeBuzzReminders();
        }
        face_->render(/*partialRefresh=*/true);
    }

    void handleButtonPress() override {
        uint64_t wake = esp_sleep_get_ext1_wakeup_status();
        if (wake & MENU_BTN_MASK) {
            // From the watchface, MENU opens our DriftStats overlay; from
            // any library state, delegate so the user can navigate out.
            if (guiState == WATCHFACE_STATE) {
                ensureFace();
                face_->openDriftStats();
                return;
            }
            Watchy::handleButtonPress();
            return;
        }
        if (wake & BACK_BTN_MASK) {
            // From the watchface, BACK opens the QR-cycle overlay. Any
            // non-watchface state (library menu) falls back to the default
            // library handler so the user can navigate back.
            if (guiState == WATCHFACE_STATE) {
                ensureFace();
                face_->openQrCodes();
                return;
            }
            Watchy::handleButtonPress();
            return;
        }
        ensureFace();
        face_->onWake();
    }

    // [WMT-DBG] Serial-driven REPL so manual sync can be triggered without
    // a physical button press. Entered if any byte arrives on Serial within
    // ~1.5 s of boot (see setup()). Commands (one char per line, newline
    // ignored): s=force manual sync, 1=buzz(1), 2=buzz(2), v=raw vibMotor,
    // r=ESP.restart, q=exit REPL and deep-sleep normally. The library's
    // deepSleep is now virtual so this REPL can run instead.
    void debugRepl() {
        ensureFace();
        Serial.println();
        Serial.println("[WMT-DBG] REPL. cmds: s=sync 1=buzz(1) 2=buzz(2) v=vibMotor(75,4) r=restart q=quit");
        Serial.flush();
        while (true) {
            if (Serial.available()) {
                int c = Serial.read();
                if (c == '\r' || c == '\n' || c < 32) continue;
                Serial.printf("[WMT-DBG] cmd='%c'\n", (char)c);
                Serial.flush();
                switch (c) {
                    case 's':
                        Serial.println("[WMT-DBG] -> face_->syncAll()");
                        face_->syncAll();
                        Serial.println("[WMT-DBG] <- syncAll returned");
                        break;
                    case '1':
                        powerHal_->buzz(1);
                        break;
                    case '2':
                        powerHal_->buzz(2);
                        break;
                    case 'v':
                        Watchy::vibMotor(75, 4);
                        break;
                    case 'r':
                        Serial.println("[WMT-DBG] ESP.restart()");
                        Serial.flush();
                        ESP.restart();
                        break;
                    case 'q':
                        Serial.println("[WMT-DBG] quitting REPL -> deepSleep");
                        Serial.flush();
                        return;
                    default:
                        Serial.printf("[WMT-DBG] unknown cmd '%c'\n", (char)c);
                        break;
                }
                Serial.flush();
            }
            delay(10);
        }
    }

    // Override library's deepSleep so the REPL (when armed) can keep the
    // device awake. When g_debugReplArmed is false this is a straight
    // passthrough.
    void deepSleep() override {
        if (g_debugReplArmed) {
            debugRepl();
            g_debugReplArmed = false;   // quit REPL once -> real sleep
        }
        Watchy::deepSleep();
    }

private:
    // Lazily-constructed HAL + face. Stored as pointers so we can init them
    // on first use (by which point Watchy::display / Watchy::RTC are live).
    void ensureFace() {
        if (face_) return;

        displayHal_ = new wmt::WatchyDisplayHal(Watchy::display);
        buttonsHal_ = new wmt::WatchyButtons();
        powerHal_   = new wmt::WatchyPower(this);
        storageHal_ = new wmt::WatchyStorage();
        thermoHal_  = new wmt::WatchyThermometer();

        drift_      = new wmt::DriftTracker();
        drift_->init(storageHal_, thermoHal_);

        clockHal_   = new wmt::WatchyClock(Watchy::RTC, drift_, thermoHal_);
        networkHal_ = new wmt::WatchyNetwork(this, clockHal_, drift_);
        events_     = new wmt::BleEventProvider(clockHal_, drift_, powerHal_);

        // Configure DayBar's global minute axis from the compile-time
        // schedule extremes.
        wmt::DayBar::configure(wmt::BAR_START_MIN, wmt::BAR_END_MIN);

        wmt::WatchFaceDeps deps{};
        deps.display  = displayHal_;
        deps.clock    = clockHal_;
        deps.buttons  = buttonsHal_;
        deps.power    = powerHal_;
        deps.network  = networkHal_;
        deps.events   = events_;
        deps.drift    = drift_;
        deps.thermo   = thermoHal_;
        deps.zones    = wmt::ZONES;
        deps.numZones = wmt::NUM_ZONES;

        face_ = new wmt::WatchFace(deps, g_mainIdx);
    }

    wmt::WatchyDisplayHal   *displayHal_ = nullptr;
    wmt::WatchyClock        *clockHal_   = nullptr;
    wmt::WatchyButtons      *buttonsHal_ = nullptr;
    wmt::WatchyPower        *powerHal_   = nullptr;
    wmt::WatchyNetwork      *networkHal_ = nullptr;
    wmt::WatchyStorage      *storageHal_ = nullptr;
    wmt::WatchyThermometer  *thermoHal_  = nullptr;
    wmt::DriftTracker       *drift_      = nullptr;
    wmt::BleEventProvider   *events_     = nullptr;
    wmt::WatchFace          *face_       = nullptr;
};

static WatchyMultiTZ watchy;

void setup() {
    Serial.begin(115200);
    // Tiny settle so the first printf isn't eaten by uart-FIFO startup.
    delay(80);
    Serial.printf("\n=== WMT boot | wakeup=%d | build=%s ===\n",
                  (int)esp_sleep_get_wakeup_cause(),
                  WMT_BUILD_ID);
    Serial.flush();

    // [WMT-DBG] REPL arm: 1.5 s window on COLD BOOT ONLY to enter the
    // serial REPL. Gated on wakeup==UNDEFINED so timer/button wakes don't
    // pay the 1.5 s cost every minute. Send any byte during this window
    // and deepSleep() will run the REPL loop instead of real deep-sleep.
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED) {
        const uint32_t armDeadline = millis() + 1500;
        while (millis() < armDeadline) {
            if (Serial.available() > 0) {
                while (Serial.available() > 0) Serial.read();
                g_debugReplArmed = true;
                Serial.println("[WMT-DBG] REPL armed (will engage at deepSleep)");
                Serial.flush();
                break;
            }
            delay(10);
        }
    }

    watchy.init();
}
void loop()  {}
