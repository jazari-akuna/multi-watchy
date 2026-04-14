#!/usr/bin/env bash
# setup.sh — apply the Watchy driver to a local Gadgetbridge clone.
#
# Idempotent: safe to re-run. Only mutates ../gadgetbridge/.
#
# Steps:
#   1. Clone Gadgetbridge if missing.
#   2. Copy Watchy driver sources into the clone.
#   3. Register WATCHY in model/DeviceType.java (enum entry + import).
#   4. Add the devicetype_watchy string resource.
#   5. Print build instructions.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# gb-driver sits next to the gadgetbridge clone: companion/gb-driver and
# companion/gadgetbridge are siblings.
GB_DIR="$(cd "$SCRIPT_DIR/.." && pwd)/gadgetbridge"
GB_REPO_URL="https://codeberg.org/Freeyourgadget/Gadgetbridge.git"

DRIVER_SRC="$SCRIPT_DIR/src/devices/watchy"
DEVICES_DST="$GB_DIR/app/src/main/java/nodomain/freeyourgadget/gadgetbridge/devices/watchy"
SERVICE_DST="$GB_DIR/app/src/main/java/nodomain/freeyourgadget/gadgetbridge/service/devices/watchy"

DEVICE_TYPE_JAVA="$GB_DIR/app/src/main/java/nodomain/freeyourgadget/gadgetbridge/model/DeviceType.java"
STRINGS_XML="$GB_DIR/app/src/main/res/values/strings.xml"

log()  { printf '[watchy-setup] %s\n' "$*"; }
warn() { printf '[watchy-setup] WARN: %s\n' "$*" >&2; }
die()  { printf '[watchy-setup] ERROR: %s\n' "$*" >&2; exit 1; }

# ---------------------------------------------------------------- clone
if [[ ! -d "$GB_DIR/.git" ]]; then
    log "Cloning Gadgetbridge into $GB_DIR"
    mkdir -p "$(dirname "$GB_DIR")"
    git clone --depth 1 "$GB_REPO_URL" "$GB_DIR"
else
    log "Gadgetbridge clone already present at $GB_DIR"
fi

# ---------------------------------------------------------- copy driver files
mkdir -p "$DEVICES_DST" "$SERVICE_DST"

copy_if_present() {
    local src="$1" dst="$2"
    if [[ -f "$src" ]]; then
        cp "$src" "$dst"
        log "Copied $(basename "$src") -> $dst"
    else
        warn "Skipping $(basename "$src") — source not present yet (other agent will provide)"
    fi
}

# Devices tree (coordinator + constants — owned by this repo).
copy_if_present "$DRIVER_SRC/WatchyCoordinator.java" "$DEVICES_DST/WatchyCoordinator.java"
copy_if_present "$DRIVER_SRC/WatchyConstants.java"   "$DEVICES_DST/WatchyConstants.java"

# Service tree (support + protocol — owned by a sibling agent; may not exist
# on first run). setup.sh must not fail if they're missing.
SUPPORT_SRC="$SCRIPT_DIR/src/service/devices/watchy"
copy_if_present "$SUPPORT_SRC/WatchySupport.java"  "$SERVICE_DST/WatchySupport.java"
copy_if_present "$SUPPORT_SRC/WatchyProtocol.java" "$SERVICE_DST/WatchyProtocol.java"

# --------------------------------------- patch model/DeviceType.java: import
[[ -f "$DEVICE_TYPE_JAVA" ]] || die "DeviceType.java not found at $DEVICE_TYPE_JAVA"

COORD_IMPORT="import nodomain.freeyourgadget.gadgetbridge.devices.watchy.WatchyCoordinator;"
if grep -qF "$COORD_IMPORT" "$DEVICE_TYPE_JAVA"; then
    log "DeviceType.java import already present"
else
    # Insert right before the TestDeviceCoordinator import to keep the sorted
    # block tidy (devices.test sorts after devices.watchy — but alphabetically
    # w < t is false, so watchy goes AFTER test. In practice we just need a
    # stable anchor, and prepending works fine.)
    TMP="$(mktemp)"
    awk -v ins="$COORD_IMPORT" '
        /^import nodomain\.freeyourgadget\.gadgetbridge\.devices\.test\.TestDeviceCoordinator;/ && !done {
            print ins
            done = 1
        }
        { print }
    ' "$DEVICE_TYPE_JAVA" > "$TMP"
    mv "$TMP" "$DEVICE_TYPE_JAVA"
    log "Inserted WatchyCoordinator import into DeviceType.java"
fi

# --------------------------------------- patch model/DeviceType.java: enum
if grep -q 'WATCHY(WatchyCoordinator' "$DEVICE_TYPE_JAVA"; then
    log "DeviceType.java already has WATCHY enum entry"
else
    TMP="$(mktemp)"
    # Insert `    WATCHY(WatchyCoordinator.class),` on the line just before
    # the TEST entry, preserving indentation.
    awk '
        /^[[:space:]]*TEST\(TestDeviceCoordinator\.class\);/ && !done {
            print "    WATCHY(WatchyCoordinator.class),"
            done = 1
        }
        { print }
    ' "$DEVICE_TYPE_JAVA" > "$TMP"
    mv "$TMP" "$DEVICE_TYPE_JAVA"
    log "Inserted WATCHY enum entry before TEST"
fi

# --------------------------------------- patch res/values/strings.xml
[[ -f "$STRINGS_XML" ]] || die "strings.xml not found at $STRINGS_XML"

if grep -q 'name="devicetype_watchy"' "$STRINGS_XML"; then
    log "strings.xml already has devicetype_watchy"
else
    TMP="$(mktemp)"
    # Insert immediately before the closing </resources> tag.
    awk '
        /^<\/resources>[[:space:]]*$/ && !done {
            print "    <string name=\"devicetype_watchy\" translatable=\"false\">Watchy</string>"
            done = 1
        }
        { print }
    ' "$STRINGS_XML" > "$TMP"
    mv "$TMP" "$STRINGS_XML"
    log "Appended devicetype_watchy string resource"
fi

# ---------------------------------------------------------------- summary
cat <<EOF

[watchy-setup] Driver applied.

Build an APK:
    cd "$GB_DIR" && ./gradlew assembleMainlineDebug

APK will be at:
    $GB_DIR/app/build/outputs/apk/mainline/debug/app-mainline-debug.apk

Install to an attached device:
    adb install -r "$GB_DIR/app/build/outputs/apk/mainline/debug/app-mainline-debug.apk"

EOF
