#pragma once
// Sim-only shim for Adafruit_GFX.h. The real Adafruit_GFX library isn't
// available on the host toolchain, but the font headers that ship under
// its Fonts/ subdirectory only need `GFXfont` / `GFXglyph` + the PROGMEM
// macros to compile. ArduinoShim.h provides both, so this header is just
// a redirect.
#include "ArduinoShim.h"
