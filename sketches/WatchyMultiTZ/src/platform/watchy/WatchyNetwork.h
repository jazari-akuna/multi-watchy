#pragma once
// Watchy-specific INetwork. Uses the Watchy library's WiFiManager + NTP
// helpers; only turns WiFi/BT off in disconnect().

#include "../../hal/INetwork.h"

class Watchy; // forward-decl from Watchy lib

namespace wmt {

// Bridges to the Watchy library's connectWiFi() / syncNTP() and ensures
// the radios are powered down on disconnect for deep-sleep current draw.
class WatchyNetwork final : public INetwork {
public:
    explicit WatchyNetwork(::Watchy *parent) : parent_(parent) {}

    bool connect() override;
    void disconnect() override;
    bool syncNtp(const char *server = "pool.ntp.org") override;

private:
    ::Watchy *parent_;
};

} // namespace wmt
