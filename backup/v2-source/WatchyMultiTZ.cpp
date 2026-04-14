#include "WatchyMultiTZ.h"
#include "DSEG7_Classic_Bold_25.h"
#include "DSEG7_Classic_Regular_15.h"
#include "Seven_Segment10pt7b.h"
#include "DSEG7_Classic_Regular_39.h"
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

// Per-zone working-day schedule, parallel to settings.h ZONES[]:
//   0 = SZX Shenzhen  : 09:00..21:00, lunch 12:00..14:00
//   1 = SFO San Fran  : 09:00..19:00, lunch 12:00..13:00
//   2 = ZRH Zurich    : 08:00..18:00, lunch 12:00..13:00
struct DaySchedule {
    int16_t workStartMin;
    int16_t workEndMin;
    int16_t lunchStartMin;
    int16_t lunchEndMin;
};
static const DaySchedule SCHEDULES[NUM_ZONES] = {
    { 9*60, 21*60, 12*60, 14*60 },
    { 9*60, 19*60, 12*60, 13*60 },
    { 8*60, 18*60, 12*60, 13*60 },
};

enum class HourKind : uint8_t { Night, Work, Lunch };

static HourKind categoriseMinute(int minute, int ws, int we, int ls, int le) {
    if (minute < ws || minute >= we) return HourKind::Night;
    if (minute >= ls && minute < le) return HourKind::Lunch;
    return HourKind::Work;
}

// Resolve perimeter pixel index i -> (x, y) of the outermost pixel, and the
// unit inward vector (dx, dy). Perimeter is walked clockwise starting at the
// top-left corner (midnight). Corners are shared between adjacent edges so the
// total perimeter pixel count is P = 2*(W + H) - 4.
static void resolvePerimeterPixel(int i, int x0, int y0, int x1, int y1,
                                  int &x, int &y, int &dxIn, int &dyIn) {
    const int W = x1 - x0 + 1;
    const int H = y1 - y0 + 1;
    // Edge lengths when walking clockwise with shared corners:
    //   top    : W pixels   (0 .. W-1)
    //   right  : H-1 pixels (W .. W+H-2)
    //   bottom : W-1 pixels (W+H-1 .. 2W+H-3)
    //   left   : H-2 pixels (2W+H-2 .. 2W+2H-5)  (= P-1 total)
    if (i < W) {
        x = x0 + i;       y = y0;
        dxIn = 0;         dyIn = +1;
    } else if (i < W + H - 1) {
        x = x1;           y = y0 + (i - W + 1);
        dxIn = -1;        dyIn = 0;
    } else if (i < 2*W + H - 2) {
        x = x1 - (i - (W + H - 1) + 1); y = y1;
        dxIn = 0;         dyIn = -1;
    } else {
        x = x0;           y = y1 - (i - (2*W + H - 2) + 1);
        dxIn = +1;        dyIn = 0;
    }
}

int WatchyMultiTZ::daysFromEpoch(time_t epoch) {
    // floor division so negative epochs also work
    return (int)(epoch / 86400 - (epoch % 86400 < 0 ? 1 : 0));
}

