#include "WatchyButtons.h"

#include <Watchy.h>           // pulls in config.h (button pin + mask macros)
#include <esp_sleep.h>

namespace wmt {

WatchyButtons::WatchyButtons() {
    pinMode(MENU_BTN_PIN, INPUT);
    pinMode(BACK_BTN_PIN, INPUT);
    pinMode(UP_BTN_PIN,   INPUT);
    pinMode(DOWN_BTN_PIN, INPUT);
}

Button WatchyButtons::wakeButton() {
    const uint64_t mask = esp_sleep_get_ext1_wakeup_status();
    if (mask & MENU_BTN_MASK) return Button::Menu;
    if (mask & BACK_BTN_MASK) return Button::Back;
    if (mask & UP_BTN_MASK)   return Button::Up;
    if (mask & DOWN_BTN_MASK) return Button::Down;
    return Button::None;
}

bool WatchyButtons::isPressed(Button b) {
    int pin;
    switch (b) {
        case Button::Menu: pin = MENU_BTN_PIN; break;
        case Button::Back: pin = BACK_BTN_PIN; break;
        case Button::Up:   pin = UP_BTN_PIN;   break;
        case Button::Down: pin = DOWN_BTN_PIN; break;
        default: return false;
    }
    // Watchy v1/v2: ACTIVE_LOW=1 in the lib's convention, i.e. "pressed reads HIGH".
    return digitalRead(pin) == HIGH;
}

} // namespace wmt
