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
 *   DRAW DISTANCE - RenderWare's RwCameraSetFarClipPlane (0x001E2358: the
 *   camera's far plane at +0x84, from f12), which the race sets each frame:
 *   scaled down, RenderWare culls the world's sectors and objects beyond it,
 *   their VU1 work and their drawing - the track world (0x0021C780) is the
 *   race picture's largest part after the reflections.
 *
 * "car_reflections" in pc_settings.ini (on/off; off by default on a tablet,
 * on elsewhere; PS2HOST_CAR_REFLECTIONS=0/1 for a run) and "draw_distance"
 * (percent of the game's own: 100 elsewhere, 100 on a tablet until measured;
 * PS2HOST_DRAW_DISTANCE=N for a run).
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "ps2host/host.h"
#include "ps2host/platform.h"
#include "ps2host/settings.h"

void F_002274B0(lifted::Context& c);        // the cars' paint reflections
void F_001E2358(lifted::Context& c);        // RwCameraSetFarClipPlane(camera, far)

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

float DrawDistance() {
    static const float scale = [] {
        const char* v = std::getenv("PS2HOST_DRAW_DISTANCE");
        double percent = v && *v ? std::atof(v) : 100.0;
        if (!(v && *v)) {
            const std::string setting = ps2host::Setting("draw_distance");
            if (!setting.empty()) percent = std::atof(setting.c_str());
        }
        percent = percent < 20.0 ? 20.0 : percent > 100.0 ? 100.0 : percent;
        std::fprintf(stderr, "[lite graphics] draw distance %.0f%%\n", percent);
        return float(percent / 100.0);
    }();
    return scale;
}

void FarClip(lifted::Context& c) {
    const float scale = DrawDistance();
    if (scale < 1.0f) {
        float far;
        std::memcpy(&far, &c.f[12], 4);
        far *= scale;
        std::memcpy(&c.f[12], &far, 4);
    }
    F_001E2358(c);
}

}  // namespace

void RegisterLiteGraphics() {
    ps2host::Override(0x001E2358u, FarClip, "RwCameraSetFarClipPlane: DRAW DISTANCE (a tablet's lighter picture)");
    ps2host::Override(0x002274B0u, Reflections, "the cars' paint reflections: CAR REFLECTIONS (off on a tablet)");
}
