# ble_harness

End-to-end test harness for the Watchy BLE event-sync GATT service. No phone
or Gadgetbridge driver required: the harness speaks the wire protocol
directly over BlueZ via `bleak`, and optionally reads the watch's debug log
over USB serial via `pyserial`.

Used two ways:

- Manually: `python3 run_all.py`
- By an autonomous agent fix-loop: the orchestrator prints `[PASS] …` /
  `[FAIL] …` lines and exits 0 iff every case passes.

## Install

Python 3.11+.

```sh
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

## Prerequisites

- Linux with BlueZ (tested on Ubuntu). The current user must be able to scan
  and connect without root — add yourself to the `bluetooth` group:

  ```sh
  sudo usermod -aG bluetooth "$USER"
  # log out / back in
  ```

- (Optional) Watch connected over USB so the harness can tail
  `/dev/ttyUSB0`. If the port is absent or unreadable, the harness prints a
  warning and continues; serial-based assertions become best-effort (they
  pass when no serial lines were captured).

- The watch must be advertising its BLE service (UUID
  `4e2d0001-6f00-4d1a-9c7b-8f7c2e0a1d3b`). On current firmware you typically
  need to press **DOWN twice** on the watch to enter pairing/advertising
  mode. The harness scans for up to 15 s.

## Run

```sh
python3 run_all.py
```

Exit codes:

- `0` — every case passed
- `1` — at least one case failed
- `2` — could not even connect (fatal setup error)

## GATT protocol

Authoritative, matches the firmware header.

| UUID                                        | Dir     | Size | Purpose      |
| ------------------------------------------- | ------- | ---- | ------------ |
| `4e2d0001-6f00-4d1a-9c7b-8f7c2e0a1d3b`      | service | —    | root service |
| `4e2d0002-6f00-4d1a-9c7b-8f7c2e0a1d3b`      | write   | 64 B | command port |
| `4e2d0003-6f00-4d1a-9c7b-8f7c2e0a1d3b`      | read/notify | 16 B | state |

Write packet tags (byte 0):

- `0x01 TIME_SYNC` — `[8..15]` int64 LE UTC seconds, `[16..19]` int32 LE GMT
  offset in seconds.
- `0x02 EVENT` — `[1]` flags (bit 0 = all-day), `[2..9]` start UTC int64 LE,
  `[10..17]` end UTC int64 LE, `[18..63]` UTF-8 title (46 B, null-padded).
- `0x03 BATCH_END` — commit the in-flight batch.
- `0x04 CLEAR` — drop the stored event list.

STATE characteristic (16 B LE): `[0..7]` last-sync UTC int64, `[8..9]` event
count uint16, `[10]` schema version uint8 (=1), `[11..15]` zero.

## Test cases

Listed in `cases.py`; each is an `async` function returning
`(ok: bool, detail: str)`.

1. **happy_path** — push TIME_SYNC + 3 timed events + BATCH_END, expect
   `event_count == 3` and a `commit: count=3` log line.
2. **all_day_rule** — driver spec says an all-day event is demoted when a
   timed event exists the same day. The harness sends only the timed event
   (mirroring the driver) and expects `event_count == 1`.
3. **time_sync** — send a TIME_SYNC that is +7 s ahead of wall-clock; expect
   the STATE characteristic's `last_sync_utc` to advance and (if serial is
   available) a `TIME_SYNC` log line.
4. **clear** — send a CLEAR; expect `event_count == 0`.
5. **reconnect** — send TIME_SYNC + one EVENT, disconnect without
   BATCH_END, reconnect, then push a clean 2-event batch. Expect
   `event_count == 2` (partial batch discarded).

## Adding a test case

1. In `cases.py`, add `async def case_foo(h: WatchyHarness) -> tuple[bool, str]:`
   that sends BLE traffic via `h.push_batch` / `h.write`, then reads
   `await h.read_state()` and `h.serial_lines()` to check invariants.
2. Return `(True, "short summary")` on success; `(False, "what went wrong")`
   on failure — the orchestrator prints the string verbatim, so keep it
   grep-friendly.
3. Register the case in the `CASES` list in `run_all.py`.

## Known quirks

- You may need to press **DOWN twice** on the watch to start advertising —
  the harness waits up to 15 s per scan.
- `bleak` on Linux occasionally needs a kick if a previous session didn't
  disconnect cleanly: `bluetoothctl disconnect <MAC>` or toggle the adapter.
- The `reconnect` case tears the serial reader down on `disconnect()` and
  does not recreate it on reconnect, so it asserts purely on the STATE
  characteristic.
- If `/dev/ttyUSB0` is missing, serial-based checks degrade to "no evidence
  → pass". Run with the watch plugged in for stricter coverage.
