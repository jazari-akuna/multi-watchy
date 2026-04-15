# test/

Tools for validating the WatchyMultiTZ firmware against real hardware.

## `ble_harness/`

End-to-end GATT test harness. Drives the watch's BLE event-sync service
directly from Linux via `bleak`, optionally tailing `/dev/ttyUSB0` for
firmware-side assertions. Five cases (happy_path, all_day_rule, time_sync,
clear, reconnect). See `ble_harness/README.md` for install, protocol
details, and how to add a case.

Quick start:

```sh
cd ble_harness
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
python3 run_all.py
```

Press **DOWN twice** on the watch to enter advertising mode before running.

## `verify_live.py`

Minimal companion to the harness — a passive serial tail that surfaces
`[BLE]` log lines emitted by `BleEventProvider` (packet dispatch,
`commit:` summaries, unknown tags) with local timestamps. Useful when
poking the watch manually from a phone or when debugging why a harness
case didn't commit.

Opens `/dev/ttyUSB0` at 115200 baud, prints every `[BLE]` line prefixed
with the host clock, and on each `commit:` line emits a parsed summary:

```
[2026-04-15 08:30:15] [BLE] commit: count=3 lastSync=1776180615
[2026-04-15 08:30:15] COMMIT count=3 lastSync=1776180615 (3 events stored)
```

Runs until Ctrl-C. Uses only `pyserial`, which is already installed in
`ble_harness/.venv`.

Run it either way:

```sh
# Via the shebang (points at the ble_harness venv directly):
./verify_live.py

# Or activate the venv first:
source ble_harness/.venv/bin/activate
python3 verify_live.py
```

If `/dev/ttyUSB0` is missing or busy (e.g. `arduino-cli monitor` is
attached), the script prints an error and exits non-zero.
