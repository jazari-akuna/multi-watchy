#pragma once
#include "_font_shim.h"

// MyCompactFont7pt — a tiny 4×7 bitmap font.
//
// Hand-designed "in-between" font for TUE / APR labels and the +Nd
// badge on the MultiTZ watchface: cap-height 7 px, glyph width 4 px
// (xAdvance 5 px). Larger than Picopixel (5-px cap), smaller than
// FreeSans9pt7b (~10-px cap).
//
// Covers uppercase A..Z, digits 0..9, and '+', '-', ':', '/', '.'.
// Lowercase is intentionally omitted.
//
// Glyph bitmap format follows Adafruit-GFX: row-major, MSB-first, TIGHTLY
// packed across rows AND across glyphs (a glyph's trailing bits share a
// byte with the next glyph's leading bits). The glyph record specifies
// width/height in pixels and `bitmapOffset` in BYTES — but the bit-index
// within that byte is `(bitmapOffset continues from previous glyph)`
// isn't quite right; actually the renderer reads `bitmap + bitmapOffset`
// as the start byte and consumes `width*height` bits from bit 7 of that
// first byte. So each glyph starts on a byte boundary but may leave
// trailing padding bits in its last byte.
//
// Each 4×7 glyph = 28 bits = 4 bytes (with 4 trailing padding bits).

// Helper macro to encode a 4×7 glyph as a bitstream of 28 bits = 4 bytes
// (with 4 trailing padding bits in byte 3).  Rows are supplied as their
// top-nibble (e.g. 0xF0 for 1111_0000). Two rows pack into one byte:
// first row in the high nibble, second row in the low nibble.
//
// byte[0] = r0[7..4]  r1[7..4]
// byte[1] = r2[7..4]  r3[7..4]
// byte[2] = r4[7..4]  r5[7..4]
// byte[3] = r6[7..4]  0000   (3 trailing padding bits not read)
//
// We emit 7 bytes per glyph (rather than the tight 4) so every glyph
// starts on a 7-byte boundary in the bitmap pool and bitmapOffset values
// are obvious/verifiable — the renderer only reads width*height = 28
// bits from the start byte, so the extra 3 bytes of slack are harmless.
#define GLYPH4x7(r0,r1,r2,r3,r4,r5,r6)                                         \
    (uint8_t)(((r0) & 0xF0) | (((r1) >> 4) & 0x0F)),                           \
    (uint8_t)(((r2) & 0xF0) | (((r3) >> 4) & 0x0F)),                           \
    (uint8_t)(((r4) & 0xF0) | (((r5) >> 4) & 0x0F)),                           \
    (uint8_t)((r6) & 0xF0),                                                    \
    (uint8_t)0, (uint8_t)0, (uint8_t)0

