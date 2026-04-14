#pragma once
// Placeholder until BLE-driven event ingest lands. Future: replace with
// BleEventProvider storing events in an RTC_DATA_ATTR ring buffer populated
// by a GATT write characteristic.

#include "../../hal/IEventProvider.h"

namespace wmt {

// Single-event stub. Holds one demo Event in-place (no heap). The sketch
// populates the start/end epochs at boot from IClock::nowUtc() + offsets,
// then nextEvents() returns that event as long as it hasn't ended yet.
class StubEventProvider final : public IEventProvider {
public:
    // Default offsets applied by setDemoDefault(): +2h start, +3h end.
    static constexpr int64_t DEFAULT_START_OFFSET_S = 2 * 60 * 60;
    static constexpr int64_t DEFAULT_END_OFFSET_S   = 3 * 60 * 60;
    static constexpr const char *DEFAULT_TITLE = "Next event displayed here";

    StubEventProvider();

    // Set the single demo event explicitly.
    void setDemo(const char *title, int64_t startUtc, int64_t endUtc);

    // Convenience: title = DEFAULT_TITLE,
    // start = nowUtc + DEFAULT_START_OFFSET_S,
    // end   = nowUtc + DEFAULT_END_OFFSET_S.
    void setDemoDefault(int64_t nowUtc);

    int nextEvents(int64_t fromUtc, Event *out, int maxCount) override;

private:
    Event demoEvent_{};
};

} // namespace wmt
