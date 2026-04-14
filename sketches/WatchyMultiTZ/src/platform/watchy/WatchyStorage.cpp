#include "WatchyStorage.h"

#include <Preferences.h>

namespace wmt {

bool WatchyStorage::begin(const char *ns) {
    // Just stash the namespace; opens happen lazily in put/get/length/remove
    // so we don't keep the NVS partition open across deep-sleep boundaries.
    if (!ns || !*ns) return false;
    ns_ = ns;
    return true;
}

bool WatchyStorage::putBytes(const char *key, const void *src, size_t bytes) {
    if (!ns_ || !key) return false;
    Preferences p;
    if (!p.begin(ns_, /*readOnly=*/false)) return false;
    size_t w = p.putBytes(key, src, bytes);
    p.end();
    return w == bytes;
}

size_t WatchyStorage::getBytes(const char *key, void *dst, size_t maxBytes) {
    if (!ns_ || !key) return 0;
    Preferences p;
    if (!p.begin(ns_, /*readOnly=*/true)) return 0;
    size_t r = p.getBytes(key, dst, maxBytes);
    p.end();
    return r;
}

size_t WatchyStorage::length(const char *key) {
    if (!ns_ || !key) return 0;
    Preferences p;
    if (!p.begin(ns_, /*readOnly=*/true)) return 0;
    size_t n = p.getBytesLength(key);
    p.end();
    return n;
}

bool WatchyStorage::remove(const char *key) {
    if (!ns_ || !key) return false;
    Preferences p;
    if (!p.begin(ns_, /*readOnly=*/false)) return false;
    bool ok = p.remove(key);
    p.end();
    return ok;
}

} // namespace wmt
