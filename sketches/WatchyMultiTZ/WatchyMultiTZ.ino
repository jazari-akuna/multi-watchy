#include "WatchyMultiTZ.h"

watchySettings settings{
    .cityID                = CITY_ID,
    .lat                   = "",
    .lon                   = "",
    .weatherAPIKey         = OPENWEATHERMAP_APIKEY,
    .weatherURL            = OPENWEATHERMAP_URL,
    .weatherUnit           = TEMP_UNIT,
    .weatherLang           = TEMP_LANG,
    .weatherUpdateInterval = WEATHER_UPDATE_INTERVAL,
    .ntpServer             = NTP_SERVER,
    .gmtOffset             = GMT_OFFSET_SEC,
    .vibrateOClock         = true,
};

WatchyMultiTZ watchy(settings);

void setup() {
    watchy.init();
}

void loop() {}
