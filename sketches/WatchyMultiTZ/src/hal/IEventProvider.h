#pragma once
// Event feed abstraction. Each platform supplies its own source:
//   - Watchy v3 : StubEventProvider returns a single compile-time demo
//                 event (placeholder until BLE companion lands).
//   - Watchy v4 : BleEventProvider stores events pushed in over a custom
//                 GATT characteristic (or a Gadgetbridge-compatible one).
//   - Sim       : SimEventProvider returns hardcoded scenes for testing.
//
// The face treats this as a read-only source; it asks for the next N events
// starting at or after a given UTC and renders them in the event card.

#include "Types.h"
#include <stdint.h>

namespace wmt {

class IEventProvider {
public:
    virtual ~IEventProvider() = default;

    // Fills `out[]` with up to `maxCount` events starting at or after
    // `fromUtc`, sorted ascending by start time. Returns the number
    // actually written. Implementations must be O(1)-ish — no heap, no
    // blocking. `out` is caller-owned.
    virtual int nextEvents(int64_t fromUtc, Event *out, int maxCount) = 0;

    // Optional: request a fresh sync from whatever source this provider
    // uses (BLE advertise-and-accept, for example). Blocks up to
    // `timeoutMs`. Returns true if a successful push arrived during the
    // window. Default implementation returns false (not supported) — the
    // sim and stub providers leave it that way. The Watchy BLE provider
    // overrides it.
    virtual bool syncNow(uint32_t /*timeoutMs*/) { return false; }
};

} // namespace wmt
