// SimPower — host-backed IPower. Clock is steady_clock so the elapsed-ms
// counter is monotonic even if the wall clock is adjusted.

#include "SimPower.h"

#include <chrono>
#include <thread>

namespace wmt {

SimPower::SimPower()
    : start_(std::chrono::steady_clock::now()) {}

void SimPower::delayMs(uint32_t ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

uint32_t SimPower::millisNow() {
    using namespace std::chrono;
    const auto dt = steady_clock::now() - start_;
    return (uint32_t)duration_cast<milliseconds>(dt).count();
}

} // namespace wmt
