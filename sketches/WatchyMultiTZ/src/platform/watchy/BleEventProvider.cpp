// BleEventProvider — GATT-driven event ingest + persistent ring buffer.
// Protocol and storage layout are documented in BleEventProvider.h; this
// file just implements the state machine.
//
// Threading: BLECharacteristicCallbacks fire on the BLE stack's FreeRTOS
// task. Rather than reach into private members from that task (the header
// keeps the handler private), the callback copies the 64-byte packet into
// a small lock-protected queue. The syncNow() wait loop drains that queue
// on its own task, where it has regular member access. This also keeps
// the callback short and side-effect-free, which the NimBLE docs recommend.

#include "BleEventProvider.h"
#include "WatchyClock.h"
#include "../../face/DriftTracker.h"

#include <Arduino.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>

namespace wmt {

// ---------- Persistent storage (survives deep sleep; NVS for battery pull).
static RTC_DATA_ATTR Event   g_ring[BleEventProvider::RING_CAPACITY];
static RTC_DATA_ATTR uint8_t g_ringCount = 0;
static RTC_DATA_ATTR int64_t g_lastSyncUtc = 0;

// One-shot latch so repeated construction in the same boot doesn't clobber
// live in-RAM state with stale NVS bytes.
static bool g_nvsLoaded = false;

static constexpr const char *NVS_NS = "wmt_events";

// Singleton — BLECharacteristicCallbacks don't carry user data.
BleEventProvider *BleEventProvider::s_instance = nullptr;

// STATE characteristic, valid only for the duration of a syncNow() window.
static BLECharacteristic *s_stateChar = nullptr;

// Tracks whether a peer is currently connected. Maintained by ServerCb
// callbacks; read by the syncNow() wait loop to keep BLE alive while a
// push is in flight.
static volatile bool s_clientConnected = false;

// ---------- Packet queue (callback task → sync task) -----------------------

static constexpr int QUEUE_CAP = 16;  // headroom for TIME_SYNC + 10×EVENT + BATCH_END
struct PacketSlot {
    uint8_t data[64];
    uint8_t len;
};
static PacketSlot    s_queue[QUEUE_CAP];
static volatile int  s_qHead = 0;    // write index
static volatile int  s_qTail = 0;    // read index
static portMUX_TYPE  s_qMux = portMUX_INITIALIZER_UNLOCKED;

static void queueReset() {
    portENTER_CRITICAL(&s_qMux);
    s_qHead = 0;
    s_qTail = 0;
    portEXIT_CRITICAL(&s_qMux);
}

static void queuePush(const uint8_t *data, size_t len) {
    if (len == 0 || len > 64) return;
    portENTER_CRITICAL(&s_qMux);
    int next = (s_qHead + 1) % QUEUE_CAP;
    if (next != s_qTail) {
        memcpy(s_queue[s_qHead].data, data, len);
        s_queue[s_qHead].len = (uint8_t)len;
        s_qHead = next;
    }
    portEXIT_CRITICAL(&s_qMux);
}

static bool queuePop(uint8_t out[64], uint8_t &outLen) {
    bool got = false;
    portENTER_CRITICAL(&s_qMux);
    if (s_qTail != s_qHead) {
        memcpy(out, s_queue[s_qTail].data, s_queue[s_qTail].len);
        outLen = s_queue[s_qTail].len;
        s_qTail = (s_qTail + 1) % QUEUE_CAP;
        got = true;
    }
    portEXIT_CRITICAL(&s_qMux);
    return got;
}

// ---------- Little-endian codec helpers ------------------------------------

static inline int64_t readI64LE(const uint8_t *p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= (uint64_t)p[i] << (i * 8);
    return (int64_t)v;
}

static inline int32_t readI32LE(const uint8_t *p) {
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= (uint32_t)p[i] << (i * 8);
    return (int32_t)v;
}

static inline void writeI64LE(uint8_t *p, int64_t v) {
    uint64_t u = (uint64_t)v;
    for (int i = 0; i < 8; ++i) p[i] = (uint8_t)(u >> (i * 8));
}

static inline void writeU16LE(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
}

static void packStateInto(uint8_t out[16]) {
    memset(out, 0, 16);
    writeI64LE(out + 0, g_lastSyncUtc);
    writeU16LE(out + 8, (uint16_t)g_ringCount);
    out[10] = 0x02;  // schemaVer — bumped because of batteryPct in [11]
    out[11] = BleEventProvider::s_instance
                  ? BleEventProvider::s_instance->batteryPercent()
                  : 0;
}

// ---------- BLE write callback ---------------------------------------------

namespace {
class WriteCb : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *ch) override {
        std::string s = ch->getValue();
        queuePush(reinterpret_cast<const uint8_t *>(s.data()), s.length());
    }
};
} // namespace

