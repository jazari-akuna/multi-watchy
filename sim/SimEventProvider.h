#pragma once
// Desktop-simulator IEventProvider.
//
// Stores a small fixed-capacity array of Events. nextEvents() returns those
// whose startUtc >= fromUtc, sorted ascending. The sim driver configures
// the scene (no events / one upcoming / one ongoing / two stacked) via the
// setters below.
//
// No heap, no std::vector — tiny fixed array.

#include "../sketches/WatchyMultiTZ/src/hal/IEventProvider.h"

#include <stdint.h>

namespace wmt {

class SimEventProvider : public IEventProvider {
public:
    static constexpr int CAPACITY = 8;

    SimEventProvider() = default;
    ~SimEventProvider() override = default;

    int nextEvents(int64_t fromUtc, Event *out, int maxCount) override;

    // Sim-only: scene configuration.
    void clear() { count_ = 0; }
    // Append one event. Returns false if the internal buffer is full.
    bool add(const Event &e);
    bool add(const char *title, int64_t startUtc, int64_t endUtc);

    int size() const { return count_; }

private:
    Event events_[CAPACITY];
    int   count_ = 0;
};

} // namespace wmt