const uint8_t MyCompactFont7pt_Bitmaps[] PROGMEM = {
    // 0x2B '+'                off=0
    GLYPH4x7(0x00,0x40,0x40,0xE0,0x40,0x40,0x00),
    // 0x2D '-'                off=7
    GLYPH4x7(0x00,0x00,0x00,0xE0,0x00,0x00,0x00),
    // 0x2E '.'                off=14
    GLYPH4x7(0x00,0x00,0x00,0x00,0x00,0x00,0x40),
    // 0x2F '/'                off=21
    GLYPH4x7(0x10,0x10,0x20,0x20,0x40,0x80,0x80),
    // 0x30 '0'                off=28
    GLYPH4x7(0x60,0x90,0x90,0x90,0x90,0x90,0x60),
    // 0x31 '1'                off=35
    GLYPH4x7(0x40,0xC0,0x40,0x40,0x40,0x40,0xE0),
    // 0x32 '2'                off=42
    GLYPH4x7(0x60,0x90,0x10,0x20,0x40,0x80,0xF0),
    // 0x33 '3'                off=49
    GLYPH4x7(0xE0,0x10,0x10,0x60,0x10,0x10,0xE0),
    // 0x34 '4'                off=56
    GLYPH4x7(0x10,0x30,0x50,0x90,0xF0,0x10,0x10),
    // 0x35 '5'                off=63
    GLYPH4x7(0xF0,0x80,0xE0,0x10,0x10,0x90,0x60),
    // 0x36 '6'                off=70
    GLYPH4x7(0x60,0x80,0x80,0xE0,0x90,0x90,0x60),
    // 0x37 '7'                off=77
    GLYPH4x7(0xF0,0x10,0x10,0x20,0x20,0x40,0x40),
    // 0x38 '8'                off=84
    GLYPH4x7(0x60,0x90,0x90,0x60,0x90,0x90,0x60),
    // 0x39 '9'                off=91
    GLYPH4x7(0x60,0x90,0x90,0x70,0x10,0x10,0x60),
    // 0x3A ':'                off=98
    GLYPH4x7(0x00,0x40,0x00,0x00,0x00,0x40,0x00),
    // 0x41 'A'                off=105
    GLYPH4x7(0x60,0x90,0x90,0x90,0xF0,0x90,0x90),
    // 0x42 'B'                off=112
    GLYPH4x7(0xE0,0x90,0x90,0xE0,0x90,0x90,0xE0),
    // 0x43 'C'                off=119
    GLYPH4x7(0x60,0x90,0x80,0x80,0x80,0x90,0x60),
    // 0x44 'D'                off=126
    GLYPH4x7(0xE0,0x90,0x90,0x90,0x90,0x90,0xE0),
    // 0x45 'E'                off=133
    GLYPH4x7(0xF0,0x80,0x80,0xE0,0x80,0x80,0xF0),
    // 0x46 'F'                off=140
    GLYPH4x7(0xF0,0x80,0x80,0xE0,0x80,0x80,0x80),
    // 0x47 'G'                off=147
    GLYPH4x7(0x60,0x90,0x80,0xB0,0x90,0x90,0x60),
    // 0x48 'H'                off=154
    GLYPH4x7(0x90,0x90,0x90,0xF0,0x90,0x90,0x90),
    // 0x49 'I'                off=161
    GLYPH4x7(0xE0,0x40,0x40,0x40,0x40,0x40,0xE0),
    // 0x4A 'J'                off=168
    GLYPH4x7(0x10,0x10,0x10,0x10,0x10,0x90,0x60),
    // 0x4B 'K'                off=175
    GLYPH4x7(0x90,0x90,0xA0,0xC0,0xA0,0x90,0x90),
    // 0x4C 'L'                off=182
    GLYPH4x7(0x80,0x80,0x80,0x80,0x80,0x80,0xF0),
    // 0x4D 'M'                off=189
    GLYPH4x7(0x90,0xF0,0xF0,0x90,0x90,0x90,0x90),
    // 0x4E 'N'                off=196
    GLYPH4x7(0x90,0xD0,0xD0,0xB0,0xB0,0x90,0x90),
    // 0x4F 'O'                off=203
    GLYPH4x7(0x60,0x90,0x90,0x90,0x90,0x90,0x60),
    // 0x50 'P'                off=210
    GLYPH4x7(0xE0,0x90,0x90,0xE0,0x80,0x80,0x80),
    // 0x51 'Q'                off=217
    GLYPH4x7(0x60,0x90,0x90,0x90,0xB0,0xA0,0x70),
    // 0x52 'R'                off=224
    GLYPH4x7(0xE0,0x90,0x90,0xE0,0xA0,0x90,0x90),
    // 0x53 'S'                off=231
    GLYPH4x7(0x60,0x90,0x80,0x60,0x10,0x90,0x60),
    // 0x54 'T'                off=238
    GLYPH4x7(0xF0,0x40,0x40,0x40,0x40,0x40,0x40),
    // 0x55 'U'                off=245
    GLYPH4x7(0x90,0x90,0x90,0x90,0x90,0x90,0x60),
    // 0x56 'V'                off=252
    GLYPH4x7(0x90,0x90,0x90,0x90,0x90,0x60,0x60),
    // 0x57 'W'                off=259
    GLYPH4x7(0x90,0x90,0x90,0x90,0xF0,0xF0,0x90),
    // 0x58 'X'                off=266
    GLYPH4x7(0x90,0x90,0x90,0x60,0x90,0x90,0x90),
    // 0x59 'Y'                off=273
    GLYPH4x7(0x90,0x90,0x90,0x60,0x40,0x40,0x40),
    // 0x5A 'Z'                off=280
    GLYPH4x7(0xF0,0x10,0x20,0x40,0x80,0x80,0xF0),
};

// Glyph record: { bitmapOffset, width, height, xAdvance, xOffset, yOffset }
//   width=4, height=7, xAdvance=5 (4-px glyph + 1-px side bearing),
//   xOffset=0, yOffset=-7 (baseline at bottom of glyph → glyph occupies
//   y-7..y-1 when drawn at baseline y).
//
// Codepoints 0x20..0x5A are dense so we provide a record for every
// codepoint in-range. Missing glyphs use width=height=0 so they render
// as a blank advance (space-like).

