#pragma once
// Watchy-specific IPersistentStorage. Wraps Arduino-ESP32 `Preferences`
// (NVS flash, 20 KB partition at 0x9000, 100k+ write endurance per key with
// wear-levelling across the partition).
//
// Each operation opens/closes Preferences on demand within the namespace
// captured at begin(); we never hold the flash open across sleep cycles.

#include "../../hal/IPersistentStorage.h"

namespace wmt {

// IPersistentStorage impl that lazily opens NVS within a stored namespace.
class WatchyStorage final : public IPersistentStorage {
public:
    WatchyStorage() = default;

    bool   begin(const char *ns) override;
    bool   putBytes(const char *key, const void *src, size_t bytes) override;
    size_t getBytes(const char *key, void *dst, size_t maxBytes) override;
    size_t length(const char *key) override;
    bool   remove(const char *key) override;

private:
    // Preferences::begin() copies the namespace string internally for its
    // own bookkeeping, so retaining a const char* here is safe as long as
    // the caller's string outlives this object (typical: a string literal).
    const char *ns_ = nullptr;
};

} // namespace wmt
