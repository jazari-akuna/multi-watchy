#pragma once
// ArduinoShim — tiny compatibility layer so the Adafruit-GFX-style font
// headers in sketches/WatchyMultiTZ/src/fonts/*.h compile cleanly on the
// desktop simulator.
//
// The font headers assume:
//   * `PROGMEM` is defined (on AVR it puts data in flash; on the sim it's a
//     no-op).
//   * `pgm_read_byte(p)` exists (on AVR it reads flash via special insns; on
//     the sim it's a plain dereference).
//   * `GFXglyph` and `GFXfont` are already declared (usually pulled in via
//     Adafruit_GFX's `gfxfont.h`). We declare them here since Adafruit_GFX
//     isn't available on the host.
//
// Include this BEFORE any font header.

#include <stdint.h>

#ifndef PROGMEM
#define PROGMEM
#endif

#ifndef pgm_read_byte
#define pgm_read_byte(p) (*(const uint8_t *)(p))
#endif

#ifndef pgm_read_word
#define pgm_read_word(p) (*(const uint16_t *)(p))
#endif

#ifndef pgm_read_pointer
#define pgm_read_pointer(p) ((void *)(*(void *const *)(p)))
#endif

// Adafruit_GFX GFXfont layout. Must match exactly so the existing font
// headers' aggregate initialisers bind correctly. Guard is the same one
// Adafruit_GFX's gfxfont.h uses, so if both headers are in the include
// graph only the first one wins.
#ifndef _GFXFONT_H_
#define _GFXFONT_H_
typedef struct {
    uint16_t bitmapOffset; // offset into GFXfont->bitmap
    uint8_t  width;        // bitmap dimensions in pixels
    uint8_t  height;
    uint8_t  xAdvance;     // distance to advance cursor (x axis)
    int8_t   xOffset;      // X dist from cursor pos to UL corner
    int8_t   yOffset;      // Y dist from cursor pos to UL corner (usu. neg)
} GFXglyph;

typedef struct {
    uint8_t  *bitmap;      // glyph bitmaps, concatenated
    GFXglyph *glyph;       // glyph array
    uint16_t  first;       // ASCII extents (first char)
    uint16_t  last;        // ASCII extents (last char)
    uint8_t   yAdvance;    // newline distance (y axis)
} GFXfont;
#endif // _GFXFONT_H_