const GFXglyph MyCompactFont7pt_Glyphs[] PROGMEM = {
    {  0, 0, 0, 4, 0,  0 },  // 0x20 ' '
    {  0, 0, 0, 4, 0,  0 },  // 0x21 '!'
    {  0, 0, 0, 4, 0,  0 },  // 0x22 '"'
    {  0, 0, 0, 4, 0,  0 },  // 0x23 '#'
    {  0, 0, 0, 4, 0,  0 },  // 0x24 '$'
    {  0, 0, 0, 4, 0,  0 },  // 0x25 '%'
    {  0, 0, 0, 4, 0,  0 },  // 0x26 '&'
    {  0, 0, 0, 4, 0,  0 },  // 0x27 '''
    {  0, 0, 0, 4, 0,  0 },  // 0x28 '('
    {  0, 0, 0, 4, 0,  0 },  // 0x29 ')'
    {  0, 0, 0, 4, 0,  0 },  // 0x2A '*'
    {  0, 4, 7, 5, 0, -7 },  // 0x2B '+'
    {  0, 0, 0, 4, 0,  0 },  // 0x2C ','
    {  7, 4, 7, 5, 0, -7 },  // 0x2D '-'
    { 14, 4, 7, 5, 0, -7 },  // 0x2E '.'
    { 21, 4, 7, 5, 0, -7 },  // 0x2F '/'
    { 28, 4, 7, 5, 0, -7 },  // 0x30 '0'
    { 35, 4, 7, 5, 0, -7 },  // 0x31 '1'
    { 42, 4, 7, 5, 0, -7 },  // 0x32 '2'
    { 49, 4, 7, 5, 0, -7 },  // 0x33 '3'
    { 56, 4, 7, 5, 0, -7 },  // 0x34 '4'
    { 63, 4, 7, 5, 0, -7 },  // 0x35 '5'
    { 70, 4, 7, 5, 0, -7 },  // 0x36 '6'
    { 77, 4, 7, 5, 0, -7 },  // 0x37 '7'
    { 84, 4, 7, 5, 0, -7 },  // 0x38 '8'
    { 91, 4, 7, 5, 0, -7 },  // 0x39 '9'
    { 98, 4, 7, 5, 0, -7 },  // 0x3A ':'
    {  0, 0, 0, 4, 0,  0 },  // 0x3B ';'
    {  0, 0, 0, 4, 0,  0 },  // 0x3C '<'
    {  0, 0, 0, 4, 0,  0 },  // 0x3D '='
    {  0, 0, 0, 4, 0,  0 },  // 0x3E '>'
    {  0, 0, 0, 4, 0,  0 },  // 0x3F '?'
    {  0, 0, 0, 4, 0,  0 },  // 0x40 '@'
    { 105, 4, 7, 5, 0, -7 },  // 0x41 'A'
    { 112, 4, 7, 5, 0, -7 },  // 0x42 'B'
    { 119, 4, 7, 5, 0, -7 },  // 0x43 'C'
    { 126, 4, 7, 5, 0, -7 },  // 0x44 'D'
    { 133, 4, 7, 5, 0, -7 },  // 0x45 'E'
    { 140, 4, 7, 5, 0, -7 },  // 0x46 'F'
    { 147, 4, 7, 5, 0, -7 },  // 0x47 'G'
    { 154, 4, 7, 5, 0, -7 },  // 0x48 'H'
    { 161, 4, 7, 5, 0, -7 },  // 0x49 'I'
    { 168, 4, 7, 5, 0, -7 },  // 0x4A 'J'
    { 175, 4, 7, 5, 0, -7 },  // 0x4B 'K'
    { 182, 4, 7, 5, 0, -7 },  // 0x4C 'L'
    { 189, 4, 7, 5, 0, -7 },  // 0x4D 'M'
    { 196, 4, 7, 5, 0, -7 },  // 0x4E 'N'
    { 203, 4, 7, 5, 0, -7 },  // 0x4F 'O'
    { 210, 4, 7, 5, 0, -7 },  // 0x50 'P'
    { 217, 4, 7, 5, 0, -7 },  // 0x51 'Q'
    { 224, 4, 7, 5, 0, -7 },  // 0x52 'R'
    { 231, 4, 7, 5, 0, -7 },  // 0x53 'S'
    { 238, 4, 7, 5, 0, -7 },  // 0x54 'T'
    { 245, 4, 7, 5, 0, -7 },  // 0x55 'U'
    { 252, 4, 7, 5, 0, -7 },  // 0x56 'V'
    { 259, 4, 7, 5, 0, -7 },  // 0x57 'W'
    { 266, 4, 7, 5, 0, -7 },  // 0x58 'X'
    { 273, 4, 7, 5, 0, -7 },  // 0x59 'Y'
    { 280, 4, 7, 5, 0, -7 },  // 0x5A 'Z'
};

const GFXfont MyCompactFont7pt PROGMEM = {
    (uint8_t *)MyCompactFont7pt_Bitmaps,
    (GFXglyph *)MyCompactFont7pt_Glyphs,
    0x20, 0x5A, 9  // first, last, yAdvance
};