// Day-bar visual layout (thickness 5 px total):
//   t = 0          outer white border (1 px, FG) — drawn by display.drawRect
//   t = 1, 2, 3    content band (3 px), colour depends on HourKind
//   t = 4          inner white border (1 px, FG) — drawn by display.drawRect
//
// The bars are rotated so each zone's lunch MIDPOINT sits exactly at the top
// middle of its rectangle. Because each zone's schedule is constant, the bar
// itself never moves between minute ticks — only the "now" pin moves. The
// mapping used throughout this file is:
//
//   minute(p) = (lunchMidMin + (p - W/2) * 1440 / P) mod 1440
//
// where p is the perimeter pixel index (0..P-1) walking clockwise from the
// top-left corner, P = 2*(W+H)-4, and lunchMidMin is the schedule's lunch
// midpoint in minutes-since-midnight.
//
// The content is drawn as four independent per-edge strip loops instead of
// a single perimeter walk with perpendicular extrusion. That avoids the
// corner-overdraw artefacts the old perimeter-walk suffered from when two
// edges tried to paint different colours into the shared 5×5 corner square.
// With drawRect laying the continuous outer/inner borders, the corners stay
// visually clean regardless of where the category transitions fall.
void WatchyMultiTZ::drawDayBar(int x0, int y0, int x1, int y1,
                               int ws, int we, int ls, int le,
                               int nowMinute)
{
    const int W = x1 - x0 + 1;
    const int H = y1 - y0 + 1;
    const int P = 2*(W + H) - 4;
    const int lunchMid = (ls + le) / 2;

    // 1. Continuous white borders via drawRect — no corner artefacts.
    display.drawRect(x0,     y0,     W,     H,     FG);
    display.drawRect(x0 + 4, y0 + 4, W - 8, H - 8, FG);

    // Helper: perimeter index p → minute-of-day (lunch-centered).
    auto minuteFor = [&](int p) -> int {
        int64_t m = (int64_t)(p - W / 2) * 1440 / P + lunchMid;
        m = ((m % 1440) + 1440) % 1440;
        return (int)m;
    };

    // Helper: category + p → content pixel colour.
    auto contentColour = [&](int p) -> uint16_t {
        HourKind k = categoriseMinute(minuteFor(p), ws, we, ls, le);
        switch (k) {
            case HourKind::Night: return BG;
            case HourKind::Work:  return FG;
            case HourKind::Lunch: return ((p / 2) & 1) ? FG : BG;  // 2-px dashes
        }
        return BG;
    };

    // 2. TOP strip content: rows y0+1..y0+3 across all columns.
    //    Perimeter p = col - x0, so p = W/2 sits at the top-middle column.
    for (int col = x0; col <= x1; col++) {
        int p = col - x0;
        uint16_t c = contentColour(p);
        display.drawPixel(col, y0 + 1, c);
        display.drawPixel(col, y0 + 2, c);
        display.drawPixel(col, y0 + 3, c);
    }

    // 3. RIGHT strip content: cols x1-3..x1-1 across all rows.
    //    Perimeter p = (W-1) + (row - y0), continuing clockwise from top-right.
    //    This intentionally overlaps the top & bottom corner 3×3 squares;
    //    adjacent perimeter indices differ by only ~2.4 min so the resulting
    //    category is the same as the horizontally-painted neighbours in most
    //    cases, and when a category boundary lands inside a corner the tiny
    //    mismatch is no more than one pixel wide.
    for (int row = y0; row <= y1; row++) {
        int p = (W - 1) + (row - y0);
        if (p >= P) p -= P;
        uint16_t c = contentColour(p);
        display.drawPixel(x1 - 1, row, c);
        display.drawPixel(x1 - 2, row, c);
        display.drawPixel(x1 - 3, row, c);
    }

    // 4. BOTTOM strip content: rows y1-1..y1-3 across all columns.
    //    Perimeter p = (W + H - 2) + (x1 - col), continuing clockwise from
    //    bottom-right.
    for (int col = x0; col <= x1; col++) {
        int p = (W + H - 2) + (x1 - col);
        if (p >= P) p -= P;
        uint16_t c = contentColour(p);
        display.drawPixel(col, y1 - 1, c);
        display.drawPixel(col, y1 - 2, c);
        display.drawPixel(col, y1 - 3, c);
    }

    // 5. LEFT strip content: cols x0+1..x0+3 across all rows.
    //    Perimeter p = (2*W + H - 3) + (y1 - row), continuing clockwise from
    //    bottom-left and wrapping past 0 (P) back to the top-left corner.
    for (int row = y0; row <= y1; row++) {
        int p = (2 * W + H - 3) + (y1 - row);
        while (p >= P) p -= P;
        uint16_t c = contentColour(p);
        display.drawPixel(x0 + 1, row, c);
        display.drawPixel(x0 + 2, row, c);
        display.drawPixel(x0 + 3, row, c);
    }

    // 6. Now-tick on top of the filled strips + borders.
    drawNowTick(x0, y0, x1, y1, ws, we, ls, le, nowMinute);
}

