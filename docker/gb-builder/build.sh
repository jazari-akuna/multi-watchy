#!/usr/bin/env bash
#
# Host-side wrapper. Builds the gb-builder image (if needed) and runs
# it with the repo bind-mounted so the resulting APK ends up under
# companion/gadgetbridge/app/build/outputs/apk/ on the host.
set -euo pipefail

HERE="$(cd "$(dirname "$0")"/../.. && pwd)"
IMAGE="watchy/gb-builder:latest"

docker build \
    --network=host \
    --build-arg HOST_UID="$(id -u)" \
    --build-arg HOST_GID="$(id -g)" \
    -t "${IMAGE}" \
    "${HERE}/docker/gb-builder"

# Use -t only when stdin is a TTY so CI/non-interactive invocations work.
TTY_FLAGS=()
if [[ -t 0 && -t 1 ]]; then
    TTY_FLAGS+=(-it)
fi

docker run --rm "${TTY_FLAGS[@]}" \
    -v "${HERE}":/repo \
    -v gb-gradle-cache:/home/builder/.gradle \
    "${IMAGE}"
