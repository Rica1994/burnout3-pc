/*
 * Lighter graphics for slow machines (an iPad's): parts of the race's picture
 * that cost a tablet much and show little, left out by a setting.
 *
 * Found by leaving the race scene's functions out one at a time on an A12
 * iPad (runs/race_measure.args: what each costs) and in a frame-exact
 * scripted race on a PC (what each draws, the pictures compared):
 *
 *   CAR REFLECTIONS - 0x002274B0, the cars' paint reflections (an environment
 *   map drawn each frame): without it the bodywork only loses its shine
 *   (1.2% of the picture's pixels change) while an A12's GPU time for a race
 *   picture falls from ~40 ms to ~22 ms.
 *
 *   (Not a draw distance: the race sets RenderWare's far clip plane to 10,000
 *   units - in effect none - from derived code that calls it directly, and
 *   the track's own sectors decide what is drawn; scaling it changed nothing.)
 *
 * "car_reflections" in pc_settings.ini (on/off; off by default on a tablet,
 * on elsewhere; PS2HOST_CAR_REFLECTIONS=0/1 for a run).
 */

#include <cstdio>
#include <cstdlib>

#include "ps2host/host.h"
#include "ps2host/platform.h"
#include "ps2host/settings.h"

void F_002274B0(lifted::Context& c);        // the cars' paint reflections

namespace {

bool CarReflections() {
    static const bool on = [] {
        const char* v = std::getenv("PS2HOST_CAR_REFLECTIONS");
        const bool wanted = v && *v ? *v != '0' : ps2host::SettingOn("car_reflections", !ps2host::platform::kMobile);
        std::fprintf(stderr, "[lite graphics] car reflections %s\n", wanted ? "on" : "off");
        return wanted;
    }();
    return on;
}

void Reflections(lifted::Context& c) {
    if (CarReflections()) F_002274B0(c);
    else c.r[2] = 0;
}

}  // namespace

void RegisterLiteGraphics() {
    ps2host::Override(0x002274B0u, Reflections, "the cars' paint reflections: CAR REFLECTIONS (off on a tablet)");
}