// ---------- NVS -------------------------------------------------------------

void BleEventProvider::loadFromNvs() {
    if (g_nvsLoaded) return;
    g_nvsLoaded = true;

    Preferences p;
    if (!p.begin(NVS_NS, /*readOnly=*/true)) return;
    g_ringCount   = p.getUChar("count", 0);
    p.getBytes("ring", g_ring, sizeof g_ring);
    g_lastSyncUtc = p.getLong64("lastSync", 0);
    p.end();
    if (g_ringCount > RING_CAPACITY) g_ringCount = 0;  // corrupt → reset
}

void BleEventProvider::saveToNvs() {
    Preferences p;
    if (!p.begin(NVS_NS, /*readOnly=*/false)) return;
    p.putUChar("count", g_ringCount);
    p.putBytes("ring", g_ring, sizeof g_ring);
    p.putLong64("lastSync", g_lastSyncUtc);
    p.end();
}

// ---------- ctor + accessors -----------------------------------------------

BleEventProvider::BleEventProvider(WatchyClock *clock, DriftTracker *drift, IPower *power)
    : clock_(clock), drift_(drift), power_(power) {
    s_instance = this;
    loadFromNvs();
}

int64_t  BleEventProvider::lastSyncUtc() const { return g_lastSyncUtc; }
uint16_t BleEventProvider::eventCount() const  { return (uint16_t)g_ringCount; }

uint8_t BleEventProvider::batteryPercent() const {
    if (!power_) return 0;
    const float v = power_->batteryVoltage();
    constexpr float V_EMPTY = 3.30f;
    constexpr float V_FULL  = 4.20f;
    float frac = (v - V_EMPTY) / (V_FULL - V_EMPTY);
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    return (uint8_t)(frac * 100.0f + 0.5f);
}

// ---------- IEventProvider -------------------------------------------------

int BleEventProvider::nextEvents(int64_t fromUtc, Event *out, int maxCount) {
    if (!out || maxCount <= 0) return 0;
    int n = 0;

    for (int i = 0; i < (int)g_ringCount && n < maxCount; ++i) {
        if (g_ring[i].endUtc <= fromUtc) continue;
        int pos = n;
        while (pos > 0 && out[pos - 1].startUtc > g_ring[i].startUtc) {
            out[pos] = out[pos - 1];
            --pos;
        }
        out[pos] = g_ring[i];
        ++n;
    }
    return n;
}

