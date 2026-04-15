#!/usr/bin/env python3
"""Regenerate sketches/WatchyMultiTZ/src/assets/qr_codes.h.

Reads QR images from tools/qr_sources/ (or tools/qr_sources_example/ as a
fallback on a fresh clone), decodes them via `zbarimg`, then re-encodes
every payload via `qrencode` at a SHARED QR version chosen so that:

  1. Every payload fits,
  2. Every payload can use at least ECC level Q (bumping the version
     if necessary — e.g. a payload that needs v4 ECC M won't get only
     M; we bump to v5 where it fits at Q).

Within that shared version each payload takes the HIGHEST ECC level that
still fits, so shorter payloads get extra scan robustness for free.
Sharing the version means every code renders at identical physical size
on the watchface.

Runs with system tools only (zbar-tools, qrencode, python3-pil) — no pip.
"""

from __future__ import annotations

import io
import shutil
import subprocess
import sys
from pathlib import Path

from PIL import Image

REPO        = Path(__file__).resolve().parent.parent
SRC_DIR     = REPO / "tools" / "qr_sources"
EXAMPLE_DIR = REPO / "tools" / "qr_sources_example"
OUT_H       = REPO / "sketches" / "WatchyMultiTZ" / "src" / "assets" / "qr_codes.h"

SUPPORTED_EXTS = {".png", ".jpg", ".jpeg", ".bmp", ".gif"}
ECC_LEVELS_BEST_FIRST = ("H", "Q", "M", "L")
MIN_SHARED_ECC        = "Q"   # every payload must fit at this or better


