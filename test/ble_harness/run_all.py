#!/usr/bin/env python3
"""Orchestrator: run every case in ``cases.py`` and exit 0 iff all pass.

Output format is intentionally line-oriented so an autonomous fix-loop can
grep for ``[PASS]``/``[FAIL]`` prefixes and the trailing ``N/M passed`` line.
"""

from __future__ import annotations

import asyncio
import sys

from harness import WatchyHarness
import cases


CASES = [
    ("happy_path", cases.case_happy_path),
    ("all_day_rule", cases.case_all_day_rule),
    ("time_sync", cases.case_time_sync),
    ("clear", cases.case_clear),
    ("reconnect", cases.case_reconnect),
]


async def main() -> int:
    h = WatchyHarness(serial_port="/dev/ttyUSB0")
    try:
        await h.scan_and_connect(timeout=15.0)
    except Exception as e:
        print(f"[FATAL] scan_and_connect: {type(e).__name__}: {e}")
        return 2

    passed = 0
    total = len(CASES)
    for name, fn in CASES:
        try:
            ok, detail = await fn(h)
        except Exception as e:
            ok, detail = False, f"exception: {type(e).__name__}: {e}"
        status = "PASS" if ok else "FAIL"
        print(f"[{status}] {name} — {detail}")
        if ok:
            passed += 1

    try:
        await h.disconnect()
    except Exception:
        pass

    print(f"\n{passed}/{total} passed")
    return 0 if passed == total else 1


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