bool BleEventProvider::syncNow(uint32_t timeoutMs, IButtons *abortOn) {
    batchArrived_ = false;
    accumCount_   = 0;
    queueReset();

    BLEDevice::init(ADV_NAME);
    BLEServer *server = BLEDevice::createServer();

    // Track peer connection state so syncNow()'s wait loop doesn't tear
    // down BLE mid-transaction. Auto-resume advertising on disconnect
    // (bluedroid does not, by default), so the harness's reconnect case
    // and real-world reconnects work.
    class ServerCb : public BLEServerCallbacks {
        void onConnect(BLEServer *) override {
            s_clientConnected = true;
        }
        void onDisconnect(BLEServer *s) override {
            s_clientConnected = false;
            s->getAdvertising()->start();
        }
    };
    server->setCallbacks(new ServerCb{});

    BLEService *svc   = server->createService(SERVICE_UUID);

    BLECharacteristic *wr = svc->createCharacteristic(
        CHAR_WRITE_UUID, BLECharacteristic::PROPERTY_WRITE);
    wr->setCallbacks(new WriteCb{});

    BLECharacteristic *st = svc->createCharacteristic(
        CHAR_STATE_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    st->addDescriptor(new BLE2902());

    s_stateChar = st;
    {
        uint8_t blob[16];
        packStateInto(blob);
        st->setValue(blob, sizeof blob);
    }

    svc->start();
    BLEAdvertising *adv = server->getAdvertising();
    adv->addServiceUUID(SERVICE_UUID);
    adv->start();

    // Advertise for the full window, even after a batch commits — the peer
    // may push multiple batches in one session (e.g. test harness running
    // several cases, or a phone re-syncing). `batchArrived_` is sticky: we
    // return true at the end if any batch committed during the window.
    // Wait loop. Exit conditions, in priority order:
    //   1. Any button pressed → immediate abort.
    //   2. A batch has committed AND no further packets arrived for
    //      POST_COMMIT_IDLE_MS → phone's done pushing, we can close.
    //   3. timeoutMs elapsed AND no peer currently connected.
    //
    // The idle-after-commit exit is what prevents "stuck in Syncing BLE":
    // Gadgetbridge happily keeps the GATT connection alive indefinitely
    // after its last write, so without this the loop would never end.
    constexpr unsigned long POST_COMMIT_IDLE_MS = 2000;
    const unsigned long start = millis();
    unsigned long lastPacketMs = 0;
    bool everCommitted = false;
    while (true) {
        uint8_t buf[64];
        uint8_t len = 0;
        bool gotAny = false;
        while (queuePop(buf, len)) {
            onWritePacket(buf, len);
            gotAny = true;
        }
        if (gotAny) lastPacketMs = millis();
        if (batchArrived_) {
            everCommitted = true;
            batchArrived_ = false;
            lastPacketMs = millis();
        }
        if (abortOn && (abortOn->isPressed(Button::Back) ||
                        abortOn->isPressed(Button::Menu) ||
                        abortOn->isPressed(Button::Up)   ||
                        abortOn->isPressed(Button::Down))) {
            break;
        }
        const unsigned long now = millis();
        const bool idleAfterCommit =
            everCommitted && (now - lastPacketMs) >= POST_COMMIT_IDLE_MS;
        if (idleAfterCommit) break;
        const bool timedOut = (now - start) >= timeoutMs;
        if (timedOut && !s_clientConnected) break;
        delay(50);
    }

    adv->stop();
    // Release the BLE stack so the Watchy OTA path can re-init it later.
    BLEDevice::deinit(true);
    s_stateChar = nullptr;

    return everCommitted;
}

// ---------- Packet dispatch -------------------------------------------------

void BleEventProvider::onWritePacket(const uint8_t *data, size_t len) {
    Serial.printf("[BLE] tag=0x%02x len=%u\n",
                  (len > 0 ? data[0] : 0), (unsigned)len);
    if (len != 64 || data == nullptr) return;

    switch (data[0]) {
    case TAG_TIME_SYNC: {
        // TIME_SYNC starts a new session. Discard any events accumulated
        // from a prior session that never reached BATCH_END (e.g. the peer
        // disconnected mid-batch).
        accumCount_ = 0;
        int64_t currentUtc = readI64LE(data + 8);
        int32_t gmtOffset  = readI32LE(data + 16);
        if (clock_) {
            int64_t preRaw = clock_->rawRtcUtc();
            clock_->setUtc(currentUtc);
            if (drift_) drift_->onSample(preRaw, currentUtc, gmtOffset);
        }
        break;
    }
    case TAG_EVENT: {
        if (accumCount_ >= RING_CAPACITY) return;  // silent drop
        Event &e = accum_[accumCount_];
        e.flags    = data[1];
        e._pad     = 0;
        e.startUtc = readI64LE(data + 2);
        e.endUtc   = readI64LE(data + 10);
        memcpy(e.title, data + 18, EVENT_TITLE_MAX);
        e.title[EVENT_TITLE_MAX - 1] = '\0';
        ++accumCount_;
        break;
    }
    case TAG_BATCH_END:
        commitBatch();
        break;
    case TAG_CLEAR:
        clearRing();
        break;
    default:
        Serial.printf("[BLE] unknown tag 0x%02x\n", data[0]);
        break;
    }
}

void BleEventProvider::commitBatch() {
    if (accumCount_ > 0) {
        memcpy(g_ring, accum_, sizeof(Event) * accumCount_);
        g_ringCount = accumCount_;
        accumCount_ = 0;
    }
    g_lastSyncUtc = clock_ ? clock_->nowUtc() : (int64_t)0;
    saveToNvs();
    batchArrived_ = true;
    publishState();
    Serial.printf("[BLE] commit: count=%u lastSync=%lld\n",
                  (unsigned)g_ringCount, (long long)g_lastSyncUtc);
}

void BleEventProvider::clearRing() {
    accumCount_ = 0;
    g_ringCount = 0;
    memset(g_ring, 0, sizeof g_ring);
    saveToNvs();
    publishState();
}

void BleEventProvider::publishState() {
    if (!s_stateChar) return;
    uint8_t blob[16];
    packStateInto(blob);
    s_stateChar->setValue(blob, sizeof blob);
    s_stateChar->notify();
}

} // namespace wmt
