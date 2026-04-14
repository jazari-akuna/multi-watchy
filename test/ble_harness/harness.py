"""BLE + serial test harness for the Watchy event-sync service.

Public surface (see module-level functions and ``WatchyHarness`` class) matches
the contract documented in ``README.md``. All BLE calls are async; the serial
reader runs in a background daemon thread so tests can drain recent log lines
via ``serial_lines()`` at any point.
"""

from __future__ import annotations

import asyncio
import struct
import sys
import threading
import time
from typing import Optional

from bleak import BleakClient, BleakScanner

try:
    import serial  # pyserial
except Exception:  # pragma: no cover - optional at runtime
    serial = None  # type: ignore[assignment]


# ---------------------------------------------------------------------------
# GATT UUIDs (must match firmware header)
# ---------------------------------------------------------------------------
SERVICE_UUID = "4e2d0001-6f00-4d1a-9c7b-8f7c2e0a1d3b"
WRITE_UUID = "4e2d0002-6f00-4d1a-9c7b-8f7c2e0a1d3b"
STATE_UUID = "4e2d0003-6f00-4d1a-9c7b-8f7c2e0a1d3b"

PACKET_LEN = 64
STATE_LEN = 16

TAG_TIME_SYNC = 0x01
TAG_EVENT = 0x02
TAG_BATCH_END = 0x03
TAG_CLEAR = 0x04


# ---------------------------------------------------------------------------
# Packet builders
# ---------------------------------------------------------------------------
def _blank() -> bytearray:
    return bytearray(PACKET_LEN)


def make_time_sync(utc_seconds: int, gmt_offset_sec: int) -> bytes:
    """Build a 64B TIME_SYNC packet.

    Layout: [0]=0x01, [8..15]=int64 LE utc, [16..19]=int32 LE gmt offset.
    """
    p = _blank()
    p[0] = TAG_TIME_SYNC
    struct.pack_into("<q", p, 8, int(utc_seconds))
    struct.pack_into("<i", p, 16, int(gmt_offset_sec))
    return bytes(p)


def make_event(
    start_utc: int,
    end_utc: int,
    title: str,
    all_day: bool = False,
) -> bytes:
    """Build a 64B EVENT packet.

    Layout: [0]=0x02, [1]=flags, [2..9]=int64 LE start, [10..17]=int64 LE end,
    [18..63]=UTF-8 title null-padded (truncated to 46 bytes).
    """
    p = _blank()
    p[0] = TAG_EVENT
    p[1] = 0x01 if all_day else 0x00
    struct.pack_into("<q", p, 2, int(start_utc))
    struct.pack_into("<q", p, 10, int(end_utc))
    title_bytes = title.encode("utf-8")[:46]
    p[18 : 18 + len(title_bytes)] = title_bytes
    # remaining title bytes already zero from _blank()
    return bytes(p)


def make_batch_end() -> bytes:
    p = _blank()
    p[0] = TAG_BATCH_END
    return bytes(p)


def make_clear() -> bytes:
    p = _blank()
    p[0] = TAG_CLEAR
    return bytes(p)


# ---------------------------------------------------------------------------
# Serial reader (background thread)
# ---------------------------------------------------------------------------
class _SerialReader:
    """Daemon thread that drains a serial port into a shared line buffer."""

    def __init__(self, port: str, baud: int = 115200):
        self.port = port
        self.baud = baud
        self._ser: Optional["serial.Serial"] = None
        self._lines: list[str] = []
        self._lock = threading.Lock()
        self._stop = threading.Event()
        self._thread: Optional[threading.Thread] = None
        self._available = False
        self._open()

    @property
    def available(self) -> bool:
        return self._available

    def _open(self) -> None:
        if serial is None:
            print(
                f"[harness] pyserial not installed; serial capture disabled ({self.port})",
                file=sys.stderr,
            )
            return
        try:
            self._ser = serial.Serial(self.port, self.baud, timeout=0.1)
        except Exception as e:
            print(
                f"[harness] serial port {self.port} unavailable: {e}; continuing without serial",
                file=sys.stderr,
            )
            self._ser = None
            return
        self._available = True
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def _run(self) -> None:
        assert self._ser is not None
        buf = b""
        while not self._stop.is_set():
            try:
                chunk = self._ser.read(256)
            except Exception:
                # Port probably went away; exit quietly.
                return
            if not chunk:
                continue
            buf += chunk
            while b"\n" in buf:
                line, _, buf = buf.partition(b"\n")
                try:
                    text = line.decode("utf-8", errors="replace").rstrip("\r")
                except Exception:
                    text = repr(line)
                with self._lock:
                    self._lines.append(text)

    def drain(self) -> list[str]:
        with self._lock:
            out = self._lines
            self._lines = []
        return out

    def close(self) -> None:
        self._stop.set()
        if self._thread is not None:
            self._thread.join(timeout=0.5)
        if self._ser is not None:
            try:
                self._ser.close()
            except Exception:
                pass


