#pragma once
// Watchy-specific IClock. Reads/writes the hardware RTC via WatchyRTC and
// converts between local-time (as the Watchy lib stores it) and UTC.

#include "../../hal/IClock.h"

class WatchyRTC; // forward-decl from Watchy lib

namespace wmt {

// IClock impl that treats the Watchy RTC as "local-time at gmtOffset" and
// exposes everything to the face as UTC epochs.
class WatchyClock final : public IClock {
public:
    explicit WatchyClock(WatchyRTC &rtc) : rtc_(rtc) {}

    int64_t nowUtc() override;
    void    setUtc(int64_t epoch) override;
    LocalDateTime toLocal(int64_t utc, const char *posixTZ) override;

private:
    WatchyRTC &rtc_;
};

} // namespace wmt
