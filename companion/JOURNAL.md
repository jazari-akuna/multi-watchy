# WatchyMultiTZ development journal

Chronological record of non-obvious decisions, surprises, and milestone notes.
Grows one entry per non-trivial event. Terse is fine.

Format:

```
## YYYY-MM-DD — Short title
(one-line context)
- observation / decision / action
- ...
```

---

## 2026-04-15 — Calendar sync: pivot to Gadgetbridge

- Original direction was a bespoke Android app (Kotlin + WorkManager).
- User challenged: why not Gadgetbridge? Re-evaluated.
- **Decision**: write a Gadgetbridge device driver (~4 Java files + 2
  registrations) instead of a standalone app. We get CalendarContract
  queries, multi-calendar settings UI, Nordic BLE wrapper, permissions
  onboarding, and F-Droid distribution for free. Trade-off accepted:
  upstream PR takes weeks; we run our fork locally until merged.
- Verified Gadgetbridge's actual hook API (via upstream source):
  `onAddCalendarEvent(CalendarEventSpec)` / `onDeleteCalendarEvent(type,id)`.
  Pushes are per-event; our driver buffers + debounces ~500 ms then
  flushes one atomic BLE batch (TIME_SYNC → EVENT × N → BATCH_END).

## 2026-04-15 — GATT protocol v1

- Service UUID `4e2d0001-6f00-4d1a-9c7b-8f7c2e0a1d3b` (random v4).
- 64 B write packets chosen to match the in-memory Event struct size
  (title[46] + flags + pad + 2×int64). One format works both on the wire
  and in RAM — minimizes encoding bugs at both ends.
- No sequence numbers: ATT guarantees in-order delivery on a single
  characteristic.
- Commit is atomic via BATCH_END — the accumulator only swaps into the
  ring buffer + NVS shadow when the end marker arrives.

## 2026-04-15 — Double-press DOWN = sync

- Single DOWN press was previously `forceSync()` (NTP). User asked for
  a double-press gesture instead, so a stray button poke doesn't wake
  WiFi unnecessarily.
- Watch polls DOWN for 500 ms after a DOWN wake; a second press triggers
  `syncAll()`: BLE first, WiFi NTP fallback.
- Single press now a no-op (just sleep).

---

## 2026-04-15 — M3 source complete; APK build deferred on this host

Driver source + setup + Docker builder all landed. Spent ~40 min trying to
build an APK end-to-end (both inside Docker and via a locally-installed Android
SDK) but hit a persistent `sdkmanager "Fetch remote repository"` hang — the
tool spins on loading its package index even though `curl` to the same CDN
URLs returns 200 in <1 s. Known to be slow/flaky in some networks; no config
tweak I tried (IPv4-force, custom licenses hash, `--network=host`) broke it
loose under my time budget.

What's verified:
- setup.sh runs cleanly + idempotently against the upstream clone.
- Our 4 Java files exist in the right packages with AGPLv3 headers and the
  right base classes (`AbstractBLEDeviceCoordinator`, `AbstractBTLESingleDeviceSupport`).
- Code audit shows proper imports + structure. Debounced calendar push, all-day
  demotion, atomic batch flush via BATCH_END all present.

Not verified on this host: actual APK byte output. Path forward is one of:
1. Run `bash docker/gb-builder/build.sh` on a host with better network (the
   Dockerfile already has `--network=host` so it inherits whatever works there).
2. Install `openjdk-21-jdk` + finish `sdkmanager "platforms;android-34"
   "build-tools;34.0.0"` locally, then `cd companion/gadgetbridge && ./gradlew
   assembleMainlineDebug`.

Not blocking M1. The watch firmware is validated end-to-end via the Python BLE
harness; it does not care whether GB or a PWA or a raw script pushed the
packets. M4 (phone-in-loop) requires an Android device + a successful APK
build; neither is achievable in this session's environment.

## 2026-04-15 — Starting M3 (Gadgetbridge driver)

Gadgetbridge cloned (shallow) at `companion/gadgetbridge/` (gitignored). Verified
actual upstream APIs:

- Base class: `AbstractBLEDeviceCoordinator` + `AbstractBTLEDeviceSupport`.
- Calendar hooks are per-event: `onAddCalendarEvent(CalendarEventSpec)` +
  `onDeleteCalendarEvent(byte, long)`. Our driver buffers them with a 500 ms
  debounce then flushes an atomic batch over our GATT.
- DeviceType.java is a flat enum — just add `WATCHY(WatchyCoordinator.class),`
  before `TEST` entry. No DeviceHelper edit needed; it iterates values().
- BangleJS is the closest reference for calendar handling (see `BangleJSDeviceSupport.java`).
- PineTimeJF is the closest reference for minimal BLE coordinator.

Dispatched 3 agents in parallel:
- G1: WatchyCoordinator + WatchyConstants + setup.sh + README.
- G2: WatchySupport + WatchyProtocol (the calendar-push heart).
- G3: Docker APK builder so user doesn't need Android SDK locally.

## 2026-04-15 — M1 reached: 5/5 harness cases pass on real hardware

Orchestrated 5 agents in parallel (BleEventProvider.cpp, WatchFace double-press
+ syncAll, .ino swap, Web Bluetooth PWA, Python harness). All landed cleanly.
Found + fixed three integration bugs during bring-up on real hardware:

1. **Compile: `size_t` not declared in BleEventProvider.h** — added `<stddef.h>`.
2. **syncNow() tore down BLE on first BATCH_END** — made the wait loop run for
   the full timeout; `batchArrived_` is now sticky across the session. Return
   `everCommitted` (any commit → true). Rationale: phones may push multiple
   batches; the test harness runs 5 cases over one advertising window.
3. **Bluedroid did not auto-resume advertising on disconnect** — added an
   explicit `BLEServerCallbacks::onDisconnect` that re-starts the advertiser.
   Without this the reconnect case (disconnect mid-batch → rescan) would find
   the watch unreachable.
4. **Partial batch leaked into the next session** — made `TIME_SYNC` implicitly
   start a new batch (resets `accumCount_`). Without this, a peer that dropped
   mid-batch and came back would get events from both sessions committed on the
   next `BATCH_END`. TIME_SYNC-as-session-start is also the natural contract the
   phone side will use.

Also added a cold-boot 60 s BLE advertise window so the harness (and a future
first-time-pair flow) can find the watch without a human button press.

Firmware at 93 % flash (1841 KB / 1966 KB). BLE adds significant size; still
125 KB of headroom.
