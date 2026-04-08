# watch-legacy

Personal workspace for an SQFMI Watchy (ESP32-PICO-D4, 1.54" e-paper, 200x200 1-bit).

## Layout

- `sketches/WatchyMultiTZ/` — custom multi-timezone watchface sketch. Subclasses the `Watchy` class from `sqfmi/Watchy` v1.4.15, shows Shenzhen / San Francisco / Zurich simultaneously, current zone rendered big in the middle with full date, others as strips with `+Nd`/`-Nd` day-offset badges. POSIX TZ strings (`CST-8`, `PST8PDT,M3.2.0,M11.1.0`, `CET-1CEST,M3.5.0,M10.5.0/3`) handle DST automatically. UP cycles the current zone (partial refresh, ~450 ms). DOWN forces a WiFi+NTP re-sync then powers WiFi off. A state-aware settle loop polls all four buttons for 10 s after any press and performs a single full refresh on quiescence — no full refreshes otherwise.
- `backup/` — full 4 MB flash snapshots taken before each flashing step, with SHA-256s. Rollback with `esptool --port /dev/ttyUSB0 write-flash 0x0 backup/<file>.bin`.
- `Watchy/` — upstream `sqfmi/Watchy` library at tag v1.4.15, cloned for source reference. Gitignored; re-clone with `git clone https://github.com/sqfmi/Watchy.git && cd Watchy && git checkout v1.4.15`.
- `build/` — arduino-cli compile outputs. Gitignored.

## Hardware notes

Identified as **Watchy with v2.0 button wiring** (UP on GPIO 35, BATT on GPIO 34) despite having a USB-micro connector and plastic case. The v1.5 build (UP on GPIO 32) did not wake on UP press. Build FQBN:

```
esp32:esp32:watchy:Revision=v20,PartitionScheme=min_spiffs
```

## Build & flash

Requires arduino-cli + ESP32 core 2.0.17 + Watchy library 1.4.15 + deps (see `sketches/WatchyMultiTZ/README.md`).

```sh
arduino-cli compile \
  --fqbn 'esp32:esp32:watchy:Revision=v20,PartitionScheme=min_spiffs' \
  --output-dir build/MultiTZ_v20 \
  sketches/WatchyMultiTZ

arduino-cli upload \
  --fqbn 'esp32:esp32:watchy:Revision=v20,PartitionScheme=min_spiffs,UploadSpeed=115200' \
  --port /dev/ttyUSB0 \
  --input-dir build/MultiTZ_v20 \
  sketches/WatchyMultiTZ
```
