// Desktop simulator entry point for the WatchyMultiTZ watchface.
//
// Produces a 200x200 PNG of the watchface by running the exact same
// rendering code as the on-device firmware, just plumbed into SimDisplay /
// SimClock etc. instead of the Watchy HAL.
//
// CLI:
//   --time YYYY-MM-DDTHH:MM     main-zone wall-clock time (the sim picks a
//                                UTC epoch that, for the zone indicated by
//                                --main-idx, resolves to this wall time).
//                                Default: 2026-04-14T19:05
//   --battery <volts>           battery voltage, e.g. 3.9. Default: 3.9
//   --main-idx <0|1|2>          which of the three zones is "main".
//                                Default: 2 (ZRH to match the mockup)
//   --out <path>                PNG output path. Default: out/screenshot.png

// stb_image_write is implemented in SimDisplay.cpp (single TU). Main just
// relies on SimDisplay's internal call to it.

// Platform-agnostic HAL + face code.
#include "hal/Types.h"
#include "face/WatchFace.h"
#include "face/DayBar.h"

// Sim backends.
#include "SimDisplay.h"
#include "SimClock.h"
#include "SimButtons.h"
#include "SimPower.h"
#include "SimNetwork.h"
#include "SimEventProvider.h"

// Global configuration (ZONES array, bar range constants).
#include "../sketches/WatchyMultiTZ/settings.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

namespace {

struct Args {
    std::string time    = "2026-04-14T19:05";
    float       battery = 3.9f;
    int         mainIdx = 2;
    std::string out     = "out/screenshot.png";
};

void printUsage(const char *argv0) {
    std::fprintf(stderr,
        "usage: %s [--time YYYY-MM-DDTHH:MM] [--battery V] "
        "[--main-idx 0|1|2] [--out PATH]\n",
        argv0);
}

bool parseArgs(int argc, char **argv, Args &a) {
    for (int i = 1; i < argc; ++i) {
        const char *k = argv[i];
        auto needsVal = [&](const char *name) -> const char * {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "error: %s requires a value\n", name);
                return nullptr;
            }
            return argv[++i];
        };
        if (!std::strcmp(k, "--time")) {
            const char *v = needsVal(k); if (!v) return false;
            a.time = v;
        } else if (!std::strcmp(k, "--battery")) {
            const char *v = needsVal(k); if (!v) return false;
            a.battery = static_cast<float>(std::atof(v));
        } else if (!std::strcmp(k, "--main-idx")) {
            const char *v = needsVal(k); if (!v) return false;
            a.mainIdx = std::atoi(v);
            if (a.mainIdx < 0 || a.mainIdx > 2) {
                std::fprintf(stderr, "error: --main-idx must be 0, 1, or 2\n");
                return false;
            }
        } else if (!std::strcmp(k, "--out")) {
            const char *v = needsVal(k); if (!v) return false;
            a.out = v;
        } else if (!std::strcmp(k, "-h") || !std::strcmp(k, "--help")) {
            printUsage(argv[0]);
            std::exit(0);
        } else {
            std::fprintf(stderr, "error: unknown arg '%s'\n", k);
            printUsage(argv[0]);
            return false;
        }
    }
    return true;
}

// Parse "YYYY-MM-DDTHH:MM" (sim local for the selected main zone) into a
// UTC epoch. The approach: treat the input as wall-clock time in the
// main-zone's POSIX TZ, find the corresponding UTC by running a local
// tzset cycle and calling mktime(). We go via setenv+tzset so the exact
// same TZ rules the face uses also govern the sim's parsing.
int64_t parseLocalTimeToUtc(const char *iso, const char *posixTZ) {
    int Y, M, D, h, m;
    if (std::sscanf(iso, "%d-%d-%dT%d:%d", &Y, &M, &D, &h, &m) != 5) {
        std::fprintf(stderr, "warning: couldn't parse --time '%s', defaulting\n",
                     iso);
        Y = 2026; M = 4; D = 14; h = 19; m = 5;
    }
    char prevTZ[128] = {0};
    const char *curTZ = std::getenv("TZ");
    if (curTZ) std::snprintf(prevTZ, sizeof prevTZ, "%s", curTZ);

    ::setenv("TZ", posixTZ, 1);
    ::tzset();

    std::tm tm_{};
    tm_.tm_year = Y - 1900;
    tm_.tm_mon  = M - 1;
    tm_.tm_mday = D;
    tm_.tm_hour = h;
    tm_.tm_min  = m;
    tm_.tm_sec  = 0;
    tm_.tm_isdst = -1;
    std::time_t t = std::mktime(&tm_);

    // Restore TZ.
    if (curTZ) ::setenv("TZ", prevTZ, 1);
    else       ::unsetenv("TZ");
    ::tzset();

    return static_cast<int64_t>(t);
}

} // namespace

int main(int argc, char **argv) {
    Args args;
    if (!parseArgs(argc, argv, args)) return 1;

    // Select the main-zone POSIX TZ so the sim can interpret the --time arg.
    const int midx = ((args.mainIdx % wmt::NUM_ZONES) + wmt::NUM_ZONES) % wmt::NUM_ZONES;
    const char *mainTZ = wmt::ZONES[midx].posixTZ;
    const int64_t nowUtc = parseLocalTimeToUtc(args.time.c_str(), mainTZ);

    // Configure the DayBar's minute axis from the compile-time zone schedule.
    wmt::DayBar::configure(wmt::BAR_START_MIN, wmt::BAR_END_MIN);

    // Build sim HAL.
    wmt::SimDisplay        display(args.out.c_str());
    wmt::SimClock          clock;    clock.setUtc(nowUtc);
    wmt::SimButtons        buttons;
    wmt::SimPower          power;    power.setFakeBattery(args.battery);
    wmt::SimNetwork        network;
    wmt::SimEventProvider  events;
    // Pre-populate a demo event. The offsets are in seconds; negative values
    // place the event in the past (useful for testing in-event inversion).
    // Defaults: starts +1 h, ends +2 h (i.e. not active yet).
    wmt::Event demo{};
    std::snprintf(demo.title, sizeof demo.title, "Next event displayed here");
    const char *evOffsetEnv = std::getenv("SIM_EVENT_OFFSET");
    const int64_t startOff = evOffsetEnv ? std::atoll(evOffsetEnv) : 3600;
    demo.startUtc = nowUtc + startOff;
    demo.endUtc   = demo.startUtc + 3600;
    events.add(demo);

    // Compose WatchFace.
    wmt::WatchFaceDeps deps{};
    deps.display  = &display;
    deps.clock    = &clock;
    deps.buttons  = &buttons;
    deps.power    = &power;
    deps.network  = &network;
    deps.events   = &events;
    deps.zones    = wmt::ZONES;
    deps.numZones = wmt::NUM_ZONES;

    int mainIdxRef = args.mainIdx;
    wmt::WatchFace face(deps, mainIdxRef);

    // Render one frame (partial refresh doesn't matter for sim).
    face.render(/*partialRefresh=*/false);

    std::printf("sim rendered\n");
    std::printf("  time     = %s  (zone %s, utc=%lld)\n",
                args.time.c_str(), mainTZ, (long long)nowUtc);
    std::printf("  battery  = %.2fV\n", static_cast<double>(args.battery));
    std::printf("  main-idx = %d (%s)\n", args.mainIdx, wmt::ZONES[midx].label);
    std::printf("  out      = %s\n", args.out.c_str());
    return 0;
}
