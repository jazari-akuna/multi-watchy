#pragma once
// Network abstraction. Minimal — only the pieces the face actually uses.
// BLE is NOT included here; BLE-driven event ingest goes through
// IEventProvider (platform-specific implementation writes into a buffer
// the provider exposes).

#include "Types.h"

namespace wmt {

class INetwork {
public:
    virtual ~INetwork() = default;

    // Connect using credentials persisted by the platform (Watchy: NVS from
    // WiFiManager captive-portal flow). Blocking, ~3 s timeout.
    // Returns true if connected.
    virtual bool connect() = 0;

    virtual void disconnect() = 0;

    // Synchronous NTP sync → writes to clock via IClock::setUtc().
    // Returns true on success.
    virtual bool syncNtp(const char *server = "pool.ntp.org") = 0;
};

} // namespace wmt
