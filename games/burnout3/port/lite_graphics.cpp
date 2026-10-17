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
 *   (0.5-1.2% of the picture's pixels change). It saves little GPU time: in a
 *   frame-exact race (lavapipe, frames 17700-18290) the same ~2,100 draws and
 *   ~250 passes a picture, the passes' area 37.7 -> 37.1 MP (-1.6%). (An A12's
 *   40 -> 22 ms GPU time in one 8 s window was noise.) On by default.
 *
 *   (Not a draw distance: the race sets RenderWare's far clip plane to 10,000
 *   units - in effect none - from derived code that calls it directly, and
 *   the track's own sectors decide what is drawn; scaling it changed nothing.)
 *
 * "car_reflections" in pc_settings.ini (on/off; off by default on a tablet,
 * where everything not needed to play goes, on elsewhere;
 * PS2HOST_CAR_REFLECTIONS=0/1 for a run).
 */

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "ps2host/host.h"
#include "ps2host/platform.h"
#include "ps2host/settings.h"

void F_002274B0(lifted::Context& c);        // the cars' paint reflections
void F_0026A710(lifted::Context& c);        // an object put into a view (0 the race's, 1-3 the reflection map's)

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

/* The track drawn into the reflection map's three small views (FBP 0x18B,
   0x193, 0x19B, 128 wide, put together in 0x16B for the cars' paint): the
   world's objects are put into views 1-3 one by one here (0x0021C780), each
   right after the race's own view - three more renders of the track, and a
   render pass to and fro for each object. Without reflections, not done. */
void ReflectionViews(lifted::Context& c) {
    const std::uint64_t view = c.r[5];
    if (!CarReflections() && view >= 1 && view <= 3) {
        c.r[2] = 0;
        return;
    }
    F_0026A710(c);
}

}  // namespace

void RegisterLiteGraphics() {
    ps2host::Override(0x002274B0u, Reflections, "the cars' paint reflections: CAR REFLECTIONS (a setting)");
    ps2host::Override(0x0026A710u, ReflectionViews, "an object put into a view: the reflection map's views with CAR REFLECTIONS");
}
