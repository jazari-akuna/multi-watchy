#pragma once
// Persistent key/value storage that survives deep-sleep AND battery pull.
//
// On Watchy this wraps `Preferences` (NVS flash, 20 KB partition at 0x9000,
// 100k+ write endurance per key with wear-levelling across the partition).
// On the sim it's an in-memory std::map.
//
// Namespace convention: the caller picks a short namespace string once at
// init time; all subsequent get/put calls are scoped to that namespace.
// DriftTracker uses namespace "wmt_drift".
//
// Values are blobs of bytes. Caller packs/unpacks integers; this HAL does
// not try to be a typed KV store — byte-level portability matters more
// than type safety at this scale.

#include "Types.h"
#include <stddef.h>

namespace wmt {

class IPersistentStorage {
public:
    virtual ~IPersistentStorage() = default;

    // Enter a namespace. Subsequent get/put operate inside it.
    // Returns true if the namespace is accessible.
    virtual bool begin(const char *ns) = 0;

    // Write `bytes` bytes from `src` under key `key`. Returns true on success.
    // `bytes == 0` is a valid way to clear a key.
    virtual bool putBytes(const char *key, const void *src, size_t bytes) = 0;

    // Read up to `maxBytes` bytes of key `key` into `dst`. Returns the number
    // of bytes actually read (0 if the key doesn't exist). Caller is expected
    // to size the buffer to the known record schema.
    virtual size_t getBytes(const char *key, void *dst, size_t maxBytes) = 0;

    // Size in bytes of the currently-stored value for `key`, or 0 if absent.
    // Useful for var-length blobs; for fixed-size records you can ignore it.
    virtual size_t length(const char *key) = 0;

    // Remove a key. No-op (returns true) if the key is absent.
    virtual bool remove(const char *key) = 0;
};

} // namespace wmt
