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

// -----------------------------------------------------------------------------
// Persistent state that must survive deep-sleep. Lives in RTC slow memory;
// lost on battery pull (defaults to 0 on cold boot, which means "SZX main").
// -----------------------------------------------------------------------------
RTC_DATA_ATTR int g_mainIdx = 2;   // default: ZRH as main (matches mockup)
// Minute-tick counter. Every 30 minutes the watchface opens a short silent
// BLE advertise window so Gadgetbridge can reconnect and push fresh events.
RTC_DATA_ATTR uint16_t g_minutesSinceBleSync = 0;

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
    /*vibrateOClock=*/        true,
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
        // Periodic silent sync: every 30 minute-tick wakes, open a short
        // BLE advertise window so a nearby Gadgetbridge can push updates.
        // Skipped on cold boot (wakeup cause == UNDEFINED) per user directive.
        if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_UNDEFINED) {
            if (g_minutesSinceBleSync < 65535) ++g_minutesSinceBleSync;
            if (g_minutesSinceBleSync >= 30) {
                g_minutesSinceBleSync = 0;
                // 10 s silent advertise. nullptr for abort-on-button = don't
                // let casual button touches kill a passive background sync.
                events_->syncNow(10000, nullptr);
            }
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
        ensureFace();
        face_->onWake();
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

void setup() { watchy.init(); }
void loop()  {}
