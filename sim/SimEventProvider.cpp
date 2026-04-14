// SimEventProvider — fixed-capacity event store for the sim.
//
// nextEvents():
//   * filters to endUtc > fromUtc (i.e. upcoming OR currently in progress)
//   * copies matches into `out` sorted ascending by startUtc
//   * returns the number of entries written (≤ maxCount)
//
// We sort via an in-place insertion sort on the output buffer — N is tiny
// (CAPACITY == 8) so anything fancier would just be noise.

#include "SimEventProvider.h"

#include <string.h>

namespace wmt {

bool SimEventProvider::add(const Event &e) {
    if (count_ >= CAPACITY) return false;
    events_[count_++] = e;
    return true;
}

bool SimEventProvider::add(const char *title, int64_t startUtc, int64_t endUtc) {
    if (count_ >= CAPACITY) return false;
    Event &e = events_[count_++];
    // Null-safe copy, always NUL-terminated.
    if (title) {
        size_t i = 0;
        for (; i < EVENT_TITLE_MAX - 1 && title[i]; ++i) e.title[i] = title[i];
        e.title[i] = '\0';
    } else {
        e.title[0] = '\0';
    }
    e.flags    = 0;
    e._pad     = 0;
    e.startUtc = startUtc;
    e.endUtc   = endUtc;
    return true;
}

int SimEventProvider::nextEvents(int64_t fromUtc, Event *out, int maxCount) {
    if (!out || maxCount <= 0) return 0;
    int n = 0;

    for (int i = 0; i < count_ && n < maxCount; ++i) {
        // Include ongoing events — anything whose endUtc is still ahead of
        // `fromUtc` is eligible. The EventCard uses this to detect "in-event"
        // inversion, so ongoing events must be returned to the caller.
        if (events_[i].endUtc <= fromUtc) continue;

        // Insertion-sort position within [0..n) by startUtc ascending.
        int pos = n;
        while (pos > 0 && out[pos - 1].startUtc > events_[i].startUtc) {
            out[pos] = out[pos - 1];
            --pos;
        }
        out[pos] = events_[i];
        ++n;
    }
    return n;
}

} // namespace wmt
