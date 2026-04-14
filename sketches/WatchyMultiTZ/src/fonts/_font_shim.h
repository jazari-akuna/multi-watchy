#pragma once
// Cross-platform shim to let the Adafruit-GFX-style font headers in this
// directory compile on BOTH the Arduino/Watchy target AND the desktop
// simulator.
//
//   - On the Arduino/Watchy build, `<Watchy.h>` transitively pulls in
//     `<Adafruit_GFX.h>` which defines PROGMEM, pgm_read_byte, and the
//     GFXfont / GFXglyph types. In that case this shim is a no-op (the
//     guards below all take the already-defined path).
//
//   - On the host/simulator build, none of those symbols exist. The shim
//     provides them.
//
// Include this header FIRST from any .cpp or .h that then includes one of
// the DSEG7 / Seven_Segment10pt7b font headers.

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

// Guard matches Adafruit_GFX's gfxfont.h (line 7) so including this shim
// after Adafruit_GFX.h is a no-op.
#ifndef _GFXFONT_H_
#define _GFXFONT_H_
typedef struct {
    uint16_t bitmapOffset;
    uint8_t  width;
    uint8_t  height;
    uint8_t  xAdvance;
    int8_t   xOffset;
    int8_t   yOffset;
} GFXglyph;

typedef struct {
    uint8_t  *bitmap;
    GFXglyph *glyph;
    uint16_t  first;
    uint16_t  last;
    uint8_t   yAdvance;
} GFXfont;
#endif // _GFXFONT_H_
