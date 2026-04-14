"""Five async test cases for the Watchy BLE event-sync service.

Each case returns ``(ok: bool, detail: str)``. Cases assume the harness is
already connected and they may leave the watch in a non-pristine state — the
orchestrator in ``run_all.py`` runs them in a deterministic order.
"""

from __future__ import annotations

import asyncio
import time

from harness import (
    WatchyHarness,
    make_clear,
    make_event,
    make_time_sync,
    make_batch_end,
)


# Tolerance, in seconds, between the harness's clock and the last_sync_utc
# reported by the watch after a TIME_SYNC write.
CLOCK_SKEW_TOLERANCE_S = 30


def _now() -> int:
    return int(time.time())


def _serial_contains(lines: list[str], needle: str) -> bool:
    return any(needle in ln for ln in lines)


async def case_happy_path(h: WatchyHarness) -> tuple[bool, str]:
    """TIME_SYNC + 3 timed events + BATCH_END → expect event_count == 3."""
    # Drain anything stale on the serial port first.
    h.serial_lines()
    now = _now()
    gmt_offset = 3600  # arbitrary but nonzero to exercise the field
    ts = make_time_sync(now, gmt_offset)
    events = [
        make_event(
            start_utc=now + (i + 1) * 3600,
            end_utc=now + (i + 1) * 3600 + 1800,
            title=f"Test {i + 1}/3",
        )
        for i in range(3)
    ]
    await h.push_batch(ts, events)
    # Give the firmware a moment to commit + log.
    await asyncio.sleep(0.75)
    state = await h.read_state()
    lines = h.serial_lines(timeout=0.25)

    problems: list[str] = []
    if state["event_count"] != 3:
        problems.append(f"event_count={state['event_count']} (want 3)")
    skew = abs(state["last_sync_utc"] - _now())
    if skew > CLOCK_SKEW_TOLERANCE_S:
        problems.append(
            f"last_sync_utc skew={skew}s > {CLOCK_SKEW_TOLERANCE_S}s"
        )
    if state["schema_version"] != 1:
        problems.append(f"schema_version={state['schema_version']} (want 1)")
    # Serial check is best-effort (may be disabled).
    if lines and not _serial_contains(lines, "commit: count=3"):
        problems.append("serial missing 'commit: count=3'")

    if problems:
        return False, "; ".join(problems)
    return True, "3 events stored"


async def case_all_day_rule(h: WatchyHarness) -> tuple[bool, str]:
    """Simulate the driver's demotion rule: when an all-day and a timed event
    both exist today, the driver only forwards the timed one. We verify the
    watch stores exactly 1 event when fed that single timed event.
    """
    h.serial_lines()
    now = _now()
    ts = make_time_sync(now, 0)
    timed = make_event(
        start_utc=now + 2 * 3600,
        end_utc=now + 2 * 3600 + 1800,
        title="Timed today",
    )
    # CLEAR first so prior test state doesn't pollute the count.
    await h.write(make_clear())
    await asyncio.sleep(0.25)
    await h.push_batch(ts, [timed])
    await asyncio.sleep(0.75)
    state = await h.read_state()
    if state["event_count"] != 1:
        return False, f"event_count={state['event_count']} (want 1)"
    return True, "driver-demoted payload stored as 1 event"


async def case_time_sync(h: WatchyHarness) -> tuple[bool, str]:
    """Push a TIME_SYNC with a deliberate +7s offset; verify the watch
    acknowledges (serial ``TIME_SYNC`` pattern) and reports a reasonable
    last_sync_utc on the STATE characteristic.
    """
    h.serial_lines()
    real_utc = _now()
    offset = 7
    ts = make_time_sync(real_utc + offset, 0)
    await h.write(ts)
    await asyncio.sleep(0.5)
    state = await h.read_state()
    lines = h.serial_lines(timeout=0.25)

    skew = abs(state["last_sync_utc"] - (real_utc + offset))
    if skew > CLOCK_SKEW_TOLERANCE_S:
        return False, (
            f"last_sync_utc skew={skew}s vs expected {real_utc + offset}"
        )
    if lines and not _serial_contains(lines, "TIME_SYNC"):
        return False, "serial lines captured but none contain 'TIME_SYNC'"
    return True, f"RTC advanced, skew={skew}s"


async def case_clear(h: WatchyHarness) -> tuple[bool, str]:
    """CLEAR packet → event_count == 0."""
    h.serial_lines()
    await h.write(make_clear())
    await asyncio.sleep(0.5)
    state = await h.read_state()
    if state["event_count"] != 0:
        return False, f"event_count={state['event_count']} (want 0)"
    return True, "store cleared"


async def case_reconnect(h: WatchyHarness) -> tuple[bool, str]:
    """Disconnect mid-batch after TIME_SYNC + 1 EVENT, reconnect, then push a
    clean 2-event batch. The watch must discard the partial batch and end at
    event_count == 2.
    """
    # Start from a known-empty state.
    h.serial_lines()
    await h.write(make_clear())
    await asyncio.sleep(0.25)

    now = _now()
    await h.write(make_time_sync(now, 0))
    await h.write(
        make_event(
            start_utc=now + 3600,
            end_utc=now + 3600 + 1800,
            title="Partial (should be discarded)",
        )
    )
    # Drop connection without BATCH_END.
    await h.disconnect()
    # Re-create serial reader on reconnect path by giving the harness a fresh
    # scan. The serial thread itself may have been closed in disconnect();
    # scan_and_connect() doesn't reopen it, but that's OK — the case only
    # asserts on STATE.
    await asyncio.sleep(1.0)
    await h.scan_and_connect(timeout=15.0)

    await h.push_batch(
        make_time_sync(_now(), 0),
        [
            make_event(
                start_utc=_now() + 7200,
                end_utc=_now() + 7200 + 1800,
                title="Clean A",
            ),
            make_event(
                start_utc=_now() + 10800,
                end_utc=_now() + 10800 + 1800,
                title="Clean B",
            ),
        ],
    )
    await asyncio.sleep(0.75)
    state = await h.read_state()
    if state["event_count"] != 2:
        return False, (
            f"event_count={state['event_count']} (want 2 — partial batch "
            f"should have been discarded)"
        )
    return True, "partial batch discarded; clean batch stored 2 events"
