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
    // Draws the bottom status strip: battery + wifi icon + a thin hairline.
    void drawChrome();
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
