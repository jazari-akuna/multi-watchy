#!/usr/bin/env -S /home/sagan/Projects/watch-legacy/test/ble_harness/.venv/bin/python
"""Live BLE log tail for Watchy.

Tails /dev/ttyUSB0 at 115200 baud and surfaces lines emitted by
BleEventProvider (tag=..., commit: count=..., unknown tag ...).
When a `commit:` line arrives, parses count + lastSync and prints
a human-readable summary.

Usage:
    ./verify_live.py              # shebang uses ble_harness venv
    python3 verify_live.py        # source the ble_harness venv first

Dependencies: pyserial (already in test/ble_harness/.venv).
Stop with Ctrl-C.
"""
from __future__ import annotations

import re
import sys
from datetime import datetime

import serial  # type: ignore[import-not-found]

PORT = "/dev/ttyUSB0"
BAUD = 115200
COMMIT_RE = re.compile(r"commit:\s*count=(\d+)\s+lastSync=(-?\d+)")


def stamp() -> str:
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S")


def main() -> int:
    try:
        ser = serial.Serial(PORT, BAUD, timeout=1)
    except serial.SerialException as e:
        print(f"[{stamp()}] ERROR opening {PORT}: {e}", file=sys.stderr)
        return 1

    print(f"[{stamp()}] tailing {PORT} @ {BAUD} (Ctrl-C to stop)")
    try:
        while True:
            raw = ser.readline()
            if not raw:
                continue
            try:
                line = raw.decode("utf-8", errors="replace").rstrip()
            except Exception:
                continue
            if "[BLE]" not in line:
                continue
            print(f"[{stamp()}] {line}")
            m = COMMIT_RE.search(line)
            if m:
                count = int(m.group(1))
                last_sync = int(m.group(2))
                noun = "event" if count == 1 else "events"
                print(
                    f"[{stamp()}] COMMIT count={count} "
                    f"lastSync={last_sync} ({count} {noun} stored)"
                )
    except KeyboardInterrupt:
        print(f"\n[{stamp()}] stopped")
    finally:
        ser.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
