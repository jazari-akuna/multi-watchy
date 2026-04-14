#include "WatchyNetwork.h"

#include <Watchy.h>
#include <WiFi.h>

// The Watchy library owns this persistent offset; syncNTP() wants it.
extern RTC_DATA_ATTR long gmtOffset;

namespace wmt {

bool WatchyNetwork::connect() {
    return parent_->connectWiFi();
}

void WatchyNetwork::disconnect() {
    WiFi.mode(WIFI_OFF);
    btStop();
}

bool WatchyNetwork::syncNtp(const char *server) {
    // Three-arg overload handles NTPClient setup, breakTime() and RTC.set()
    // internally. Accepts a String but we pass through a const char* to
    // avoid forcing a String allocation at our layer.
    return parent_->syncNTP(gmtOffset, String(server));
}

} // namespace wmt
