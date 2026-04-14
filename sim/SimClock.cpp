// SimClock — host-backed IClock.
//
// Timezone conversion uses libc: we setenv("TZ", posixTZ) + tzset() +
// localtime_r(), then restore TZ=UTC0 so unrelated code sees UTC.
//
// setFakeTime() parses a narrow ISO-8601 subset sufficient for the sim:
//   "YYYY-MM-DDTHH:MM:SSZ"
//   "YYYY-MM-DDTHH:MM:SS+HH:MM"
//   "YYYY-MM-DDTHH:MM:SS-HH:MM"

#include "SimClock.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

namespace wmt {

int64_t SimClock::nowUtc() {
    return utc_;
}

void SimClock::setUtc(int64_t epoch) {
    utc_ = epoch;
}

LocalDateTime SimClock::toLocal(int64_t utc, const char *posixTZ) {
    LocalDateTime out{};

    // Swap to the requested zone, expand, restore UTC.
    setenv("TZ", posixTZ ? posixTZ : "UTC0", 1);
    tzset();

    time_t t = (time_t)utc;
    struct tm tm_{};
    localtime_r(&t, &tm_);

    out.year   = (int16_t)(tm_.tm_year + 1900);
    out.month  = (int8_t)(tm_.tm_mon + 1);
    out.day    = (int8_t)tm_.tm_mday;
    out.hour   = (int8_t)tm_.tm_hour;
    out.minute = (int8_t)tm_.tm_min;
    out.second = (int8_t)tm_.tm_sec;
    out.wday   = (int8_t)tm_.tm_wday;

    setenv("TZ", "UTC0", 1);
    tzset();
    return out;
}

// --- setFakeTime -----------------------------------------------------------

// Parse exactly N ASCII digits starting at *p. Advances p. Returns -1 on
// failure (non-digit encountered).
static int parseN(const char *&p, int n) {
    int v = 0;
    for (int i = 0; i < n; ++i) {
        if (*p < '0' || *p > '9') return -1;
        v = v * 10 + (*p - '0');
        ++p;
    }
    return v;
}

// Convert a calendar (assumed UTC) to a Unix epoch without relying on
// timegm() (non-portable). Uses Howard Hinnant's days_from_civil.
static int64_t utcCalendarToEpoch(int y, int m, int d,
                                  int hh, int mm, int ss) {
    // Days from 1970-01-01 to y-m-d (civil).
    y -= (m <= 2);
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + (d - 1);
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    const int64_t days = (int64_t)era * 146097 + (int64_t)doe - 719468;
    return days * 86400 + (int64_t)hh * 3600 + (int64_t)mm * 60 + ss;
}

bool SimClock::setFakeTime(const char *iso) {
    if (!iso) return false;
    const char *p = iso;

    int y  = parseN(p, 4); if (y  < 0 || *p++ != '-') return false;
    int mo = parseN(p, 2); if (mo < 0 || *p++ != '-') return false;
    int d  = parseN(p, 2); if (d  < 0) return false;
    if (*p != 'T' && *p != ' ') return false;
    ++p;
    int hh = parseN(p, 2); if (hh < 0 || *p++ != ':') return false;
    int mm = parseN(p, 2); if (mm < 0 || *p++ != ':') return false;
    int ss = parseN(p, 2); if (ss < 0) return false;

    int offSec = 0;
    if (*p == 'Z' || *p == '\0') {
        // UTC.
    } else if (*p == '+' || *p == '-') {
        const int sign = (*p == '-') ? -1 : 1;
        ++p;
        int oh = parseN(p, 2); if (oh < 0) return false;
        int om = 0;
        if (*p == ':') { ++p; om = parseN(p, 2); if (om < 0) return false; }
        offSec = sign * (oh * 3600 + om * 60);
    } else {
        return false;
    }

    const int64_t epoch = utcCalendarToEpoch(y, mo, d, hh, mm, ss) - offSec;
    utc_ = epoch;
    return true;
}

} // namespace wmt