# ---------------------------------------------------------------------------
# Main harness class
# ---------------------------------------------------------------------------
class WatchyHarness:
    def __init__(self, serial_port: str | None = "/dev/ttyUSB0"):
        self._client: Optional[BleakClient] = None
        self._device_address: Optional[str] = None
        self._serial: Optional[_SerialReader] = None
        if serial_port:
            self._serial = _SerialReader(serial_port)

    # -- connection ---------------------------------------------------------
    async def scan_and_connect(self, timeout: float = 15.0) -> None:
        """Scan for a device advertising ``SERVICE_UUID`` and connect to it.

        Raises ``TimeoutError`` if no matching device is seen within timeout.
        """
        target_uuid = SERVICE_UUID.lower()
        devices = await BleakScanner.discover(
            timeout=timeout, return_adv=True
        )
        match_addr: Optional[str] = None
        # ``devices`` is a dict[address, (BLEDevice, AdvertisementData)] when
        # return_adv=True (bleak >= 0.20).
        try:
            items = devices.items()
        except AttributeError:
            # Older bleak returned list[BLEDevice]; fall back.
            items = [(d.address, (d, None)) for d in devices]
        for addr, pair in items:
            _dev, adv = pair
            uuids = []
            if adv is not None and getattr(adv, "service_uuids", None):
                uuids = [u.lower() for u in adv.service_uuids]
            if target_uuid in uuids:
                match_addr = addr
                break
        if match_addr is None:
            raise TimeoutError(
                f"No device advertising {SERVICE_UUID} found within {timeout:.1f}s"
            )
        self._device_address = match_addr
        self._client = BleakClient(match_addr)
        await self._client.connect()
        if not self._client.is_connected:
            raise ConnectionError(f"Failed to connect to {match_addr}")

    async def disconnect(self) -> None:
        if self._client is not None and self._client.is_connected:
            try:
                await self._client.disconnect()
            except Exception:
                pass
        self._client = None
        if self._serial is not None:
            self._serial.close()
            self._serial = None

    # -- GATT I/O -----------------------------------------------------------
    def _require_client(self) -> BleakClient:
        if self._client is None or not self._client.is_connected:
            raise RuntimeError("Not connected; call scan_and_connect() first")
        return self._client

    async def write(self, packet: bytes) -> None:
        assert len(packet) == PACKET_LEN, (
            f"BLE packet must be exactly {PACKET_LEN} bytes, got {len(packet)}"
        )
        client = self._require_client()
        await client.write_gatt_char(WRITE_UUID, packet, response=True)

    async def read_state(self) -> dict:
        client = self._require_client()
        raw = await client.read_gatt_char(STATE_UUID)
        if len(raw) < STATE_LEN:
            raise ValueError(
                f"STATE characteristic returned {len(raw)} bytes, expected {STATE_LEN}"
            )
        last_sync = struct.unpack_from("<q", raw, 0)[0]
        event_count = struct.unpack_from("<H", raw, 8)[0]
        schema_version = raw[10]
        return {
            "last_sync_utc": int(last_sync),
            "event_count": int(event_count),
            "schema_version": int(schema_version),
        }

    async def push_batch(self, time_sync: bytes, events: list[bytes]) -> None:
        """Send TIME_SYNC, each EVENT, then BATCH_END, awaiting each write."""
        await self.write(time_sync)
        for ev in events:
            await self.write(ev)
        await self.write(make_batch_end())

    # -- serial -------------------------------------------------------------
    def serial_lines(self, timeout: float = 0.0) -> list[str]:
        """Return lines captured on the serial port since the last call.

        If ``timeout > 0`` we wait up to that many seconds for *new* lines.
        """
        if self._serial is None or not self._serial.available:
            return []
        if timeout <= 0:
            return self._serial.drain()
        deadline = time.monotonic() + timeout
        collected: list[str] = []
        while True:
            collected.extend(self._serial.drain())
            if collected or time.monotonic() >= deadline:
                return collected
            time.sleep(0.05)
