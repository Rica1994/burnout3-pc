/*
 * Frame skipping for slow machines (an iPad's): the race's world goes on at
 * the game's 60 steps a second, and its picture is drawn only as often as the
 * machine keeps up - instead of the whole game running in slow motion.
 *
 * The race's draw (0x00134A20) moves nothing on (docs/HIGH_FPS_PLAN.md: the
 * HUD editor's pause runs the draw without the world update, pixel for pixel
 * the same; the camera is rebuilt in the next draw from the cars). Each call
 * here is one step of the race; the time between two calls against the 1/60 s
 * a step should take is the lag. While the lag is more than a step, the draw
 * is left out - at most three in a row, so a picture comes at least every
 * fourth step - and the window shows the last picture drawn
 * (NoteDrawLeftOut) rather than a frame buffer nobody drew this step.
 *
 * On by default on a tablet; "frame_skip" in pc_settings.ini (on/off) or
 * PS2HOST_FRAMESKIP=0/1 (a scripted run's --frameskip=0/1) turns it.
 */

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>

#include "ps2host/host.h"
#include "ps2host/platform.h"
#include "ps2host/run.h"
#include "ps2host/settings.h"

void F_00134A20(lifted::Context& c);        // the race's draw

namespace {

using Clock = std::chrono::steady_clock;
constexpr double kStep = 1.0 / 59.94;        // a PS2's NTSC field
constexpr int kMostSkipped = 3;              // in a row

bool Enabled() {
    static const bool on = [] {
        const char* v = std::getenv("PS2HOST_FRAMESKIP");
        const bool wanted = v && *v ? *v != '0' : ps2host::SettingOn("frame_skip", ps2host::platform::kMobile);
        std::fprintf(stderr, "[frame skip] %s\n", wanted ? "on: the race's draw left out while it lags behind"
                                                         : "off");
        return wanted;
    }();
    return on;
}

void RaceDraw(lifted::Context& c) {
    if (!Enabled()) {
        F_00134A20(c);
        return;
    }
    static Clock::time_point last{};
    static double lag = 0.0;
    static int in_a_row = 0;
    static unsigned drawn = 0, skipped = 0;
    static Clock::time_point reported = Clock::now();
    const Clock::time_point now = Clock::now();
    const double step = last == Clock::time_point{} ? kStep : std::chrono::duration<double>(now - last).count();
    last = now;
    // A gap (a pause, a menu, loading): start again on time.
    if (step > 0.5) lag = 0.0;
    else lag = std::clamp(lag + step - kStep, 0.0, 4 * kStep);
    const bool skip = lag > kStep && in_a_row < kMostSkipped;
    if (skip) {
        ++in_a_row;
        ++skipped;
        ps2host::NoteDrawLeftOut();
    } else {
        in_a_row = 0;
        ++drawn;
        F_00134A20(c);
    }
    if (std::chrono::duration<double>(now - reported).count() >= 10.0) {
        std::fprintf(stderr, "[frame skip] last 10 s: %u race pictures drawn, %u left out\n", drawn, skipped);
        drawn = skipped = 0;
        reported = now;
    }
}

}  // namespace

void RegisterFrameSkip() {
    ps2host::Override(0x00134A20u, RaceDraw, "the race's draw: left out while a slow machine lags (frame skip)");
}
