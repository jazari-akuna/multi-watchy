#ifndef SETTINGS_H
#define SETTINGS_H

// ---- Weather settings (drives auto NTP sync as a side effect of weather fetch) ----
// Shenzhen city ID on openweathermap.org — anchors gmtOffset to UTC+8 after each weather poll.
#define CITY_ID "1795565"
#define OPENWEATHERMAP_URL "http://api.openweathermap.org/data/2.5/weather?id={cityID}&lang={lang}&units={units}&appid={apiKey}"
#define OPENWEATHERMAP_APIKEY "REDACTED" // public default shipped by SQFMI — replace if you have your own
#define TEMP_UNIT "metric"
#define TEMP_LANG "en"
#define WEATHER_UPDATE_INTERVAL 30 // minutes

// ---- NTP ----
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC (3600 * 8) // Shenzhen UTC+8 — will be overwritten by weather API response

// Defined in WatchyMultiTZ.ino (only that TU needs it — prevents ODR duplicate-definition).
extern watchySettings settings;

// ---- Multi-timezone zone table ----
struct MultiTZZone {
    const char *label;   // 3-letter airport code shown on-screen
    const char *city;    // fuller name, unused for now but kept for reference
    const char *posix;   // POSIX TZ string with DST rules
};

static const MultiTZZone ZONES[] = {
    { "SZX", "Shenzhen",      "CST-8" },                              // UTC+8, no DST
    { "SFO", "San Francisco", "PST8PDT,M3.2.0,M11.1.0" },              // US DST
    { "ZRH", "Zurich",        "CET-1CEST,M3.5.0,M10.5.0/3" },          // EU DST
};
static const int NUM_ZONES = sizeof(ZONES) / sizeof(ZONES[0]);

#endif // SETTINGS_H
