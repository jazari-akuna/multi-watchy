#pragma once
// Desktop-simulator INetwork. The sim never connects — every method is a
// no-op returning `false`. NTP sync is a no-op because SimClock is already
// authoritative (driven by setFakeTime()).

#include "../sketches/WatchyMultiTZ/src/hal/INetwork.h"

namespace wmt {

class SimNetwork : public INetwork {
public:
    SimNetwork() = default;
    ~SimNetwork() override = default;

    bool connect() override                    { return false; }
    void disconnect() override                 {}
    bool syncNtp(const char * = nullptr) override { return false; }
};

} // namespace wmt
