#pragma once
// Desktop-simulator IPersistentStorage. Backed by an in-memory std::map so
// DriftTracker::init() and setSyntheticState() can run without null guards.
//
// State does NOT survive between sim invocations — the sim is invoked
// per-frame, so each render starts from a clean slate. That's intentional:
// the sim drives state explicitly via setSyntheticState() rather than
// relying on persisted history.

#include "hal/IPersistentStorage.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace wmt {

class SimStorage final : public IPersistentStorage {
public:
    bool   begin(const char *ns) override;
    bool   putBytes(const char *key, const void *src, size_t bytes) override;
    size_t getBytes(const char *key, void *dst, size_t maxBytes) override;
    size_t length(const char *key) override;
    bool   remove(const char *key) override;

private:
    std::string ns_;
    std::map<std::string, std::vector<uint8_t>> kv_;   // key -> blob
};

} // namespace wmt
