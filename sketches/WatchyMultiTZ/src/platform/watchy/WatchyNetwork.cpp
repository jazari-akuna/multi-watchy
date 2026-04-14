#include "WatchyNetwork.h"
#include "WatchyClock.h"

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
    // Bracket the library's NTP call with RAW RTC reads (not the
    // drift-corrected nowUtc()) so DriftTracker measures the true crystal
    // drift, not the residual after our own correction.
    int64_t preRaw = (clock_ ? clock_->rawRtcUtc() : 0);

    // Three-arg overload handles NTPClient setup, breakTime() and RTC.set()
    // internally. Accepts a String but we pass through a const char* to
    // avoid forcing a String allocation at our layer.
    bool ok = parent_->syncNTP(gmtOffset, String(server));
    if (!ok) return false;

    // After a successful library sync, the RTC has been written to NTP time.
    // Read it RAW again; this is our best estimate of NTP truth.
    int64_t postRaw = (clock_ ? clock_->rawRtcUtc() : 0);
    if (drift_ && clock_) {
        drift_->onSample(preRaw, postRaw, static_cast<int32_t>(gmtOffset));
    }
    return true;
}

} // namespace wmt
