#pragma once
#ifndef WMT_HAL_TYPES_H_
#define WMT_HAL_TYPES_H_
// Platform-agnostic value types shared by HAL interfaces and face code.
// Zero dependencies. 1-bit display semantics: `Ink` carries *logical*
// foreground / background. Each concrete IDisplay resolves the logical ink
// to its platform's colour constant (e.g. GxEPD_BLACK / GxEPD_WHITE). This
// lets a card draw itself "inverted" just by swapping the two inks at the
// top of its render method.

#include <stdint.h>

namespace wmt {

struct Rect {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    constexpr int16_t right()  const { return (int16_t)(x + w - 1); }
    constexpr int16_t bottom() const { return (int16_t)(y + h - 1); }
};

struct Point {
    int16_t x;
    int16_t y;
};

enum class Ink : uint8_t {
    Fg = 0,   // logical foreground (ink "on")
    Bg = 1,   // logical background (ink "off")
};

constexpr Ink invert(Ink i) { return i == Ink::Fg ? Ink::Bg : Ink::Fg; }

enum class Button : uint8_t {
    None = 0,
    Menu,
    Back,
    Up,
    Down,
};

// Calendar-style local datetime. Populated by IClock::toLocal().
struct LocalDateTime {
    int16_t year;   // 1970+
    int8_t  month;  // 1..12
    int8_t  day;    // 1..31
    int8_t  hour;   // 0..23
    int8_t  minute; // 0..59
    int8_t  second; // 0..59
    int8_t  wday;   // 0=Sun..6=Sat
};

// Maximum title length for a calendar/event entry — sized to fit comfortably
// on the 200-px event card with some padding.
static constexpr int EVENT_TITLE_MAX = 48;

struct Event {
    char    title[EVENT_TITLE_MAX];
    int64_t startUtc;  // Unix epoch seconds
    int64_t endUtc;
};

} // namespace wmt

#endif // WMT_HAL_TYPES_H_
