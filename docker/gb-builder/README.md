# gb-builder

Containerised build environment for the patched Gadgetbridge fork that
ships with a Watchy driver. Produces a signed-debug APK without
requiring an Android SDK install on the host.

## What it does

1. Builds a Docker image containing JDK 17 and Android SDK 34
   (platform-tools + `platforms;android-34` + `build-tools;34.0.0`).
2. Runs the image with the repo bind-mounted at `/repo`.
3. Inside the container, `inside.sh`:
   - Applies the Watchy driver by running
     `companion/gb-driver/setup.sh`, which copies / patches the
     driver sources into `companion/gadgetbridge/`.
   - Invokes Gradle to assemble a debug APK (tries
     `assembleMainlineDebug`, falling back to `assembleMainDebug` or
     `assembleDebug` depending on which flavour the vendored
     Gadgetbridge tree exposes).
   - Prints the path of every APK produced.

Gradle's caches live on a named Docker volume (`gb-gradle-cache`) so
repeated builds don't re-download dependencies.

## Requirements

- Docker Engine on the host (Linux, macOS, or Windows with WSL2).
  Nothing else -- no local JDK or Android SDK needed.

## Usage

```sh
./docker/gb-builder/build.sh
```

Run from anywhere; the script resolves the repo root relative to its
own location. The first build takes roughly 10 minutes (Gradle deps +
SDK are fetched). Subsequent builds with a warm `gb-gradle-cache`
volume complete in ~2 minutes.

## Output

APKs are written to:

```
companion/gadgetbridge/app/build/outputs/apk/**/*.apk
```

The exact subdirectory depends on the flavour/variant Gradle chose.
`inside.sh` prints every APK path at the end of a successful build.

## Installing on a device

```sh
adb install -r companion/gadgetbridge/app/build/outputs/apk/mainline/debug/app-mainline-debug.apk
```

Adjust the path to match what the build printed. Use `-r` to replace
an existing install while keeping its data.

## Footprint

- Image size: ~2.2 GB (JDK base + Android SDK, only the components
  needed are installed; sources/emulator/docs are omitted).
- `gb-gradle-cache` volume: grows to ~1 GB after the first build.

## Offline behaviour

Once the image is built and the Gradle cache volume is warm, `docker
run` does not need network access. All network pulls are confined to
`docker build` time.

## Troubleshooting

- **Permission denied on APK / Gradle files on the host.** The image
  creates a `builder` user with UID matching `id -u` on the host at
  build time. If you've re-`chown`ed the repo or switched user, rebuild
  the image so the UID is refreshed: `docker build --no-cache ...` or
  simply re-run `build.sh` -- the build arg `HOST_UID` is passed on
  every invocation.
- **`assembleMainlineDebug` not found.** `inside.sh` automatically
  probes `assembleMainDebug` and `assembleDebug` as fallbacks. If none
  match, check the Gradle output for the correct task name and invoke
  `./gradlew <task>` manually inside a shell started with
  `docker run --rm -it --entrypoint bash watchy/gb-builder:latest`.
- **Wipe the Gradle cache:** `docker volume rm gb-gradle-cache`.
