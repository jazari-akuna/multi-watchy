#ifndef WATCHY_MULTI_TZ_H
#define WATCHY_MULTI_TZ_H

#include <Watchy.h>
#include "settings.h"

class WatchyMultiTZ : public Watchy {
public:
    using Watchy::Watchy;   // inherit constructor taking const watchySettings&

    void drawWatchFace() override;
    void handleButtonPress() override;

private:
    // Draws the "big" middle slot for the current zone (label + time + full date).
    void drawZoneBig(const MultiTZZone &z, const struct tm &localTm, int topY);
    // Draws a "small" slot (top or bottom strip) for a non-current zone.
    void drawZoneSmall(const MultiTZZone &z, const struct tm &localTm, int dayDelta, int topY);
    // Draw a 3-px-thick day bar around the given rectangle, colouring each
    // perimeter pixel according to the zone's work/lunch/night schedule.
    void drawDayBar(int x0, int y0, int x1, int y1,
                    int workStartMin, int workEndMin,
                    int lunchStartMin, int lunchEndMin,
                    int nowMinute);
    // Paint the inverted-colour perpendicular "now" tick on top of a day bar.
    void drawNowTick(int x0, int y0, int x1, int y1,
                     int workStartMin, int workEndMin,
                     int lunchStartMin, int lunchEndMin,
                     int nowMinute);
    // Compact battery icon (16x6 body + 2x3 cap + 0-3 fill bars).
    void drawBatteryIcon(int x, int y);
    // Forces a WiFi-up → NTP sync → WiFi-off cycle and refreshes the watchface.
    void forceSyncNow();
    // Poll for up to 10 s after a button-driven partial refresh. Handles
    // additional UP presses inline (partial refresh + timer reset) so rapid
    // zone cycling stays snappy; any other button press exits immediately so
    // the next deep-sleep→ext1-wake cycle handles it. On 10 s of quiescence
    // performs a single full refresh to clear accumulated ghosting.
    void settleThenFullRefresh();
    // Utility: days-since-epoch for a struct tm whose UTC seconds we have.
    static int daysFromEpoch(time_t epoch);
};

#endif // WATCHY_MULTI_TZ_H
