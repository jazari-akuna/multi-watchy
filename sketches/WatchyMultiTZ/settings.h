#pragma once
// Global configuration shared by both the Watchy firmware sketch AND the
// desktop simulator. Keep this header free of Arduino- or ESP32-specific
// code so the sim can include it too.

#include "src/face/TimeZone.h"
#include "src/face/Schedule.h"
#include "src/assets/flags.h"

namespace wmt {

// Timezones in display order. mainIdx (held in RTC_DATA_ATTR) indexes into
// this array; UP cycles it forward. Each zone carries its own schedule,
// procedural flag drawer, and the (width, height) of the drawn flag.
//
// Schedules (minutes since local midnight):
//   SZX Shenzhen       work 09:00 - 20:00, lunch 12:00 - 14:00
//   SFO San Francisco  work 09:00 - 19:00, lunch 12:00 - 13:00
//   ZRH Zurich         work 08:00 - 18:00, lunch 12:00 - 13:00
static const TimeZone ZONES[] = {
    { "SZX", "Shenzhen",      "CST-8",
      { 9*60, 20*60, 12*60, 14*60 },
      &drawFlagSzx, /*flagW=*/20, /*flagH=*/14 },
    { "SFO", "San Francisco", "PST8PDT,M3.2.0,M11.1.0",
      { 9*60, 19*60, 12*60, 13*60 },
      &drawFlagSfo, /*flagW=*/20, /*flagH=*/14 },
    { "ZRH", "Zurich",        "CET-1CEST,M3.5.0,M10.5.0/3",
      { 8*60, 18*60, 12*60, 13*60 },
      &drawFlagZrh, /*flagW=*/14, /*flagH=*/14 },
};
static constexpr int NUM_ZONES = (int)(sizeof(ZONES) / sizeof(ZONES[0]));

// Compile-time bar range. Derived from the ZONES table above, but declared
// directly as constexpr because TimeZone carries a function pointer which
// isn't constexpr-friendly in C++17 (would require indirection through a
// constexpr-function-pointer-returning function). We keep these in sync by
// convention; the static_assert below catches drift.
constexpr int16_t EARLIEST_START_MIN =  8 * 60;   // ZRH work start (08:00)
constexpr int16_t LATEST_END_MIN     = 20 * 60;   // SZX work end   (20:00)
constexpr int16_t HALF_WIDTH_MIN     = halfWidth(EARLIEST_START_MIN, LATEST_END_MIN);
constexpr int16_t BAR_START_MIN      = (int16_t)(12*60 - HALF_WIDTH_MIN);
constexpr int16_t BAR_END_MIN        = (int16_t)(12*60 + HALF_WIDTH_MIN);

// OpenWeatherMap is kept for library compatibility (the Watchy library's
// weather-fetch path calls syncNTP as a side effect, which we still want).
// Pick a city in the zone used for NTP anchoring so the library's gmtOffset
// matches what we expect. SZX (Shenzhen, id 1795565) = UTC+8 always.
constexpr const char *CITY_ID              = "1795565";
constexpr const char *OPENWEATHERMAP_APIKEY = "REDACTED";
constexpr const char *NTP_SERVER           = "pool.ntp.org";
constexpr int         GMT_OFFSET_SEC       = 8 * 3600;
constexpr int         WEATHER_UPDATE_MIN   = 30;

} // namespace wmt
