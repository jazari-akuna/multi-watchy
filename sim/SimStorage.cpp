// SimStorage — in-memory IPersistentStorage for the desktop simulator.
// See header for rationale.

#include "SimStorage.h"

#include <cstring>

namespace wmt {

bool SimStorage::begin(const char *ns) {
    ns_ = ns ? ns : "";
    return true;
}

bool SimStorage::putBytes(const char *key, const void *src, size_t bytes) {
    if (!key) return false;
    auto &blob = kv_[key];
    blob.resize(bytes);
    if (bytes && src) std::memcpy(blob.data(), src, bytes);
    return true;
}

size_t SimStorage::getBytes(const char *key, void *dst, size_t maxBytes) {
    if (!key) return 0;
    auto it = kv_.find(key);
    if (it == kv_.end()) return 0;
    const size_t n = (it->second.size() < maxBytes) ? it->second.size() : maxBytes;
    if (n && dst) std::memcpy(dst, it->second.data(), n);
    return n;
}

size_t SimStorage::length(const char *key) {
    if (!key) return 0;
    auto it = kv_.find(key);
    return (it == kv_.end()) ? 0 : it->second.size();
}

bool SimStorage::remove(const char *key) {
    if (!key) return true;
    kv_.erase(key);
    return true;
}

} // namespace wmt