void WatchyMultiTZ::drawNowTick(int x0, int y0, int x1, int y1,
                                int ws, int we, int ls, int le,
                                int nowMinute)
{
    const int W = x1 - x0 + 1;
    const int H = y1 - y0 + 1;
    const int P = 2*(W + H) - 4;
    const int TH = 5;            // matches drawDayBar bar thickness
    const int INNER_LEN = 4;     // pixels extending inward into the zone interior
    const int TICK_WIDTH = 2;    // pin is 2 px wide along the perimeter direction
    const int lunchMid = (ls + le) / 2;

    // Clamp nowMinute to [0, 1439] defensively.
    if (nowMinute < 0) nowMinute = 0;
    if (nowMinute > 1439) nowMinute = 1439;

    // Inverse of the lunch-centered mapping:
    //   minute = (p - W/2) * 1440 / P + lunchMid
    //   p      = W/2 + (minute - lunchMid) * P / 1440
    int iBase = W / 2 + (int)((int64_t)(nowMinute - lunchMid) * P / 1440);
    iBase = ((iBase % P) + P) % P;

    HourKind k = categoriseMinute(nowMinute, ws, we, ls, le);

    // Draw TICK_WIDTH adjacent perimeter pixels, each with its own
    // perpendicular extrusion.
    for (int w = 0; w < TICK_WIDTH; w++) {
        int i = (iBase + w) % P;

        int x, y, dxIn, dyIn;
        resolvePerimeterPixel(i, x0, y0, x1, y1, x, y, dxIn, dyIn);

        // In-bar portion: each of the 5 thickness layers gets a pixel with
        // the INVERTED colour of whatever drawDayBar painted there.
        for (int t = 0; t < TH; t++) {
            uint16_t segColour;
            if (t == 0 || t == TH - 1) {
                segColour = FG;   // white border row
            } else {
                switch (k) {
                    case HourKind::Night: segColour = BG; break;
                    case HourKind::Work:  segColour = FG; break;
                    case HourKind::Lunch: segColour = ((i / 2) & 1) ? FG : BG; break;
                }
            }
            uint16_t tickColour = (segColour == FG) ? BG : FG;
            display.drawPixel(x + dxIn * t, y + dyIn * t, tickColour);
        }

        // Interior portion: inward from the innermost bar row. Zone interior
        // is BG (black in dark mode), so each tick pixel is FG (white).
        for (int t = TH; t < TH + INNER_LEN; t++) {
            display.drawPixel(x + dxIn * t, y + dyIn * t, FG);
        }
    }
}

