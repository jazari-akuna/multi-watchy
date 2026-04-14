#!/usr/bin/env bash
#
# Runs inside the gb-builder container. Applies the Watchy driver to the
# vendored Gadgetbridge source tree, then builds a signed-debug APK.
set -euo pipefail

cd /repo

echo "==== Applying Watchy driver to Gadgetbridge ===="
bash companion/gb-driver/setup.sh

cd companion/gadgetbridge

# Pick the first matching assemble task. Gadgetbridge uses product
# flavors; upstream currently publishes "mainline" but older forks used
# plain "main" or no flavor at all -- try each in order.
echo
echo "==== Discovering Gradle assemble task ===="
CANDIDATES=(assembleMainlineDebug assembleMainDebug assembleDebug)

# Generate a task list once so we don't pay for `gradlew tasks` per probe.
# `|| true` because gradle may exit non-zero if the project has warnings.
TASK_LIST="$(./gradlew --no-daemon --quiet tasks --all 2>/dev/null || true)"

TARGET=""
for candidate in "${CANDIDATES[@]}"; do
    if printf '%s\n' "${TASK_LIST}" | grep -qE "(^|[[:space:]])${candidate}([[:space:]]|$)"; then
        TARGET="${candidate}"
        break
    fi
done

if [[ -z "${TARGET}" ]]; then
    # Fallback: just try mainlineDebug -- most likely on current upstream.
    TARGET="assembleMainlineDebug"
    echo "WARNING: could not detect assemble task from 'tasks --all'; trying ${TARGET}"
fi

echo "Using Gradle task: ${TARGET}"
echo
echo "==== Building APK ===="
./gradlew --no-daemon "${TARGET}"

echo
echo "==== APK output ===="
find app/build/outputs/apk -name '*.apk' -printf '%p\n'
