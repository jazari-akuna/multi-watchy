#!/usr/bin/env python3
"""Generate an Adafruit_GFX-compatible C header from a TTF.

Covers codepoints 0x20..0xFF (Latin-1 supplement), so accented characters
like 'é', 'à', 'ç' render properly. Output matches the struct layout the
Watchy sketch already uses (GFXglyph / GFXfont in `_font_shim.h`).
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def rasterize(font_path: Path, size: int, first: int, last: int):
    """Return (bitmap_bytes: list[int], glyphs: list[(offset,w,h,xa,xo,yo)])"""
    font = ImageFont.truetype(str(font_path), size=size)
    ascent, descent = font.getmetrics()
    line_height = ascent + descent

    bitmap_bytes: list[int] = []
    glyphs: list[tuple[int, int, int, int, int, int]] = []

    for cp in range(first, last + 1):
        ch = chr(cp)
        # skip codepoints the font doesn't actually have (keep placeholder so indices align)
        # Rasterize to an L-mode image (8-bit), then threshold. PIL's
        # "1"-mode mask has a quirky raw layout across versions; doing
        # the thresholding ourselves is simpler and portable.
        img_mask = font.getmask(ch, mode="L")
        w, h = img_mask.size
        bbox = font.getbbox(ch) or (0, 0, 0, 0)
        x_advance = int(round(font.getlength(ch)))
        xo = bbox[0]
        yo = bbox[1] - ascent   # negative above baseline

        offset = len(bitmap_bytes)
        if w > 0 and h > 0:
            acc = 0
            nbits = 0
            for y in range(h):
                for x in range(w):
                    v = img_mask.getpixel((x, y))
                    bit = 1 if v >= 128 else 0
                    acc = (acc << 1) | bit
                    nbits += 1
                    if nbits == 8:
                        bitmap_bytes.append(acc)
                        acc = 0
                        nbits = 0
            if nbits:
                bitmap_bytes.append(acc << (8 - nbits))

        glyphs.append((offset, w, h, x_advance, xo, yo))

    return bitmap_bytes, glyphs, line_height


def emit(name: str, font_path: Path, size: int, first: int, last: int) -> str:
    bitmap_bytes, glyphs, line_height = rasterize(font_path, size, first, last)

    lines: list[str] = []
    lines.append(f"// Auto-generated from {font_path.name}, size {size}.")
    lines.append(f"// Range U+{first:04X}..U+{last:04X} (Latin-1 supplement).")
    lines.append(f"// Source: tools/gen_gfx_font.py")
    lines.append("#pragma once")
    lines.append('#include "_font_shim.h"')
    lines.append("")
    lines.append(f"static const uint8_t {name}Bitmaps[] PROGMEM = {{")
    # 12 per line
    for i in range(0, len(bitmap_bytes), 12):
        row = ", ".join(f"0x{b:02X}" for b in bitmap_bytes[i:i + 12])
        lines.append(f"    {row},")
    lines.append("};")
    lines.append("")
    lines.append(f"static const GFXglyph {name}Glyphs[] PROGMEM = {{")
    for (off, w, h, xa, xo, yo) in glyphs:
        lines.append(
            f"    {{ {off:5d}, {w:3d}, {h:3d}, {xa:3d}, {xo:4d}, {yo:4d} }},"
        )
    lines.append("};")
    lines.append("")
    lines.append(
        f"const GFXfont {name} PROGMEM = {{"
        f" (uint8_t  *){name}Bitmaps,"
        f" (GFXglyph *){name}Glyphs,"
        f" 0x{first:02X}, 0x{last:02X},"
        f" {line_height} }};"
    )
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ttf", required=True, type=Path)
    ap.add_argument("--size", type=int, required=True)
    ap.add_argument("--name", required=True)
    ap.add_argument("--first", type=lambda s: int(s, 0), default=0x20)
    ap.add_argument("--last", type=lambda s: int(s, 0), default=0xFF)
    ap.add_argument("--out", type=Path, required=True)
    args = ap.parse_args()

    if not args.ttf.exists():
        print(f"missing ttf: {args.ttf}", file=sys.stderr)
        return 1
    text = emit(args.name, args.ttf, args.size, args.first, args.last)
    args.out.write_text(text)
    bytes_ = args.out.stat().st_size
    print(f"wrote {args.out} ({bytes_} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