def decode(image_path: Path) -> str:
    result = subprocess.run(
        ["zbarimg", "-q", "--raw", str(image_path)],
        capture_output=True, text=True, check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(f"zbarimg failed on {image_path.name}: {result.stderr.strip()}")
    payload = result.stdout.rstrip("\n")
    if not payload:
        raise RuntimeError(f"no QR decoded in {image_path.name}")
    if payload.startswith("QR-Code:"):
        payload = payload[len("QR-Code:"):]
    return payload


def fits_at(payload: str, version: int, ecc: str) -> bool:
    """True iff qrencode accepts `payload` at exactly (version, ecc)."""
    cmd = ["qrencode", "-t", "PNG", "-s", "1", "-m", "0",
           "-l", ecc, "-v", str(version), "-o", "-", payload]
    result = subprocess.run(cmd, capture_output=True, check=False)
    if result.returncode != 0:
        return False
    img = Image.open(io.BytesIO(result.stdout)).convert("1")
    w, _ = img.size
    expected = 17 + 4 * version   # N = 17 + 4v
    # qrencode silently bumps the version when the payload doesn't fit.
    return w == expected


def encode_matrix(payload: str, version: int, ecc: str) -> list[list[bool]]:
    cmd = ["qrencode", "-t", "PNG", "-s", "1", "-m", "0",
           "-l", ecc, "-v", str(version), "-o", "-", payload]
    result = subprocess.run(cmd, capture_output=True, check=False)
    if result.returncode != 0:
        raise RuntimeError(f"qrencode failed at v{version} ECC {ecc}: "
                           f"{result.stderr.decode(errors='replace')}")
    img = Image.open(io.BytesIO(result.stdout)).convert("1")
    w, h = img.size
    if w != h or w != 17 + 4 * version:
        raise RuntimeError(f"qrencode bumped version — payload '{payload}' "
                           f"does not fit at v{version} ECC {ecc}")
    px = img.load()
    return [[px[x, y] == 0 for x in range(w)] for y in range(h)]


def pack_msb(matrix: list[list[bool]]) -> bytes:
    n = len(matrix)
    bytes_per_row = (n + 7) // 8
    out = bytearray()
    for row in matrix:
        for byte_i in range(bytes_per_row):
            b = 0
            for bit_i in range(8):
                col = byte_i * 8 + bit_i
                if col < n and row[col]:
                    b |= 0x80 >> bit_i
            out.append(b)
    return bytes(out)


def pick_shared_version(payloads: list[str], min_ecc: str) -> int:
    """Smallest QR version at which EVERY payload fits at `min_ecc` or better."""
    for v in range(1, 41):
        if all(fits_at(p, v, min_ecc) for p in payloads):
            return v
    raise RuntimeError(f"no version up to 40 fits all payloads at ECC {min_ecc}+")


def best_ecc_at(payload: str, version: int) -> str:
    """Highest ECC level at which `payload` fits at `version`."""
    for ecc in ECC_LEVELS_BEST_FIRST:
        if fits_at(payload, version, ecc):
            return ecc
    raise RuntimeError(f"no ECC level fits payload at v{version}")


def short_label(path: Path) -> str:
    stem = path.stem
    parts = stem.split("_", 1)
    return parts[1] if len(parts) == 2 and parts[0].isdigit() else stem


def select_source_dir() -> Path:
    """Prefer the user's private sources; fall back to committed examples."""
    user_imgs = sorted(p for p in SRC_DIR.iterdir()
                       if p.is_file() and p.suffix.lower() in SUPPORTED_EXTS) \
                if SRC_DIR.exists() else []
    if user_imgs:
        return SRC_DIR
    print(f"  (no images in {SRC_DIR.relative_to(REPO)}, "
          f"using {EXAMPLE_DIR.relative_to(REPO)})")
    return EXAMPLE_DIR


def emit_header(codes: list[tuple[str, str, int, str, bytes]]) -> str:
    """codes: list of (label, payload, version, ecc, packed_bytes)."""
    lines = [
        "// Generated by tools/gen_qr_codes.py — DO NOT EDIT.",
        "// Regenerate after changing source images in tools/qr_sources/.",
        "// This file is gitignored because its matrices + label payload text",
        "// may be private; tools/qr_sources_example/ + this file's .example",
        "// sibling carry the committed placeholder data.",
        "#pragma once",
        "",
        "#include <cstdint>",
        "",
        "namespace wmt {",
        "",
        "struct QrCode {",
        "    uint8_t        size;   // module count (N). Data is N × ((N+7)/8) bytes.",
        "    const uint8_t *data;   // MSB-first, row-major, 1-bit packed.",
        "    const char    *label;  // short UI label (not the payload)",
        "};",
        "",
    ]

    for i, (label, payload, version, ecc, packed) in enumerate(codes):
        n = 17 + 4 * version
        lines.append(f"// QR {i}: \"{payload}\"  (v{version} ECC {ecc}, {n}x{n})")
        lines.append(f"static const uint8_t qr_data_{i}[] = {{")
        bpr = (n + 7) // 8
        for row_i in range(n):
            row = packed[row_i * bpr:(row_i + 1) * bpr]
            lines.append("    " + ", ".join(f"0x{b:02x}" for b in row) + ",")
        lines.append("};")
        lines.append("")

    lines.append("static const QrCode QR_CODES[] = {")
    for i, (label, _, version, _, _) in enumerate(codes):
        n = 17 + 4 * version
        lines.append(f"    {{ {n}, qr_data_{i}, \"{label}\" }},")
    lines.append("};")
    lines.append(f"static constexpr int QR_COUNT = {len(codes)};")
    lines.append("")
    lines.append("} // namespace wmt")
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    src_dir = select_source_dir()
    imgs = sorted(p for p in src_dir.iterdir()
                  if p.is_file() and p.suffix.lower() in SUPPORTED_EXTS)
    if not imgs:
        print(f"no images in {src_dir} or {EXAMPLE_DIR}", file=sys.stderr)
        return 1

    # Decode all payloads so we can pick a shared version.
    decoded: list[tuple[Path, str, str]] = []
    for img in imgs:
        decoded.append((img, short_label(img), decode(img)))

    payloads = [p for _, _, p in decoded]
    shared_v = pick_shared_version(payloads, MIN_SHARED_ECC)
    n_shared = 17 + 4 * shared_v

    codes: list[tuple[str, str, int, str, bytes]] = []
    for img, label, payload in decoded:
        ecc = best_ecc_at(payload, shared_v)
        matrix = encode_matrix(payload, shared_v, ecc)
        packed = pack_msb(matrix)
        codes.append((label, payload, shared_v, ecc, packed))
        print(f"  {img.name:30s} → {label:12s} v{shared_v} ECC {ecc} "
              f"({n_shared}x{n_shared}) {len(packed)}B  payload: {payload}")

    OUT_H.parent.mkdir(parents=True, exist_ok=True)
    OUT_H.write_text(emit_header(codes))

    total = sum(len(c[4]) for c in codes)
    privacy_note = " (GITIGNORED — contains your payloads)" \
                   if src_dir == SRC_DIR else ""
    print(f"wrote {OUT_H.relative_to(REPO)}  "
          f"({len(codes)} codes, v{shared_v} {n_shared}x{n_shared}, "
          f"{total} B total){privacy_note}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
