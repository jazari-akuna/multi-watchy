#pragma once
// BleEventProvider — Watchy-specific IEventProvider that receives events
// over a custom BLE GATT service and stores them in RTC slow memory (+
// NVS shadow) so they survive deep-sleep AND battery pull.
//
// GATT shape (v1; authoritative copy of the UUIDs — the Android driver
// and the Web-Bluetooth PWA must match these exactly):
//
//   Service    4e2d0001-6f00-4d1a-9c7b-8f7c2e0a1d3b
//   Char WRITE 4e2d0002-6f00-4d1a-9c7b-8f7c2e0a1d3b  (WRITE, 64 B/packet)
//   Char STATE 4e2d0003-6f00-4d1a-9c7b-8f7c2e0a1d3b  (READ + NOTIFY, 16 B)
//
// Write-packet layout (64 B exactly; byte offsets, LE ints):
//
//   [0] tag:
//        0x01 TIME_SYNC   [8..15]=currentUtc int64, [16..19]=gmtOffset int32
//        0x02 EVENT       [1]=flags, [2..9]=startUtc, [10..17]=endUtc,
//                         [18..63]=title (46 B UTF-8, null-padded)
//        0x03 BATCH_END   commits accumulator atomically
//        0x04 CLEAR       discards accumulator; empties the ring
//
// STATE read/notify layout (16 B):
//   [0..7]   lastSyncUtc    int64
//   [8..9]   eventCount     uint16
//   [10]     schemaVer      uint8 = 2 (was 1; bumped for batteryPct byte)
//   [11]     batteryPct     uint8 0..100 (schemaVer >= 2 only)
//   [12..15] reserved
//
// A single phone-initiated sync session looks like:
//   TIME_SYNC → EVENT × N → BATCH_END
// ATT guarantees in-order delivery on a single characteristic, so no
// sequence number is needed. Commit is atomic: only BATCH_END swaps the
// accumulator into the ring and persists to NVS.

#include "../../hal/IEventProvider.h"
#include "../../hal/IButtons.h"
#include "../../hal/IPower.h"
#include "../../hal/Types.h"

#include <stdint.h>
#include <stddef.h>

namespace wmt {

class WatchyClock;
class DriftTracker;

class BleEventProvider final : public IEventProvider {
public:
    // Capacity of the persistent ring buffer. 10 × 64 B = 640 B in RTC
    // slow memory. Matches the BLE packet size on purpose.
    static constexpr int RING_CAPACITY = 10;

    // Advertised device name (used by the Gadgetbridge coordinator for
    // human-readable discovery; scan-filter is by service UUID).
    static constexpr const char *ADV_NAME = "Watchy-WMT";

    // Service / characteristic UUIDs (string form).
    static constexpr const char *SERVICE_UUID = "4e2d0001-6f00-4d1a-9c7b-8f7c2e0a1d3b";
    static constexpr const char *CHAR_WRITE_UUID = "4e2d0002-6f00-4d1a-9c7b-8f7c2e0a1d3b";
    static constexpr const char *CHAR_STATE_UUID = "4e2d0003-6f00-4d1a-9c7b-8f7c2e0a1d3b";

    // Packet tags.
    static constexpr uint8_t TAG_TIME_SYNC = 0x01;
    static constexpr uint8_t TAG_EVENT     = 0x02;
    static constexpr uint8_t TAG_BATCH_END = 0x03;
    static constexpr uint8_t TAG_CLEAR     = 0x04;

    // Construct. `clock` and `drift` may be nullptr — time-sync packets
    // are then silently dropped (useful for minimal bring-up). `power` may
    // also be nullptr — STATE char then reports battery = 0 (unknown).
    BleEventProvider(WatchyClock *clock, DriftTracker *drift, IPower *power = nullptr);

    // IEventProvider interface ---------------------------------------------
    int  nextEvents(int64_t fromUtc, Event *out, int maxCount) override;

    // Starts BLE advertising and blocks up to `timeoutMs` waiting for a
    // successful phone-initiated sync (BATCH_END received). Returns true
    // if a batch committed during the window. Turns BLE off on return so
    // the library's OTA path can still open BLE later without conflict.
    bool syncNow(uint32_t timeoutMs, IButtons *abortOn = nullptr) override;

    // Accessors (used by tests + the STATE characteristic packer).
    int64_t lastSyncUtc() const;
    uint16_t eventCount() const;
    uint8_t  batteryPercent() const;   // 0..100; 0 if power HAL absent

    // Singleton, used by BLE callbacks (see `s_instance` comment below).
    // Exposed public so file-static helpers in the .cpp can reach accessors.
    static BleEventProvider *s_instance;

private:
    // GATT event hooks live as static free functions in the .cpp — the
    // BLE server callbacks require C-style function pointers (well,
    // subclasses of BLECharacteristicCallbacks). We route them back into
    // a singleton instance via s_instance.

    void onWritePacket(const uint8_t *data, size_t len);
    void commitBatch();
    void clearRing();
    void loadFromNvs();
    void saveToNvs();
    void publishState();   // write STATE char + notify subscribers

    WatchyClock  *clock_;
    DriftTracker *drift_;
    IPower       *power_;

    // Accumulator for in-flight batch (not persisted).
    Event   accum_[RING_CAPACITY];
    uint8_t accumCount_ = 0;

    // "A batch was committed" flag, polled by syncNow()'s wait loop.
    volatile bool batchArrived_ = false;
};

} // namespace wmt
