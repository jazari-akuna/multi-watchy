# Watchy BLE bring-up tool

A minimal Web Bluetooth page used to exercise the watch's GATT service while
the real Gadgetbridge driver is still under construction. Not a production
companion: it has three buttons and a log.

## What it does

- Connects to a watch advertising service UUID `4e2d0001-6f00-4d1a-9c7b-8f7c2e0a1d3b`.
- Sends a `CLEAR` packet.
- Sends a canned test batch: `TIME_SYNC` (now + current tz offset) followed by
  three synthetic events (`Design review`, `Lunch with team`, `Gym` at now+1h,
  +2h, +3h) and a `BATCH_END`.
- Reads the 16-byte STATE characteristic and pretty-prints
  `lastSyncUtc / eventCount / schemaVersion`.

## Running

From this directory:

```
python3 -m http.server 8000
```

Open <http://localhost:8000/> in Chrome. Web Bluetooth requires a secure
context, but `http://localhost` counts as secure, so no TLS setup is needed
during development.

## Caveats

- Chrome only. Works on Chrome desktop (Linux, macOS, Windows, ChromeOS) and
  Chrome on Android. Firefox and iOS Safari do not implement Web Bluetooth.
- On Android, pair-less GATT still requires Location permission for the
  browser (OS-level Bluetooth scan requirement).
- The watch does not advertise by default to save battery. Press **DOWN
  twice** on the watch to start a short advertise window, then click
  "Connect watch" within that window.
- If the device picker is empty, re-trigger advertising on the watch and
  retry. Chrome caches GATT state, so if reconnects misbehave, disconnect
  from Chrome's `chrome://bluetooth-internals` page and retry.

## Modifying the test batch

All packet construction lives in `app.js`. To change the canned batch, edit
the `events` array inside `sendTestBatch()` (title, hour offset). To add new
packet tags, add a `TAG_*` constant and a builder alongside `buildEvent` /
`buildTimeSync`. Keep packets at exactly 64 bytes and match the wire format
documented in the firmware's BLE header.
