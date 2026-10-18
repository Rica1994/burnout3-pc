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
 * The lag leaks away (x0.9 a step): the game keeps to the vertical blank, so
 * at full speed steps come exactly 1/60 s apart and a lag once over a step
 * never went down again - an A12 iPad's race drew one step in four, 15 fps,
 * with its GPU 65% busy and the EE 60% idle. And a step is left out too while
 * the last one drawn is still on its way - not yet done by the GS thread or
 * the GPU (GsFrameDone, GpuFrameDone): drawing goes as fast as the GPU
 * finishes pictures, no faster (a queue of pictures would only add delay).
 * Measured on the A12 (17 Oct, 18:19): no better - 123-157 race pictures in
 * 10 s against 150, a dip to 70% speed, presents less even. The GPU is ~80%
 * busy at one picture in four with ~43 ms a picture, and seen only at frame
 * ends a picture done is drawn again a step late: the GPU idles between
 * pictures. Since 18 Oct the default is paced by the GPU's measured cost
 * per picture instead (Pacer, below); PS2HOST_FRAMESKIP_MODE=lag is the lag
 * alone as before, =gpu (or PS2HOST_FRAMESKIP_GPU=1) the leaking lag and the
 * GPU's pace.
 *
 * On by default on a tablet; "frame_skip" in pc_settings.ini (on/off) or
 * PS2HOST_FRAMESKIP=0/1 (a scripted run's --frameskip=0/1) turns it.
 */

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

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

/* The paced way (the default): a picture every N steps, N chosen from what a
   picture costs the GPU (its timestamps, GpuBusyNs) - the smallest N whose
   N/60 s holds it with room to spare. The lag alone never comes down at full
   speed (above): an A12's race stayed at one in four, 15 fps, with its GPU
   ~43 ms a picture and so only ~65% busy - one in three (20 fps, 50 ms) holds
   that. Unlike waiting for the GPU to be done (PS2HOST_FRAMESKIP_GPU), a
   picture is not held back while the one before it is still on the GPU: one
   picture queued behind another keeps the GPU busy rather than idle between
   them, and ~43 ms of work in each 50 ms gap fits. As the GPU gets cheaper
   (fewer passes, less to decode), N comes down by itself: under ~29 ms a
   picture, one in two - 30 fps. The lag guards it: a game falling behind real
   time (the GPU or the GS thread slower than measured: a crash, traffic) goes
   up a step at once and stays there a while. */
struct Pacer {
    int n = kMostSkipped + 1;           // a picture every n steps
    double cost_ms = 0.0;               // a picture's GPU time, averaged
    std::uint64_t window_gpu = 0;
    unsigned window_drawn = 0, fits_lower = 0;
    double hold_until = 0.0;            // seconds: no step down before
    double lag = 0.0;

    static int StepsFor(double ms) {
        // Room: 12% and a millisecond (the GS thread's own end of a frame,
        // presenting, a picture costing more than the average).
        const double need = ms * 1.12 + 1.0;
        for (int n = 1; n <= kMostSkipped + 1; ++n)
            if (n * kStep * 1000.0 >= need) return n;
        return kMostSkipped + 1;
    }
    // At each drawn picture.
    void Drawn(double now) {
        const std::uint64_t gpu = ps2host::GpuBusyNs();
        if (ps2host::GpuProfilingNow()) {       // (its time a little high: not counted)
            window_drawn = 0;
            return;
        }
        if (!window_drawn) window_gpu = gpu;
        if (++window_drawn < 8) return;
        // 8 pictures (about half a second): their GPU time, the left-out
        // steps' between them counted with them (they are its cost too).
        const double ms = double(gpu - window_gpu) / 1e6 / double(window_drawn - 1);
        window_drawn = 0;
        if (gpu <= window_gpu) return;          // no timestamps yet
        cost_ms = cost_ms == 0.0 ? ms : cost_ms * 0.6 + ms * 0.4;
        const int want = StepsFor(cost_ms);
        if (want > n) {
            n = want;
            fits_lower = 0;
        } else if (want < n && now >= hold_until) {
            // Down a step only once it has fitted for three windows running.
            if (++fits_lower >= 3) {
                --n;
                fits_lower = 0;
            }
        } else {
            fits_lower = 0;
        }
    }
    // At each step: the game's lag behind real time.
    void Step(double step, double now) {
        // Three steps behind (a single long frame - a load, a crash's camera -
        // is not the GPU falling behind: on the iPad, 18 Oct 11:26, two steps
        // and a 2 s hold left it at one in four a third of the time at 33 ms).
        lag = std::clamp(lag * 0.95 + step - kStep, 0.0, 8 * kStep);
        if (lag > 3 * kStep && n <= kMostSkipped) {
            ++n;
            lag = 0.0;
            fits_lower = 0;
            hold_until = now + 1.0;
        }
    }
};

void RaceDraw(lifted::Context& c) {
    ps2host::InputAnchor();                   // an input recording's race start (dbc.cpp)
    if (!Enabled()) {
        F_00134A20(c);
        return;
    }
    static Clock::time_point last{};
    static const Clock::time_point start = Clock::now();
    static double lag = 0.0;
    static std::uint64_t drawn_frame = 0;      // the frame the last drawn step's picture went in
    // PS2HOST_FRAMESKIP_MODE: paced (the default), lag (by the lag alone, as
    // before 18 Oct), gpu (the lag leaking and the GPU's pace; also
    // PS2HOST_FRAMESKIP_GPU=1).
    enum Mode { kPaced, kLag, kGpu };
    static const Mode mode = [] {
        const char* gpu = std::getenv("PS2HOST_FRAMESKIP_GPU");
        if (gpu && *gpu && *gpu != '0') return kGpu;
        const char* m = std::getenv("PS2HOST_FRAMESKIP_MODE");
        const std::string v = m ? m : "";
        if (v == "lag") return kLag;
        if (v == "gpu") return kGpu;
        return kPaced;
    }();
    static Pacer pacer;
    static int in_a_row = 0;
    static unsigned drawn = 0, skipped = 0;
    static Clock::time_point reported = Clock::now();
    const Clock::time_point now = Clock::now();
    const double seconds = std::chrono::duration<double>(now - start).count();
    const double step = last == Clock::time_point{} ? kStep : std::chrono::duration<double>(now - last).count();
    last = now;
    bool skip = false;
    if (mode == kPaced && ps2host::GpuBusyNs() != 0) {
        if (step > 0.5) pacer.lag = 0.0;            // a gap: a pause, a menu, loading
        else pacer.Step(step, seconds);
        skip = in_a_row + 1 < pacer.n && in_a_row < kMostSkipped;
    } else {
        // A gap (a pause, a menu, loading): start again on time.
        if (step > 0.5) lag = 0.0;
        else if (mode != kGpu) lag = std::clamp(lag + step - kStep, 0.0, 4 * kStep);
        else lag = std::clamp(lag * 0.9 + step - kStep, 0.0, 4 * kStep);
        // This step's picture goes in the frame after the last one ended.
        const std::uint64_t gpu_done = ps2host::GpuFrameDone();
        const bool on_its_way = mode == kGpu && drawn_frame &&
                                (ps2host::GsFrameDone() < drawn_frame || (gpu_done && gpu_done < drawn_frame));
        skip = (lag > kStep || on_its_way) && in_a_row < kMostSkipped;
    }
    const std::uint64_t frame = ps2host::LastFrame() + 1;
    if (skip) {
        ++in_a_row;
        ++skipped;
        ps2host::NoteDrawLeftOut();
    } else {
        in_a_row = 0;
        ++drawn;
        drawn_frame = frame;
        F_00134A20(c);
        if (mode == kPaced) pacer.Drawn(seconds);
    }
    if (std::chrono::duration<double>(now - reported).count() >= 10.0) {
        if (mode == kPaced)
            std::fprintf(stderr, "[frame skip] last 10 s: %u race pictures drawn, %u left out; one in %d (GPU %.1f ms a picture)\n",
                         drawn, skipped, pacer.n, pacer.cost_ms);
        else
            std::fprintf(stderr, "[frame skip] last 10 s: %u race pictures drawn, %u left out\n", drawn, skipped);
        drawn = skipped = 0;
        reported = now;
    }
}

}  // namespace

void RegisterFrameSkip() {
    ps2host::Override(0x00134A20u, RaceDraw, "the race's draw: left out while a slow machine lags (frame skip)");
}
