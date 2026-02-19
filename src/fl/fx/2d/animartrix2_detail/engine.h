#pragma once

// Engine: Standalone implementation for Animartrix2 animations.
// Extracted from animartrix2_detail.hpp so it can be forward-declared
// and included independently by chasing_spirals implementations.

#include "crgb.h"
#include "fl/fx/2d/animartrix2_detail/context.h"
#include "fl/fx/2d/animartrix2_detail/core_types.h"
#include "fl/fx/2d/animartrix2_detail/engine_core.h"
#include "fl/math_macros.h"
#include "fl/stl/optional.h"
#include "fl/stl/stdint.h"
#include "fl/stl/vector.h"

namespace fl {

struct Engine {
    Context *mCtx;

    // Core ANIMartRIX state (extracted from animartrix_detail::ANIMartRIX)
    int num_x = 0;
    int num_y = 0;
    float speed_factor = 1;
    float radial_filter_radius = 23.0;
    bool serpentine = false;

    render_parameters animation;
    oscillators timings;
    modulators move;
    rgb pixel;

    fl::vector<fl::vector<float>> polar_theta;
    fl::vector<fl::vector<float>> distance;

    unsigned long a = 0;
    unsigned long b = 0;
    unsigned long c = 0;

    float show1 = 0.0f;
    float show2 = 0.0f;
    float show3 = 0.0f;
    float show4 = 0.0f;
    float show5 = 0.0f;
    float show6 = 0.0f;
    float show7 = 0.0f;
    float show8 = 0.0f;
    float show9 = 0.0f;
    float show0 = 0.0f;

    fl::optional<fl::u32> currentTime;

    Engine(Context *ctx) : mCtx(ctx) {}
    ~Engine();

    void setTime(fl::u32 t) { currentTime = t; }
    fl::u32 getTime() { return currentTime.has_value() ? currentTime.value() : fl::millis(); }

    void init(int w, int h) {
        animation = render_parameters();
        timings = oscillators();
        move = modulators();
        pixel = rgb();

        this->num_x = w;
        this->num_y = h;
        this->radial_filter_radius = FL_MIN(w, h) * 0.65;
        render_polar_lookup_table(
            (num_x / 2) - 0.5,
            (num_y / 2) - 0.5,
            polar_theta,
            distance,
            num_x,
            num_y);
        timings.master_speed = 0.01;
    }

    void setSpeedFactor(float speed) { this->speed_factor = speed; }

    // Method wrappers that delegate to standalone functions
    void calculate_oscillators(oscillators &t) {
        fl::calculate_oscillators(t, move, getTime(), speed_factor);
    }

    void run_default_oscillators(float master_speed = 0.005) {
        fl::run_default_oscillators(timings, move, getTime(), speed_factor, master_speed);
    }

    float render_value(render_parameters &anim) {
        return fl::render_value(anim);
    }

    rgb rgb_sanity_check(rgb &p) {
        return fl::rgb_sanity_check(p);
    }

    void get_ready() {
        fl::get_ready(a, b);
    }

    void logOutput() {
        fl::logOutput(b);
    }

    void logFrame() {
        fl::logFrame(c);
    }

    // Color blend wrappers
    float subtract(float &x, float &y) { return fl::subtract(x, y); }
    float multiply(float &x, float &y) { return fl::multiply(x, y); }
    float add(float &x, float &y) { return fl::add(x, y); }
    float screen(float &x, float &y) { return fl::screen(x, y); }
    float colordodge(float &x, float &y) { return fl::colordodge(x, y); }
    float colorburn(float &x, float &y) { return fl::colorburn(x, y); }

    void setPixelColorInternal(int x, int y, rgb pixel) {
        fl::u16 idx = mCtx->xyMapFn(x, y, mCtx->xyMapUserData);
        mCtx->leds[idx] = CRGB(pixel.red, pixel.green, pixel.blue);
    }

    fl::u16 xyMap(fl::u16 x, fl::u16 y) {
        return mCtx->xyMapFn(x, y, mCtx->xyMapUserData);
    }
};

inline Engine::~Engine() = default;

inline Context::~Context() {
    delete mEngine;
}

// Initialize context with grid dimensions
inline void init(Context &ctx, int w, int h) {
    if (!ctx.mEngine) {
        ctx.mEngine = new Engine(&ctx);
    }
    ctx.num_x = w;
    ctx.num_y = h;
    ctx.mEngine->init(w, h);
}

// Set time for deterministic rendering
inline void setTime(Context &ctx, fl::u32 t) {
    ctx.currentTime = t;
    if (ctx.mEngine) {
        ctx.mEngine->setTime(t);
    }
}

}  // namespace fl
