#pragma once
// Watchy-specific INetwork. Uses the Watchy library's WiFiManager + NTP
// helpers; only turns WiFi/BT off in disconnect().
//
// The ctor also takes a WatchyClock + DriftTracker so that syncNtp() can
// bracket the library's NTP call with raw RTC reads, feeding the tracker
// the (preRaw, postRaw) pair it needs to learn the per-unit FC offset.

#include "../../hal/INetwork.h"
#include "../../face/DriftTracker.h"

class Watchy; // forward-decl from Watchy lib

namespace wmt {

class WatchyClock; // forward-decl, defined in WatchyClock.h

// Bridges to the Watchy library's connectWiFi() / syncNTP() and ensures
// the radios are powered down on disconnect for deep-sleep current draw.
class WatchyNetwork final : public INetwork {
public:
    WatchyNetwork(::Watchy *parent, WatchyClock *clock, DriftTracker *drift)
        : parent_(parent), clock_(clock), drift_(drift) {}

    bool connect() override;
    void disconnect() override;
    bool syncNtp(const char *server = "pool.ntp.org") override;

private:
    ::Watchy     *parent_;
    WatchyClock  *clock_;   // may be nullptr (sim variants don't need this one)
    DriftTracker *drift_;   // may be nullptr
};

} // namespace wmt
