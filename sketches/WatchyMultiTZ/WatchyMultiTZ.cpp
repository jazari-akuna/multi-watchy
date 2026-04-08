#include "WatchyMultiTZ.h"
#include "DSEG7_Classic_Bold_25.h"
#include "DSEG7_Classic_Regular_15.h"
#include "Seven_Segment10pt7b.h"
#include <Fonts/FreeMonoBold9pt7b.h>
#include <WiFi.h>
#include <time.h>
#include <TimeLib.h>

// The library maintains this in Watchy.cpp; we reuse it read-mostly as the UTC anchor.
extern RTC_DATA_ATTR long gmtOffset;

// Our own persistent state (survives deep sleep; lost on battery pull).
RTC_DATA_ATTR int  currentTzIndex           = 0;   // index into ZONES[]
RTC_DATA_ATTR int  minutesSinceForcedSync   = 0;   // safety-net counter

static constexpr uint16_t FG = GxEPD_WHITE;
static constexpr uint16_t BG = GxEPD_BLACK;

static const char *WDAY3[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
static const char *MON3[]  = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

int WatchyMultiTZ::daysFromEpoch(time_t epoch) {
    // floor division so negative epochs also work
    return (int)(epoch / 86400 - (epoch % 86400 < 0 ? 1 : 0));
}

void WatchyMultiTZ::drawWatchFace() {
    // 1. Safety-net: force a sync every 1440 minutes (~24h).
    minutesSinceForcedSync++;
    if (minutesSinceForcedSync >= 1440) {
        minutesSinceForcedSync = 0;
        forceSyncNow();
        return;
    }

    // 2. Clear screen.
    display.fillScreen(BG);
    display.setTextColor(FG);

    // 3. Derive UTC from the library's local-time RTC snapshot.
    time_t local_epoch = makeTime(currentTime);
    time_t utc_epoch   = local_epoch - gmtOffset;

    // 4. Resolve zone slot assignments.
    int midIdx = ((currentTzIndex % NUM_ZONES) + NUM_ZONES) % NUM_ZONES;
    int topIdx = (midIdx + 1) % NUM_ZONES;
    int botIdx = (midIdx + 2) % NUM_ZONES;

    // 5. Compute local tm for each slot using POSIX TZ strings.
    struct tm tmMid = {}, tmTop = {}, tmBot = {};

    setenv("TZ", ZONES[midIdx].posix, 1);
    tzset();
    localtime_r(&utc_epoch, &tmMid);

    setenv("TZ", ZONES[topIdx].posix, 1);
    tzset();
    localtime_r(&utc_epoch, &tmTop);

    setenv("TZ", ZONES[botIdx].posix, 1);
    tzset();
    localtime_r(&utc_epoch, &tmBot);

    // 6. Day-delta via a monotonic ordinal (400 spacing so tm_yday 0..365 never collides).
    auto tmDayOrd = [](const struct tm &t) {
        return (t.tm_year + 1900) * 400 + t.tm_yday;
    };
    int topDelta = tmDayOrd(tmTop) - tmDayOrd(tmMid);
    int botDelta = tmDayOrd(tmBot) - tmDayOrd(tmMid);

    // 7-11. Layout the three slots and two hairlines.
    drawZoneSmall(ZONES[topIdx], tmTop, topDelta, 4);
    display.drawLine(10, 47, 190, 47, FG);
    drawZoneBig(ZONES[midIdx], tmMid, 54);
    display.drawLine(10, 148, 190, 148, FG);
    drawZoneSmall(ZONES[botIdx], tmBot, botDelta, 152);

    // 12. Battery + wifi + zone dots.
    drawChrome();

    // 13. Restore TZ so any later library code behaves predictably.
    setenv("TZ", "UTC0", 1);
    tzset();
}

void WatchyMultiTZ::drawZoneBig(const MultiTZZone &z, const struct tm &localTm, int topY) {
    // Label
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(FG);
    display.setCursor(10, topY + 12);
    display.print(z.label);

    // Big HH:MM using DSEG7_Classic_Bold_25
    display.setFont(&DSEG7_Classic_Bold_25);
    display.setCursor(30, topY + 50);
    if (localTm.tm_hour < 10) display.print('0');
    display.print(localTm.tm_hour);
    display.print(':');
    if (localTm.tm_min < 10) display.print('0');
    display.print(localTm.tm_min);

    // Full date line "Wed Apr 08 2026" via Seven_Segment10pt7b
    display.setFont(&Seven_Segment10pt7b);
    display.setCursor(10, topY + 85);
    display.print(WDAY3[localTm.tm_wday]);
    display.print(' ');
    display.print(MON3[localTm.tm_mon]);
    display.print(' ');
    if (localTm.tm_mday < 10) display.print('0');
    display.print(localTm.tm_mday);
    display.print(' ');
    display.print(localTm.tm_year + 1900);
}

void WatchyMultiTZ::drawZoneSmall(const MultiTZZone &z, const struct tm &localTm, int dayDelta, int topY) {
    // Label
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(FG);
    display.setCursor(10, topY + 12);
    display.print(z.label);

    // Day-delta badge, right-aligned
    if (dayDelta != 0) {
        char badge[8];
        snprintf(badge, sizeof(badge), "%+dd", dayDelta);
        int16_t  bx1, by1;
        uint16_t bw, bh;
        display.getTextBounds(badge, 0, 0, &bx1, &by1, &bw, &bh);
        display.setCursor(190 - (int)bw, topY + 12);
        display.print(badge);
    }

    // Small HH:MM via DSEG7_Classic_Regular_15
    display.setFont(&DSEG7_Classic_Regular_15);
    display.setCursor(10, topY + 38);
    if (localTm.tm_hour < 10) display.print('0');
    display.print(localTm.tm_hour);
    display.print(':');
    if (localTm.tm_min < 10) display.print('0');
    display.print(localTm.tm_min);
}

void WatchyMultiTZ::drawChrome() {
    // --- Battery bars (right-aligned, bottom) ---
    float vbat = getBatteryVoltage();
    int bars = 0;
    if (vbat > 4.0f)       bars = 3;
    else if (vbat > 3.6f)  bars = 2;
    else if (vbat > 3.2f)  bars = 1;
    else                   bars = 0;

    // Outer battery body: 22x8 rect with a 2x4 cap on the right.
    const int batW = 22;
    const int batH = 8;
    const int batX = 200 - batW - 6;   // small margin from right edge
    const int batY = 189;
    display.drawRect(batX, batY, batW, batH, FG);
    display.fillRect(batX + batW, batY + 2, 2, 4, FG);
    // Three bar segments inside the body.
    const int segW = 6;
    const int segH = 4;
    const int segY = batY + 2;
    for (int i = 0; i < 3; i++) {
        int sx = batX + 2 + i * (segW + 1);
        if (i < bars) {
            display.fillRect(sx, segY, segW, segH, FG);
        }
    }

    // --- WiFi marker (bottom-left) ---
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(FG);
    display.setCursor(5, 198);
    display.print(WIFI_CONFIGURED ? "W" : " ");

    // --- Zone-index dots (bottom center) ---
    for (int i = 0; i < NUM_ZONES; i++) {
        int x = 94 + i * 6;
        display.drawRect(x, 193, 4, 4, FG);
        if (i == currentTzIndex) {
            display.fillRect(x, 193, 4, 4, FG);
        }
    }
}

void WatchyMultiTZ::forceSyncNow() {
    // 1. Flash a "Syncing..." overlay (partial refresh to save battery & time).
    display.setFullWindow();
    display.fillScreen(BG);
    display.setTextColor(FG);
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(30, 100);
    display.print("Syncing NTP...");
    display.display(true);

    bool ok = false;
    if (connectWiFi()) {
        ok = syncNTP(gmtOffset);    // use the last-known anchor offset
    }
    WiFi.mode(WIFI_OFF);
    btStop();

    // 2. Show result for ~1.5 seconds (still partial refresh).
    display.fillScreen(BG);
    display.setCursor(30, 100);
    display.print(ok ? "Sync OK" : "Sync FAIL");
    display.display(true);
    delay(1500);

    // 3. Refresh time and redraw the real watchface with a partial refresh;
    //    settleThenFullRefresh() (called from the DOWN branch) will handle
    //    the eventual full refresh 10 s after user activity stops.
    if (ok) {
        RTC.read(currentTime);
    }
    showWatchFace(true);
}

// Unified state-aware button poll. Handles UP/DOWN/MENU/BACK inline for both
// WATCHFACE_STATE and MAIN_MENU_STATE so the library's base-class
// handleButtonPress (and its 5 s fast-menu loop) never run and therefore
// never trigger the full refreshes baked into those paths. On 10 s of
// quiescence, performs a single full refresh of whichever state we are in.
void WatchyMultiTZ::settleThenFullRefresh() {
    pinMode(MENU_BTN_PIN, INPUT);
    pinMode(BACK_BTN_PIN, INPUT);
    pinMode(UP_BTN_PIN,   INPUT);
    pinMode(DOWN_BTN_PIN, INPUT);

    const unsigned long SETTLE_MS = 10000;
    unsigned long lastActivity = millis();

    while (millis() - lastActivity < SETTLE_MS) {
        // --- UP ---
        if (digitalRead(UP_BTN_PIN) == HIGH) {
            while (digitalRead(UP_BTN_PIN) == HIGH) delay(5);  // release debounce
            if (guiState == WATCHFACE_STATE) {
                currentTzIndex = (currentTzIndex + 1) % NUM_ZONES;
                RTC.read(currentTime);
                showWatchFace(true);
            } else if (guiState == MAIN_MENU_STATE) {
                menuIndex--;
                if (menuIndex < 0) menuIndex = MENU_LENGTH - 1;
                showMenu(menuIndex, true);
            }
            lastActivity = millis();
            continue;
        }
        // --- DOWN ---
        if (digitalRead(DOWN_BTN_PIN) == HIGH) {
            while (digitalRead(DOWN_BTN_PIN) == HIGH) delay(5);
            if (guiState == WATCHFACE_STATE) {
                forceSyncNow();
            } else if (guiState == MAIN_MENU_STATE) {
                menuIndex++;
                if (menuIndex > MENU_LENGTH - 1) menuIndex = 0;
                showMenu(menuIndex, true);
            }
            lastActivity = millis();
            continue;
        }
        // --- MENU ---
        if (digitalRead(MENU_BTN_PIN) == HIGH) {
            while (digitalRead(MENU_BTN_PIN) == HIGH) delay(5);
            if (guiState == WATCHFACE_STATE) {
                showMenu(menuIndex, true);  // partial, not base's full refresh
            } else if (guiState == MAIN_MENU_STATE) {
                // Dispatch the selected menu item. These base-class methods
                // have their own UI loops and may use full refresh internally
                // (that is the library's design); we accept that for rarely
                // visited screens like About/Vibrate/Accel/Set-Time/WiFi/NTP.
                switch (menuIndex) {
                    case 0: showAbout();         break;
                    case 1: showBuzz();          break;
                    case 2: showAccelerometer(); break;
                    case 3: setTime();           break;
                    case 4: setupWifi();         break;
                    case 5: showSyncNTP();       break;
                    default: break;
                }
            }
            lastActivity = millis();
            continue;
        }
        // --- BACK ---
        if (digitalRead(BACK_BTN_PIN) == HIGH) {
            while (digitalRead(BACK_BTN_PIN) == HIGH) delay(5);
            if (guiState == MAIN_MENU_STATE) {
                RTC.read(currentTime);
                showWatchFace(true);  // partial, not base's full refresh
            }
            // In WATCHFACE_STATE, BACK is reserved / no-op; still resets the
            // 10 s timer so "any button push" restarts the settle window.
            lastActivity = millis();
            continue;
        }
        delay(20);
    }

    // 10 s of no input → single full refresh of whichever state we are in.
    if (guiState == WATCHFACE_STATE) {
        showWatchFace(false);
    } else if (guiState == MAIN_MENU_STATE) {
        showMenu(menuIndex, false);
    }
}

void WatchyMultiTZ::handleButtonPress() {
    uint64_t wakeupBit = esp_sleep_get_ext1_wakeup_status();

    // WATCHFACE_STATE: full custom dispatch. MENU opens the menu with a
    // partial refresh instead of the library's full refresh, then enters
    // the settle loop which takes over further navigation.
    if (guiState == WATCHFACE_STATE) {
        if (wakeupBit & UP_BTN_MASK) {
            currentTzIndex = (currentTzIndex + 1) % NUM_ZONES;
            RTC.read(currentTime);
            showWatchFace(true);
            settleThenFullRefresh();
            return;
        }
        if (wakeupBit & DOWN_BTN_MASK) {
            forceSyncNow();
            settleThenFullRefresh();
            return;
        }
        if (wakeupBit & MENU_BTN_MASK) {
            showMenu(menuIndex, true);
            settleThenFullRefresh();
            return;
        }
        if (wakeupBit & BACK_BTN_MASK) {
            settleThenFullRefresh();   // reserved action, but still resets 10s timer
            return;
        }
    }

    // MAIN_MENU_STATE: intercept everything so no base-class full refresh runs.
    if (guiState == MAIN_MENU_STATE) {
        if (wakeupBit & BACK_BTN_MASK) {
            RTC.read(currentTime);
            showWatchFace(true);       // partial exit to watchface
            settleThenFullRefresh();
            return;
        }
        if (wakeupBit & UP_BTN_MASK) {
            menuIndex--;
            if (menuIndex < 0) menuIndex = MENU_LENGTH - 1;
            showMenu(menuIndex, true);
            settleThenFullRefresh();
            return;
        }
        if (wakeupBit & DOWN_BTN_MASK) {
            menuIndex++;
            if (menuIndex > MENU_LENGTH - 1) menuIndex = 0;
            showMenu(menuIndex, true);
            settleThenFullRefresh();
            return;
        }
        if (wakeupBit & MENU_BTN_MASK) {
            switch (menuIndex) {
                case 0: showAbout();         break;
                case 1: showBuzz();          break;
                case 2: showAccelerometer(); break;
                case 3: setTime();           break;
                case 4: setupWifi();         break;
                case 5: showSyncNTP();       break;
                default: break;
            }
            settleThenFullRefresh();
            return;
        }
    }

    // Any other state (APP_STATE, FW_UPDATE_STATE, ...): delegate to base.
    Watchy::handleButtonPress();
}
