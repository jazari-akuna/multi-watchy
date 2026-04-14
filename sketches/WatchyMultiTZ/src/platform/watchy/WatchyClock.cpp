#include "WatchyClock.h"

#include <Watchy.h>      // brings in WatchyRTC + TimeLib (makeTime/breakTime)
#include <TimeLib.h>
#include <time.h>
#include <stdlib.h>      // setenv

// The Watchy library owns this persistent offset (seconds east of UTC).
// Declared here so we can read it from UTC<->local conversions without
// pulling more of the library's internals.
extern RTC_DATA_ATTR long gmtOffset;

namespace wmt {

int64_t WatchyClock::nowUtc() {
    tmElements_t tm;
    rtc_.read(tm);
    time_t local = makeTime(tm);
    return static_cast<int64_t>(local) - static_cast<int64_t>(gmtOffset);
}

void WatchyClock::setUtc(int64_t epoch) {
    tmElements_t tm;
    breakTime(static_cast<time_t>(epoch + gmtOffset), tm);
    rtc_.set(tm);
}

LocalDateTime WatchyClock::toLocal(int64_t utc, const char *posixTZ) {
    setenv("TZ", posixTZ, 1);
    tzset();

    struct tm tm_;
    time_t t = static_cast<time_t>(utc);
    localtime_r(&t, &tm_);

    LocalDateTime out;
    out.year   = static_cast<int16_t>(tm_.tm_year + 1900);
    out.month  = static_cast<int8_t>(tm_.tm_mon + 1);
    out.day    = static_cast<int8_t>(tm_.tm_mday);
    out.hour   = static_cast<int8_t>(tm_.tm_hour);
    out.minute = static_cast<int8_t>(tm_.tm_min);
    out.second = static_cast<int8_t>(tm_.tm_sec);
    out.wday   = static_cast<int8_t>(tm_.tm_wday); // 0=Sun..6=Sat

    // Restore UTC so anything else in the library that reads TZ is predictable.
    setenv("TZ", "UTC0", 1);
    tzset();
    return out;
}

} // namespace wmt
