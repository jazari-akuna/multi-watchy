// Placeholder until BLE-driven event ingest lands. Future: replace with
// BleEventProvider storing events in an RTC_DATA_ATTR ring buffer populated
// by a GATT write characteristic.

#include "StubEventProvider.h"

#include <string.h>

namespace wmt {

static void copyTitle(char *dst, const char *src) {
    if (src == nullptr) { dst[0] = '\0'; return; }
    int i = 0;
    for (; i < EVENT_TITLE_MAX - 1 && src[i] != '\0'; ++i) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

StubEventProvider::StubEventProvider() {
    copyTitle(demoEvent_.title, DEFAULT_TITLE);
    demoEvent_.flags    = 0;
    demoEvent_._pad     = 0;
    demoEvent_.startUtc = 0;
    demoEvent_.endUtc   = 0;
}

void StubEventProvider::setDemo(const char *title, int64_t startUtc, int64_t endUtc) {
    copyTitle(demoEvent_.title, title);
    demoEvent_.startUtc = startUtc;
    demoEvent_.endUtc   = endUtc;
}

void StubEventProvider::setDemoDefault(int64_t nowUtc) {
    copyTitle(demoEvent_.title, DEFAULT_TITLE);
    demoEvent_.startUtc = nowUtc + DEFAULT_START_OFFSET_S;
    demoEvent_.endUtc   = nowUtc + DEFAULT_END_OFFSET_S;
}

int StubEventProvider::nextEvents(int64_t fromUtc, Event *out, int maxCount) {
    if (maxCount < 1 || out == nullptr) return 0;
    if (demoEvent_.endUtc <= fromUtc)   return 0;
    out[0] = demoEvent_;
    return 1;
}

} // namespace wmt
