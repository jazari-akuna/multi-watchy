# Watchy Gadgetbridge driver

A device driver that teaches [Gadgetbridge](https://codeberg.org/Freeyourgadget/Gadgetbridge)
how to talk to a SQFMI Watchy running the `WatchyMultiTZ` firmware in this
repo. Scope for the MVP is one-way calendar push: the phone opens a BLE
session, streams events over our custom 4e2d0001 GATT service, and the watch
commits them to its RTC-slow-memory ring buffer. No activity tracking, no
notifications.

## Layout

```
companion/
  gb-driver/            <- this directory, checked into our repo
    src/devices/watchy/
      WatchyCoordinator.java
      WatchyConstants.java
    src/service/devices/watchy/
      WatchySupport.java        (owned by a sibling agent)
      WatchyProtocol.java       (owned by a sibling agent)
    setup.sh
    README.md
  gadgetbridge/         <- upstream clone, gitignored in our repo.
                           setup.sh populates and patches this tree.
```

We keep the driver source in `gb-driver/` and copy-deploy it into the
upstream clone via `setup.sh` rather than forking the full Gadgetbridge
repo. This keeps our diff small and re-baseable when upstream moves.

## Prerequisites

- `git`
- JDK 17+ (Gadgetbridge's Gradle build requires 17)
- Android SDK (cmdline-tools + platform 34 + build-tools 34)
  - Easiest: install Android Studio, then point `ANDROID_HOME` at its SDK.
- Internet access on first run (to clone Gadgetbridge from codeberg).

## Apply the driver

```bash
cd companion/gb-driver
./setup.sh
```

The script is idempotent — re-run it after you edit driver sources and it
will refresh the files in the clone without duplicating the DeviceType enum
entry or the string resource.

What it does:

1. Clones `https://codeberg.org/Freeyourgadget/Gadgetbridge.git` into
   `companion/gadgetbridge/` (shallow, `master` branch) if it's not there.
2. Copies `WatchyCoordinator.java` and `WatchyConstants.java` into the
   clone's `devices/watchy/` tree. If `WatchySupport.java` /
   `WatchyProtocol.java` exist under `gb-driver/src/service/`, it copies
   them too; otherwise it skips them with a warning.
3. Inserts a `WATCHY(WatchyCoordinator.class),` entry into
   `model/DeviceType.java` right before `TEST`, plus the matching import.
4. Appends `<string name="devicetype_watchy">Watchy</string>` to
   `res/values/strings.xml`.

## Build the APK

```bash
cd ../gadgetbridge
./gradlew assembleMainlineDebug
```

The resulting APK lands at:

```
app/build/outputs/apk/mainline/debug/app-mainline-debug.apk
```

Other useful variants:
- `assembleMainlineNightly` — signed nightly build (needs `nightly_store_file`).
- `assembleBanglejs*` — Bangle.js-flavored Gadgetbridge; ignore for Watchy work.

## Install

```bash
adb install -r app/build/outputs/apk/mainline/debug/app-mainline-debug.apk
```

If you already have official Gadgetbridge installed, uninstall it first
(`adb uninstall nodomain.freeyourgadget.gadgetbridge`) — the debug APK
signature won't match.

## Developing the driver

1. Edit files under `gb-driver/src/`.
2. Re-run `./setup.sh`.
3. Re-run the `assembleMainlineDebug` gradle task.
4. `adb install -r …` — no need to uninstall between debug rebuilds.

Logs from the running app are under tag `Watchy*`:

```bash
adb logcat -v time '*:S' 'nodomain.freeyourgadget.gadgetbridge.*:V' | grep -i watchy
```

## Upstreaming

Once the driver stabilises we can:

1. Fork Gadgetbridge on codeberg.
2. Commit the same files (no `setup.sh` / copy trick) on a branch.
3. Open a PR per
   [Gadgetbridge's contribution guide](https://codeberg.org/Freeyourgadget/Gadgetbridge/wiki/Code-Contribution-Guidelines).

The driver code itself is already AGPL-licensed to match Gadgetbridge, so
no license gymnastics are needed at upstream time.

## Notes

- `companion/gadgetbridge/` is gitignored in our repo. Only `gb-driver/`
  is tracked.
- The BLE GATT UUIDs, packet tags and layout in `WatchyConstants.java` are
  the authoritative Android-side mirror of the firmware header at
  `sketches/WatchyMultiTZ/src/platform/watchy/BleEventProvider.h`. If you
  change one, change the other.