void WatchyMultiTZ::drawBatteryIcon(int x, int y) {
    float vbat = getBatteryVoltage();
    int bars = 0;
    if      (vbat > 4.0f) bars = 3;
    else if (vbat > 3.6f) bars = 2;
    else if (vbat > 3.2f) bars = 1;

    // 16x6 body with a 2x3 cap on the right.
    const int batW = 16;
    const int batH = 6;
    display.drawRect(x, y, batW, batH, FG);
    display.fillRect(x + batW, y + 1, 2, 4, FG);
    // Three fill bars inside.
    const int segW = 4;
    const int segH = 4;
    const int segY = y + 1;
    for (int i = 0; i < 3; i++) {
        int sx = x + 1 + i * (segW + 1);
        if (i < bars) display.fillRect(sx, segY, segW, segH, FG);
    }
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

    // 7. Compute each zone's minute-of-day for day-bar rendering.
    auto minuteOfDay = [](const struct tm &t) {
        return t.tm_hour * 60 + t.tm_min;
    };
    int minTop = minuteOfDay(tmTop);
    int minMid = minuteOfDay(tmMid);
    int minBot = minuteOfDay(tmBot);

    // 8. Zone rectangles. Inset 1 px from every screen edge so the white
    //    day-bar border does not touch the physical white bezel. Layout:
    //      border col 0                 (black)
    //      top    zone  y= 1..48
    //      gap         y=49..50
    //      mid    zone  y=51..149
    //      gap         y=150..151
    //      bot    zone  y=152..198
    //      border row 199               (black)
    const int topX0 = 1, topY0 = 1,   topX1 = 198, topY1 = 48;
    const int midX0 = 1, midY0 = 51,  midX1 = 198, midY1 = 149;
    const int botX0 = 1, botY0 = 152, botX1 = 198, botY1 = 198;

    // 9. Draw content first, day bars on top so the tick overlays the bar
    //    without being overdrawn by text.
    // Content origin = outer top + 6 px (5-px bar + 1-px breathing room).
    drawZoneSmall(ZONES[topIdx], tmTop, topDelta, topY0 + 6);
    drawZoneBig(ZONES[midIdx], tmMid, midY0 + 6);
    drawZoneSmall(ZONES[botIdx], tmBot, botDelta, botY0 + 6);

    // 10. Per-zone day bars (perimeter borders + now-tick).
    const DaySchedule &sTop = SCHEDULES[topIdx];
    const DaySchedule &sMid = SCHEDULES[midIdx];
    const DaySchedule &sBot = SCHEDULES[botIdx];
    drawDayBar(topX0, topY0, topX1, topY1,
               sTop.workStartMin, sTop.workEndMin,
               sTop.lunchStartMin, sTop.lunchEndMin,
               minTop);
    drawDayBar(midX0, midY0, midX1, midY1,
               sMid.workStartMin, sMid.workEndMin,
               sMid.lunchStartMin, sMid.lunchEndMin,
               minMid);
    drawDayBar(botX0, botY0, botX1, botY1,
               sBot.workStartMin, sBot.workEndMin,
               sBot.lunchStartMin, sBot.lunchEndMin,
               minBot);

    // 11. Restore TZ so any later library code behaves predictably.
    setenv("TZ", "UTC0", 1);
    tzset();
}

void WatchyMultiTZ::drawZoneBig(const MultiTZZone &z, const struct tm &localTm, int topY) {
    // Label top-left.
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(FG);
    display.setCursor(8, topY + 12);
    display.print(z.label);

    // Compact battery icon top-right of the content area.
    drawBatteryIcon(174, topY + 5);

    // Big HH:MM using DSEG7_Classic_Regular_39. Baseline at topY + 60 so the
    // 39-px tall glyphs fit within the interior (top ~ topY + 21).
    display.setFont(&DSEG7_Classic_Regular_39);
    display.setCursor(8, topY + 60);
    if (localTm.tm_hour < 10) display.print('0');
    display.print(localTm.tm_hour);
    display.print(':');
    if (localTm.tm_min < 10) display.print('0');
    display.print(localTm.tm_min);

    // Date stack on the right column: weekday, month abbreviation, day-of-month.
    // Three lines of FreeMonoBold9pt7b with the day number in DSEG7 Bold 25.
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(150, topY + 32);
    display.print(WDAY3[localTm.tm_wday]);
    display.setCursor(150, topY + 52);
    display.print(MON3[localTm.tm_mon]);

    display.setFont(&DSEG7_Classic_Bold_25);
    display.setCursor(150, topY + 80);
    if (localTm.tm_mday < 10) display.print('0');
    display.print(localTm.tm_mday);
}

void WatchyMultiTZ::drawZoneSmall(const MultiTZZone &z, const struct tm &localTm, int dayDelta, int topY) {
    // Label
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(FG);
    display.setCursor(8, topY + 12);
    display.print(z.label);

    // Day-delta badge, right-aligned (inset for the bar)
    if (dayDelta != 0) {
        char badge[8];
        snprintf(badge, sizeof(badge), "%+dd", dayDelta);
        int16_t  bx1, by1;
        uint16_t bw, bh;
        display.getTextBounds(badge, 0, 0, &bx1, &by1, &bw, &bh);
        display.setCursor(188 - (int)bw, topY + 12);
        display.print(badge);
    }

    // Small HH:MM via DSEG7_Classic_Regular_15
    display.setFont(&DSEG7_Classic_Regular_15);
    display.setCursor(8, topY + 32);
    if (localTm.tm_hour < 10) display.print('0');
    display.print(localTm.tm_hour);
    display.print(':');
    if (localTm.tm_min < 10) display.print('0');
    display.print(localTm.tm_min);
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
