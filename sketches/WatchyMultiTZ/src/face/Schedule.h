#pragma once
// Per-zone working-day schedule, expressed in minutes-since-local-midnight.
// Deliberately POD + constexpr-friendly so settings.h can lay out an array
// of these at compile time and let DayBar derive its bar span from them.

#include <stdint.h>

namespace wmt {

struct Schedule {
    int16_t workStartMin;    // e.g. 9*60 == 09:00
    int16_t workEndMin;      // e.g. 21*60 == 21:00
    int16_t lunchStartMin;   // e.g. 12*60
    int16_t lunchEndMin;     // e.g. 14*60
};

// Constexpr helpers to compute the global bar span across all configured
// zones. These are called at compile time in settings.h with the zone array
// size, which lets DayBar::configure() be called once with the derived
// extremes and every TZ card draws onto the same minute-axis.

constexpr int16_t earliestStart(const Schedule *s, int n) {
    int16_t m = s[0].workStartMin;
    for (int i = 1; i < n; i++) {
        if (s[i].workStartMin < m) m = s[i].workStartMin;
    }
    return m;
}

constexpr int16_t latestEnd(const Schedule *s, int n) {
    int16_t m = s[0].workEndMin;
    for (int i = 1; i < n; i++) {
        if (s[i].workEndMin > m) m = s[i].workEndMin;
    }
    return m;
}

// True if `minute` (0..1439) is in the "night" phase of `s` (outside work).
constexpr bool isNight(int16_t minute, const Schedule &s) {
    return minute < s.workStartMin || minute >= s.workEndMin;
}

} // namespace wmt
