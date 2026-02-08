#pragma once
// allow-include-after-namespace

// Animartrix2 detail: Refactored version of animartrix_detail.hpp
// Original by Stefan Petrick 2023. Adapted to C++ by Netmindz 2023.
// Adapted to FastLED by Zach Vorhies 2024.
// Refactored to free-function architecture 2026.
//
// Licensed under Creative Commons Attribution License CC BY-NC 3.0
// https://creativecommons.org/licenses/by-nc/3.0/
//
// Architecture: Context struct holds all shared state. Each animation is a
// free function (Visualizer) that operates on Context. Internally delegates
// to animartrix_detail::ANIMartRIX for bit-identical output.

#ifndef ANIMARTRIX2_INTERNAL
#error "This file is not meant to be included directly. Include animartrix2.hpp instead."
#endif

// Include the original detail for its ANIMartRIX base class
#define ANIMARTRIX_INTERNAL
#include "fl/fx/2d/animartrix_detail.hpp"

#include "crgb.h"
#include "fl/stl/vector.h"
#include "fl/stl/math.h"
#include "fl/stl/optional.h"
#include "fl/stl/stdint.h"
#include "fl/sin32.h"
#include "fl/fixed_point/s16x16.h"

#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif

#ifndef FL_ANIMARTRIX_USES_FAST_MATH
#define FL_ANIMARTRIX_USES_FAST_MATH 1
#endif

#if FL_ANIMARTRIX_USES_FAST_MATH
FL_FAST_MATH_BEGIN
FL_OPTIMIZATION_LEVEL_O3_BEGIN
#endif

namespace animartrix2_detail {

// Forward declaration
struct Context;

// Visualizer: a free function that operates on Context to render one frame
using Visualizer = void (*)(Context &ctx);

// Callback for mapping (x,y) to a 1D LED index
using XYMapCallback = fl::u16 (*)(fl::u16 x, fl::u16 y, void *userData);

// Context: All shared state for animations, passed to free-function visualizers.
// Internally wraps an animartrix_detail::ANIMartRIX to reuse existing logic.
struct Context {
    // Grid dimensions
    int num_x = 0;
    int num_y = 0;

    // Output target
    CRGB *leds = nullptr;
    XYMapCallback xyMapFn = nullptr;
    void *xyMapUserData = nullptr;

    // Time
    fl::optional<fl::u32> currentTime;

    // Internal engine (reuses original implementation for bit-identical output)
    struct Engine;
    Engine *mEngine = nullptr;

    Context() = default;
    ~Context();
    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;
};

// Per-pixel pre-computed s16x16 values for Chasing_Spirals inner loop.
// These are constant per-frame (depend only on grid geometry, not time).
// Defined here (above Engine) so Engine can hold a persistent vector of them.
struct ChasingSpiralPixelLUT {
    fl::s16x16 base_angle;     // 3*theta - dist/3
    fl::s16x16 dist_scaled;    // distance * scale (0.1), pre-scaled for noise coords
    fl::s16x16 rf3;            // 3 * radial_filter (for red channel)
    fl::s16x16 rf_half;        // radial_filter / 2 (for green channel)
    fl::s16x16 rf_quarter;     // radial_filter / 4 (for blue channel)
    fl::u16 pixel_idx;        // Pre-computed xyMap(x, y) output pixel index
};

// LUT-accelerated 2D Perlin noise using s16x16 fixed-point.
// Internals use Q8.24 (24 fractional bits) for precision exceeding float.
// The fade LUT replaces the 6t^5-15t^4+10t^3 polynomial with table lookup.
// The z=0 specialization halves work vs full 3D noise.
struct perlin_s16x16 {
    static constexpr int HP_BITS = 24;
    static constexpr fl::i32 HP_ONE = 1 << HP_BITS; // 16777216 = 1.0

    // Build 257-entry Perlin fade LUT in Q8.24 format.
    static inline void init_fade_lut(fl::i32 *table) {
        for (int i = 0; i <= 256; i++) {
            fl::i64 t = static_cast<fl::i64>(i) * (HP_ONE / 256);
            fl::i64 t2 = (t * t) >> HP_BITS;
            fl::i64 t3 = (t2 * t) >> HP_BITS;
            fl::i64 inner = (t * (6LL * HP_ONE)) >> HP_BITS;
            inner -= 15LL * HP_ONE;
            inner = (t * inner) >> HP_BITS;
            inner += 10LL * HP_ONE;
            table[i] = static_cast<fl::i32>((t3 * inner) >> HP_BITS);
        }
    }

    // 2D Perlin noise. Input s16x16, output s16x16 approx [-1, 1].
    // perm: 256-byte Perlin permutation table (indexed with & 255).
    static inline fl::s16x16 pnoise2d(fl::s16x16 fx, fl::s16x16 fy,
                                      const fl::i32 *fade_lut,
                                      const fl::u8 *perm) {
        auto P = [perm](int x) -> int { return perm[x & 255]; };

        int X, Y;
        fl::i32 x, y;
        floor_frac(fx.raw(), X, x);
        floor_frac(fy.raw(), Y, y);
        X &= 255;
        Y &= 255;

        fl::i32 u = fade(x, fade_lut);
        fl::i32 v = fade(y, fade_lut);

        int A  = P(X)     + Y;
        int AA = P(A);
        int AB = P(A + 1);
        int B  = P(X + 1) + Y;
        int BA = P(B);
        int BB = P(B + 1);

        fl::i32 result = lerp(v,
            lerp(u, grad(P(AA), x,          y),
                    grad(P(BA), x - HP_ONE, y)),
            lerp(u, grad(P(AB), x,          y - HP_ONE),
                    grad(P(BB), x - HP_ONE, y - HP_ONE)));

        return fl::s16x16::from_raw(result >> (HP_BITS - fl::s16x16::FRAC_BITS));
    }

  private:
    static constexpr int FP_BITS = fl::s16x16::FRAC_BITS;
    static constexpr fl::i32 FP_ONE = 1 << FP_BITS;

    // Decompose s16x16 raw value into integer floor and Q8.24 fractional part.
    static FASTLED_FORCE_INLINE void floor_frac(fl::i32 fp16, int &ifloor,
                                                fl::i32 &frac24) {
        ifloor = fp16 >> FP_BITS;
        frac24 = (fp16 & (FP_ONE - 1)) << (HP_BITS - FP_BITS);
    }

    // LUT fade: 1 lookup + 1 lerp replaces 5 multiplies.
    static FASTLED_FORCE_INLINE fl::i32 fade(fl::i32 t, const fl::i32 *table) {
        fl::u32 idx = static_cast<fl::u32>(t) >> 16;
        fl::i32 frac = t & 0xFFFF;
        fl::i32 a = table[idx];
        fl::i32 b = table[idx + 1];
        return a + static_cast<fl::i32>(
            (static_cast<fl::i64>(frac) * (b - a)) >> 16);
    }

    static FASTLED_FORCE_INLINE fl::i32 lerp(fl::i32 t, fl::i32 a, fl::i32 b) {
        return a + static_cast<fl::i32>(
            (static_cast<fl::i64>(t) * (b - a)) >> HP_BITS);
    }

    // z=0 gradient via branchless coefficient LUT.
    static FASTLED_FORCE_INLINE fl::i32 grad(int hash, fl::i32 x, fl::i32 y) {
        struct GradCoeff { fl::i8 cx; fl::i8 cy; };
        constexpr GradCoeff lut[16] = {
            { 1,  1}, {-1,  1}, { 1, -1}, {-1, -1},
            { 1,  0}, {-1,  0}, { 1,  0}, {-1,  0},
            { 0,  1}, { 0, -1}, { 0,  1}, { 0, -1},
            { 1,  1}, { 0, -1}, {-1,  1}, { 0, -1},
        };
        const GradCoeff &g = lut[hash & 15];
        return g.cx * x + g.cy * y;
    }
};

// Bridge class: connects ANIMartRIX to Context's output callbacks
struct Context::Engine : public animartrix_detail::ANIMartRIX {
    Context *mCtx;

    // Persistent PixelLUT for Chasing_Spirals_Q31.
    // Depends only on grid geometry (polar_theta, distance, radial_filter_radius),
    // which is constant across frames. Computed once on first use, reused every frame.
    fl::vector<ChasingSpiralPixelLUT> mChasingSpiralLUT;

    // Persistent hp_fade LUT for Perlin noise (257 entries, Q8.24 format).
    // Replaces 5 multiplies per hp_fade call with table lookup + lerp.
    // Initialized on first use by q31::Chasing_Spirals_Q31.
    fl::i32 mFadeLUT[257];
    bool mFadeLUTInitialized;

    Engine(Context *ctx) : mCtx(ctx), mFadeLUT{}, mFadeLUTInitialized(false) {}

    void setPixelColorInternal(int x, int y,
                               animartrix_detail::rgb pixel) override {
        fl::u16 idx = mCtx->xyMapFn(x, y, mCtx->xyMapUserData);
        mCtx->leds[idx] = CRGB(pixel.red, pixel.green, pixel.blue);
    }

    fl::u16 xyMap(fl::u16 x, fl::u16 y) override {
        return mCtx->xyMapFn(x, y, mCtx->xyMapUserData);
    }
};

inline Context::~Context() {
    delete mEngine;
}

// Initialize context with grid dimensions
inline void init(Context &ctx, int w, int h) {
    if (!ctx.mEngine) {
        ctx.mEngine = new Context::Engine(&ctx);
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

// ============================================================
// Animation free functions (Visualizers)
// Each delegates to the corresponding ANIMartRIX method.
// ============================================================

inline void Rotating_Blob(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 0.1;
    e->timings.ratio[1] = 0.03;
    e->timings.ratio[2] = 0.03;
    e->timings.ratio[3] = 0.03;

    e->timings.offset[1] = 10;
    e->timings.offset[2] = 20;
    e->timings.offset[3] = 30;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            e->animation.offset_z = 100;
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0];
            e->animation.dist = e->distance[x][y];
            e->animation.z = e->move.linear[0];
            e->animation.low_limit = -1;
            float show1 = e->render_value(e->animation);

            e->animation.angle =
                e->polar_theta[x][y] - e->move.radial[1] + show1 / 512.0;
            e->animation.dist = e->distance[x][y] * show1 / 255.0;
            e->animation.low_limit = 0;
            e->animation.z = e->move.linear[1];
            float show2 = e->render_value(e->animation);

            e->animation.angle =
                e->polar_theta[x][y] - e->move.radial[2] + show1 / 512.0;
            e->animation.dist = e->distance[x][y] * show1 / 220.0;
            e->animation.z = e->move.linear[2];
            float show3 = e->render_value(e->animation);

            e->animation.angle =
                e->polar_theta[x][y] - e->move.radial[3] + show1 / 512.0;
            e->animation.dist = e->distance[x][y] * show1 / 200.0;
            e->animation.z = e->move.linear[3];
            float show4 = e->render_value(e->animation);

            e->pixel.red = (show2 + show4) / 2;
            e->pixel.green = show3 / 6;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Chasing_Spirals(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 0.1;
    e->timings.ratio[1] = 0.13;
    e->timings.ratio[2] = 0.16;

    e->timings.offset[1] = 10;
    e->timings.offset[2] = 20;
    e->timings.offset[3] = 30;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.angle =
                3 * e->polar_theta[x][y] + e->move.radial[0] - e->distance[x][y] / 3;
            e->animation.dist = e->distance[x][y];
            e->animation.scale_z = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.scale_x = 0.1;
            e->animation.offset_x = e->move.linear[0];
            e->animation.offset_y = 0;
            e->animation.offset_z = 0;
            e->animation.z = 0;
            float show1 = e->render_value(e->animation);

            e->animation.angle =
                3 * e->polar_theta[x][y] + e->move.radial[1] - e->distance[x][y] / 3;
            e->animation.dist = e->distance[x][y];
            e->animation.offset_x = e->move.linear[1];
            float show2 = e->render_value(e->animation);

            e->animation.angle =
                3 * e->polar_theta[x][y] + e->move.radial[2] - e->distance[x][y] / 3;
            e->animation.dist = e->distance[x][y];
            e->animation.offset_x = e->move.linear[2];
            float show3 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial_filter = (radius - e->distance[x][y]) / radius;

            e->pixel.red = 3 * show1 * radial_filter;
            e->pixel.green = show2 * radial_filter / 2;
            e->pixel.blue = show3 * radial_filter / 4;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}

inline void Rings(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 1;
    e->timings.ratio[1] = 1.1;
    e->timings.ratio[2] = 1.2;

    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.angle = 5;
            e->animation.scale_x = 0.2;
            e->animation.scale_y = 0.2;
            e->animation.scale_z = 1;
            e->animation.dist = e->distance[x][y];
            e->animation.offset_y = -e->move.linear[0];
            e->animation.offset_x = 0;
            float show1 = e->render_value(e->animation);

            e->animation.angle = 10;
            e->animation.dist = e->distance[x][y];
            e->animation.offset_y = -e->move.linear[1];
            float show2 = e->render_value(e->animation);

            e->animation.angle = 12;
            e->animation.dist = e->distance[x][y];
            e->animation.offset_y = -e->move.linear[2];
            float show3 = e->render_value(e->animation);

            e->pixel.red = show1;
            e->pixel.green = show2 / 4;
            e->pixel.blue = show3 / 4;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Waves(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 2;
    e->timings.ratio[1] = 2.1;
    e->timings.ratio[2] = 1.2;

    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.angle = e->polar_theta[x][y];
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.scale_z = 0.1;
            e->animation.dist = e->distance[x][y];
            e->animation.offset_y = 0;
            e->animation.offset_x = 0;
            e->animation.z = 2 * e->distance[x][y] - e->move.linear[0];
            float show1 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y];
            e->animation.dist = e->distance[x][y];
            e->animation.z = 2 * e->distance[x][y] - e->move.linear[1];
            float show2 = e->render_value(e->animation);

            e->pixel.red = show1;
            e->pixel.green = 0;
            e->pixel.blue = show2;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Center_Field(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 1;
    e->timings.ratio[1] = 1.1;
    e->timings.ratio[2] = 1.2;

    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.angle = e->polar_theta[x][y];
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.scale_z = 0.1;
            e->animation.dist = 5 * fl::sqrtf(e->distance[x][y]);
            e->animation.offset_y = e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.z = 0;
            float show1 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y];
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.scale_z = 0.1;
            e->animation.dist = 4 * fl::sqrtf(e->distance[x][y]);
            e->animation.offset_y = e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.z = 0;
            float show2 = e->render_value(e->animation);

            e->pixel.red = show1;
            e->pixel.green = show2;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Distance_Experiment(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 0.2;
    e->timings.ratio[1] = 0.13;
    e->timings.ratio[2] = 0.012;

    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = fl::powf(e->distance[x][y], 0.5);
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0];
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.scale_z = 0.1;
            e->animation.offset_y = e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;
            e->animation.z = 0;
            float show1 = e->render_value(e->animation);

            e->animation.dist = fl::powf(e->distance[x][y], 0.6);
            e->animation.angle = e->polar_theta[x][y] + e->move.noise_angle[2];
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.scale_z = 0.1;
            e->animation.offset_y = e->move.linear[1];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;
            e->animation.z = 0;
            float show2 = e->render_value(e->animation);

            e->pixel.red = show1 + show2;
            e->pixel.green = show2;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Caleido1(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.003;
    e->timings.ratio[0] = 0.02;
    e->timings.ratio[1] = 0.03;
    e->timings.ratio[2] = 0.04;
    e->timings.ratio[3] = 0.05;
    e->timings.ratio[4] = 0.6;
    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[0]) / 3;
            e->animation.angle = 3 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[0] + e->move.radial[4];
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.scale_z = 0.1;
            e->animation.offset_y = 2 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;
            e->animation.z = e->move.linear[0];
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[1]) / 3;
            e->animation.angle = 4 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[1] + e->move.radial[4];
            e->animation.offset_x = 2 * e->move.linear[1];
            e->animation.z = e->move.linear[1];
            float show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[2]) / 3;
            e->animation.angle = 5 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[2] + e->move.radial[4];
            e->animation.offset_y = 2 * e->move.linear[2];
            e->animation.z = e->move.linear[2];
            float show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[3]) / 3;
            e->animation.angle = 4 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[3] + e->move.radial[4];
            e->animation.offset_x = 2 * e->move.linear[3];
            e->animation.z = e->move.linear[3];
            float show4 = e->render_value(e->animation);

            e->pixel.red = show1;
            e->pixel.green = show3 * e->distance[x][y] / 10;
            e->pixel.blue = (show2 + show4) / 2;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Caleido2(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.002;
    e->timings.ratio[0] = 0.02;
    e->timings.ratio[1] = 0.03;
    e->timings.ratio[2] = 0.04;
    e->timings.ratio[3] = 0.05;
    e->timings.ratio[4] = 0.6;
    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[0]) / 3;
            e->animation.angle = 2 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[0] + e->move.radial[4];
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.scale_z = 0.1;
            e->animation.offset_y = 2 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;
            e->animation.z = e->move.linear[0];
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[1]) / 3;
            e->animation.angle = 2 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[1] + e->move.radial[4];
            e->animation.offset_x = 2 * e->move.linear[1];
            e->animation.z = e->move.linear[1];
            float show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[2]) / 3;
            e->animation.angle = 2 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[2] + e->move.radial[4];
            e->animation.offset_y = 2 * e->move.linear[2];
            e->animation.z = e->move.linear[2];
            float show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[3]) / 3;
            e->animation.angle = 2 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[3] + e->move.radial[4];
            e->animation.offset_x = 2 * e->move.linear[3];
            e->animation.z = e->move.linear[3];
            float show4 = e->render_value(e->animation);

            e->pixel.red = show1;
            e->pixel.green = show3 * e->distance[x][y] / 10;
            e->pixel.blue = (show2 + show4) / 2;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Caleido3(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.004;
    e->timings.ratio[0] = 0.02;
    e->timings.ratio[1] = 0.03;
    e->timings.ratio[2] = 0.04;
    e->timings.ratio[3] = 0.05;
    e->timings.ratio[4] = 0.6;
    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[0]) / 3;
            e->animation.angle = 2 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[0] + e->move.radial[4];
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.scale_z = 0.1;
            e->animation.offset_y = 2 * e->move.linear[0];
            e->animation.offset_x = 2 * e->move.linear[1];
            e->animation.offset_z = 0;
            e->animation.z = e->move.linear[0];
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[1]) / 3;
            e->animation.angle = 2 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[1] + e->move.radial[4];
            e->animation.offset_x = 2 * e->move.linear[1];
            e->animation.offset_y = show1 / 20.0;
            e->animation.z = e->move.linear[1];
            float show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[2]) / 3;
            e->animation.angle = 2 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[2] + e->move.radial[4];
            e->animation.offset_y = 2 * e->move.linear[2];
            e->animation.offset_x = show2 / 20.0;
            e->animation.z = e->move.linear[2];
            float show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (2 + e->move.directional[3]) / 3;
            e->animation.angle = 2 * e->polar_theta[x][y] +
                                 3 * e->move.noise_angle[3] + e->move.radial[4];
            e->animation.offset_x = 2 * e->move.linear[3];
            e->animation.offset_y = show3 / 20.0;
            e->animation.z = e->move.linear[3];
            float show4 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;

            e->pixel.red = show1 * (y + 1) / e->num_y;
            e->pixel.green = show3 * e->distance[x][y] / 10;
            e->pixel.blue = (show2 + show4) / 2;
            if (e->distance[x][y] > radius) {
                e->pixel.red = 0;
                e->pixel.green = 0;
                e->pixel.blue = 0;
            }

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Lava1(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.0015;
    e->timings.ratio[0] = 4;
    e->timings.ratio[1] = 1;
    e->timings.ratio[2] = 1;
    e->timings.ratio[3] = 0.05;
    e->timings.ratio[4] = 0.6;
    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y] * 0.8;
            e->animation.angle = e->polar_theta[x][y];
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.12;
            e->animation.scale_z = 0.01;
            e->animation.offset_y = -e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;
            e->animation.z = 30;
            float show1 = e->render_value(e->animation);

            e->animation.offset_y = -e->move.linear[1];
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.12;
            e->animation.offset_x = show1 / 100;
            e->animation.offset_y += show1 / 100;
            float show2 = e->render_value(e->animation);

            e->animation.offset_y = -e->move.linear[2];
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.12;
            e->animation.offset_x = show2 / 100;
            e->animation.offset_y += show2 / 100;
            float show3 = e->render_value(e->animation);

            float linear = (y) / (e->num_y - 1.f);

            e->pixel.red = linear * show2;
            e->pixel.green = 0.1 * linear * (show2 - show3);
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Scaledemo1(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.000001;
    e->timings.ratio[0] = 0.4;
    e->timings.ratio[1] = 0.32;
    e->timings.ratio[2] = 0.10;
    e->timings.ratio[3] = 0.05;
    e->timings.ratio[4] = 0.6;
    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = 0.3 * e->distance[x][y] * 0.8;
            e->animation.angle = 3 * e->polar_theta[x][y] + e->move.radial[2];
            e->animation.scale_x = 0.1 + (e->move.noise_angle[0]) / 10;
            e->animation.scale_y = 0.1 + (e->move.noise_angle[1]) / 10;
            e->animation.scale_z = 0.01;
            e->animation.offset_y = 0;
            e->animation.offset_x = 0;
            e->animation.offset_z = 100 * e->move.linear[0];
            e->animation.z = 30;
            float show1 = e->render_value(e->animation);

            e->animation.angle = 3;
            float show2 = e->render_value(e->animation);

            float dist = 1;
            e->pixel.red = show1 * dist;
            e->pixel.green = (show1 - show2) * dist * 0.3;
            e->pixel.blue = (show2 - show1) * dist;

            if (e->distance[x][y] > 16) {
                e->pixel.red = 0;
                e->pixel.green = 0;
                e->pixel.blue = 0;
            }

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Yves(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->a = ::micros();

    e->timings.master_speed = 0.001;
    e->timings.ratio[0] = 3;
    e->timings.ratio[1] = 2;
    e->timings.ratio[2] = 1;
    e->timings.ratio[3] = 0.13;
    e->timings.ratio[4] = 0.15;
    e->timings.ratio[5] = 0.03;
    e->timings.ratio[6] = 0.025;
    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;
    e->timings.offset[5] = 500;
    e->timings.offset[6] = 600;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y];
            e->animation.angle =
                e->polar_theta[x][y] + 2 * PI + e->move.noise_angle[5];
            e->animation.scale_x = 0.08;
            e->animation.scale_y = 0.08;
            e->animation.scale_z = 0.08;
            e->animation.offset_y = -e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;
            e->animation.z = 0;
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle =
                e->polar_theta[x][y] + 2 * PI + e->move.noise_angle[6];
            ;
            e->animation.scale_x = 0.08;
            e->animation.scale_y = 0.08;
            e->animation.scale_z = 0.08;
            e->animation.offset_y = -e->move.linear[1];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;
            e->animation.z = 0;
            float show2 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + show1 / 100 +
                                 e->move.noise_angle[3] + e->move.noise_angle[4];
            e->animation.dist = e->distance[x][y] + show2 / 50;
            e->animation.offset_y = -e->move.linear[2];

            e->animation.offset_y += show1 / 100;
            e->animation.offset_x += show2 / 100;

            float show3 = e->render_value(e->animation);

            e->animation.offset_y = 0;
            e->animation.offset_x = 0;

            float show4 = e->render_value(e->animation);

            e->pixel.red = show3;
            e->pixel.green = show3 * show4 / 255;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Spiralus(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.0011;
    e->timings.ratio[0] = 1.5;
    e->timings.ratio[1] = 2.3;
    e->timings.ratio[2] = 3;
    e->timings.ratio[3] = 0.05;
    e->timings.ratio[4] = 0.2;
    e->timings.ratio[5] = 0.03;
    e->timings.ratio[6] = 0.025;
    e->timings.ratio[7] = 0.021;
    e->timings.ratio[8] = 0.027;
    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;
    e->timings.offset[5] = 500;
    e->timings.offset[6] = 600;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y];
            e->animation.angle = 2 * e->polar_theta[x][y] + e->move.noise_angle[5] +
                                 e->move.directional[3] * e->move.noise_angle[6] *
                                     e->animation.dist / 10;
            e->animation.scale_x = 0.08;
            e->animation.scale_y = 0.08;
            e->animation.scale_z = 0.02;
            e->animation.offset_y = -e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;
            e->animation.z = e->move.linear[1];
            float show1 = e->render_value(e->animation);

            e->animation.angle = 2 * e->polar_theta[x][y] + e->move.noise_angle[7] +
                                 e->move.directional[5] * e->move.noise_angle[8] *
                                     e->animation.dist / 10;
            e->animation.offset_y = -e->move.linear[1];
            e->animation.z = e->move.linear[2];
            float show2 = e->render_value(e->animation);

            e->animation.angle = 2 * e->polar_theta[x][y] + e->move.noise_angle[6] +
                                 e->move.directional[6] * e->move.noise_angle[7] *
                                     e->animation.dist / 10;
            e->animation.offset_y = e->move.linear[2];
            e->animation.z = e->move.linear[0];
            float show3 = e->render_value(e->animation);

            float f = 1;

            e->pixel.red = f * (show1 + show2);
            e->pixel.green = f * (show1 - show2);
            e->pixel.blue = f * (show3 - show1);

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Spiralus2(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.0015;
    e->timings.ratio[0] = 1.5;
    e->timings.ratio[1] = 2.3;
    e->timings.ratio[2] = 3;
    e->timings.ratio[3] = 0.05;
    e->timings.ratio[4] = 0.2;
    e->timings.ratio[5] = 0.05;
    e->timings.ratio[6] = 0.055;
    e->timings.ratio[7] = 0.06;
    e->timings.ratio[8] = 0.027;
    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;
    e->timings.offset[5] = 500;
    e->timings.offset[6] = 600;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y];
            e->animation.angle = 5 * e->polar_theta[x][y] + e->move.noise_angle[5] +
                                 e->move.directional[3] * e->move.noise_angle[6] *
                                     e->animation.dist / 10;
            e->animation.scale_x = 0.08;
            e->animation.scale_y = 0.08;
            e->animation.scale_z = 0.02;
            e->animation.offset_y = -e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;
            e->animation.z = e->move.linear[1];
            float show1 = e->render_value(e->animation);

            e->animation.angle = 6 * e->polar_theta[x][y] + e->move.noise_angle[7] +
                                 e->move.directional[5] * e->move.noise_angle[8] *
                                     e->animation.dist / 10;
            e->animation.offset_y = -e->move.linear[1];
            e->animation.z = e->move.linear[2];
            float show2 = e->render_value(e->animation);

            e->animation.angle = 6 * e->polar_theta[x][y] + e->move.noise_angle[6] +
                                 e->move.directional[6] * e->move.noise_angle[7] *
                                     e->animation.dist / 10;
            e->animation.offset_y = e->move.linear[2];
            e->animation.z = e->move.linear[0];
            e->animation.dist = e->distance[x][y] * 0.8;
            float show3 = e->render_value(e->animation);

            float f = 1;

            e->pixel.red = f * (show1 + show2);
            e->pixel.green = f * (show1 - show2);
            e->pixel.blue = f * (show3 - show1);

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Hot_Blob(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();
    e->run_default_oscillators(0.001);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];

            e->animation.scale_x = 0.07 + e->move.directional[0] * 0.002;
            e->animation.scale_y = 0.07;

            e->animation.offset_y = -e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;

            e->animation.z = 0;
            e->animation.low_limit = -1;
            float show1 = e->render_value(e->animation);

            e->animation.offset_y = -e->move.linear[1];
            float show3 = e->render_value(e->animation);

            e->animation.offset_x = show3 / 20;
            e->animation.offset_y = -e->move.linear[0] / 2 + show1 / 70;
            e->animation.low_limit = 0;
            float show2 = e->render_value(e->animation);

            e->animation.offset_x = show3 / 20;
            e->animation.offset_y = -e->move.linear[0] / 2 + show1 / 70;
            e->animation.z = 100;
            float show4 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->animation.dist) / e->animation.dist;

            float linear = (y + 1) / (e->num_y - 1.f);

            e->pixel.red = radial * show2;
            e->pixel.green = linear * radial * 0.3 * (show2 - show4);
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Zoom(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->run_default_oscillators();
    e->timings.master_speed = 0.003;
    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = (e->distance[x][y] * e->distance[x][y]) / 2;
            e->animation.angle = e->polar_theta[x][y];

            e->animation.scale_x = 0.005;
            e->animation.scale_y = 0.005;

            e->animation.offset_y = -10 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;

            e->animation.z = 0;
            e->animation.low_limit = 0;
            float show1 = e->render_value(e->animation);

            float linear = 1;

            e->pixel.red = show1 * linear;
            e->pixel.green = 0;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Slow_Fade(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->run_default_oscillators();
    e->timings.master_speed = 0.00005;
    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist =
                fl::sqrtf(e->distance[x][y]) * 0.7 * (e->move.directional[0] + 1.5);
            e->animation.angle =
                e->polar_theta[x][y] - e->move.radial[0] + e->distance[x][y] / 5;

            e->animation.scale_x = 0.11;
            e->animation.scale_y = 0.11;

            e->animation.offset_y = -50 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0;

            e->animation.z = e->move.linear[0];
            e->animation.low_limit = -0.1;
            e->animation.high_limit = 1;
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->animation.dist * 1.1;
            e->animation.angle += e->move.noise_angle[0] / 10;
            float show2 = e->render_value(e->animation);

            e->animation.dist = e->animation.dist * 1.1;
            e->animation.angle += e->move.noise_angle[1] / 10;
            float show3 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * show1;
            e->pixel.green = radial * (show1 - show2) / 6;
            e->pixel.blue = radial * (show1 - show3) / 5;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Polar_Waves(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.5;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.0031;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = (e->distance[x][y]);
            e->animation.angle =
                e->polar_theta[x][y] - e->animation.dist * 0.1 + e->move.radial[0];
            e->animation.z = (e->animation.dist * 1.5) - 10 * e->move.linear[0];
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.15;
            e->animation.offset_x = e->move.linear[0];

            float show1 = e->render_value(e->animation);
            e->animation.angle =
                e->polar_theta[x][y] - e->animation.dist * 0.1 + e->move.radial[1];
            e->animation.z = (e->animation.dist * 1.5) - 10 * e->move.linear[1];
            e->animation.offset_x = e->move.linear[1];

            float show2 = e->render_value(e->animation);
            e->animation.angle =
                e->polar_theta[x][y] - e->animation.dist * 0.1 + e->move.radial[2];
            e->animation.z = (e->animation.dist * 1.5) - 10 * e->move.linear[2];
            e->animation.offset_x = e->move.linear[2];

            float show3 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * show1;
            e->pixel.green = radial * show2;
            e->pixel.blue = radial * show3;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void RGB_Blobs(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.2;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.0031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0] +
                                 e->move.noise_angle[0] + e->move.noise_angle[3];
            e->animation.z = (fl::sqrtf(e->animation.dist));
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = 10;
            e->animation.offset_x = 10 * e->move.linear[0];
            float show1 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + e->move.radial[1] +
                                 e->move.noise_angle[1] + e->move.noise_angle[4];
            e->animation.offset_x = 11 * e->move.linear[1];
            e->animation.offset_z = 100;
            float show2 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + e->move.radial[2] +
                                 e->move.noise_angle[2] + e->move.noise_angle[5];
            e->animation.offset_x = 12 * e->move.linear[2];
            e->animation.offset_z = 300;
            float show3 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * show1;
            e->pixel.green = radial * show2;
            e->pixel.blue = radial * show3;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void RGB_Blobs2(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.12;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.0031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0] +
                                 e->move.noise_angle[0] + e->move.noise_angle[3] +
                                 e->move.noise_angle[1];
            e->animation.z = (fl::sqrtf(e->animation.dist));
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = 10;
            e->animation.offset_x = 10 * e->move.linear[0];
            float show1 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + e->move.radial[1] +
                                 e->move.noise_angle[1] + e->move.noise_angle[4] +
                                 e->move.noise_angle[2];
            e->animation.offset_x = 11 * e->move.linear[1];
            e->animation.offset_z = 100;
            float show2 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + e->move.radial[2] +
                                 e->move.noise_angle[2] + e->move.noise_angle[5] +
                                 e->move.noise_angle[3];
            e->animation.offset_x = 12 * e->move.linear[2];
            e->animation.offset_z = 300;
            float show3 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * (show1 - show3);
            e->pixel.green = radial * (show2 - show1);
            e->pixel.blue = radial * (show3 - show2);

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void RGB_Blobs3(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.12;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.0031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y] + e->move.noise_angle[4];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0] +
                                 e->move.noise_angle[0] + e->move.noise_angle[3] +
                                 e->move.noise_angle[1];
            e->animation.z = (fl::sqrtf(e->animation.dist));
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = 10;
            e->animation.offset_x = 10 * e->move.linear[0];
            float show1 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + e->move.radial[1] +
                                 e->move.noise_angle[1] + e->move.noise_angle[4] +
                                 e->move.noise_angle[2];
            e->animation.offset_x = 11 * e->move.linear[1];
            e->animation.offset_z = 100;
            float show2 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + e->move.radial[2] +
                                 e->move.noise_angle[2] + e->move.noise_angle[5] +
                                 e->move.noise_angle[3];
            e->animation.offset_x = 12 * e->move.linear[2];
            e->animation.offset_z = 300;
            float show3 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * (show1 + show3) * 0.5 * e->animation.dist / 5;
            e->pixel.green = radial * (show2 + show1) * 0.5 * y / 15;
            e->pixel.blue = radial * (show3 + show2) * 0.5 * x / 15;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void RGB_Blobs4(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.02;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.0031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y] + e->move.noise_angle[4];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0] +
                                 e->move.noise_angle[0] + e->move.noise_angle[3] +
                                 e->move.noise_angle[1];
            e->animation.z = 3 + fl::sqrtf(e->animation.dist);
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = 10;
            e->animation.offset_x = 50 * e->move.linear[0];
            float show1 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + e->move.radial[1] +
                                 e->move.noise_angle[1] + e->move.noise_angle[4] +
                                 e->move.noise_angle[2];
            e->animation.offset_x = 50 * e->move.linear[1];
            e->animation.offset_z = 100;
            float show2 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + e->move.radial[2] +
                                 e->move.noise_angle[2] + e->move.noise_angle[5] +
                                 e->move.noise_angle[3];
            e->animation.offset_x = 50 * e->move.linear[2];
            e->animation.offset_z = 300;
            float show3 = e->render_value(e->animation);

            float radius = 23;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * (show1 + show3) * 0.5 * e->animation.dist / 5;
            e->pixel.green = radial * (show2 + show1) * 0.5 * y / 15;
            e->pixel.blue = radial * (show3 + show2) * 0.5 * x / 15;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void RGB_Blobs5(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.02;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.0031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y] + e->move.noise_angle[4];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0] +
                                 e->move.noise_angle[0] + e->move.noise_angle[3] +
                                 e->move.noise_angle[1];
            e->animation.z = 3 + fl::sqrtf(e->animation.dist);
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_z = 10;
            e->animation.offset_x = 50 * e->move.linear[0];
            float show1 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + e->move.radial[1] +
                                 e->move.noise_angle[1] + e->move.noise_angle[4] +
                                 e->move.noise_angle[2];
            e->animation.offset_x = 50 * e->move.linear[1];
            e->animation.offset_z = 100;
            float show2 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + e->move.radial[2] +
                                 e->move.noise_angle[2] + e->move.noise_angle[5] +
                                 e->move.noise_angle[3];
            e->animation.offset_x = 50 * e->move.linear[2];
            e->animation.offset_z = 300;
            float show3 = e->render_value(e->animation);

            float radius = 23;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * (show1 + show3) * 0.5 * e->animation.dist / 5;
            e->pixel.green = radial * (show2 + show1) * 0.5 * y / 15;
            e->pixel.blue = radial * (show3 + show2) * 0.5 * x / 15;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Big_Caleido(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.02;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.0031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = e->distance[x][y];
            e->animation.angle = 5 * e->polar_theta[x][y] +
                                 5 * e->move.noise_angle[0] +
                                 e->animation.dist * 0.1;
            e->animation.z = 5;
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_z = 50 * e->move.linear[0];
            e->animation.offset_x = 50 * e->move.noise_angle[0];
            e->animation.offset_y = 50 * e->move.noise_angle[1];
            float show1 = e->render_value(e->animation);

            e->animation.angle = 6 * e->polar_theta[x][y] +
                                 5 * e->move.noise_angle[1] +
                                 e->animation.dist * 0.15;
            e->animation.z = 5;
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_z = 50 * e->move.linear[1];
            e->animation.offset_x = 50 * e->move.noise_angle[1];
            e->animation.offset_y = 50 * e->move.noise_angle[2];
            float show2 = e->render_value(e->animation);

            e->animation.angle = 5;
            e->animation.z = 5;
            e->animation.scale_x = 0.10;
            e->animation.scale_y = 0.10;
            e->animation.offset_z = 10 * e->move.linear[2];
            e->animation.offset_x = 10 * e->move.noise_angle[2];
            e->animation.offset_y = 10 * e->move.noise_angle[3];
            float show3 = e->render_value(e->animation);

            e->animation.angle = 15;
            e->animation.z = 15;
            e->animation.scale_x = 0.10;
            e->animation.scale_y = 0.10;
            e->animation.offset_z = 10 * e->move.linear[3];
            e->animation.offset_x = 10 * e->move.noise_angle[3];
            e->animation.offset_y = 10 * e->move.noise_angle[4];
            float show4 = e->render_value(e->animation);

            e->animation.angle = 2;
            e->animation.z = 15;
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.15;
            e->animation.offset_z = 10 * e->move.linear[4];
            e->animation.offset_x = 10 * e->move.noise_angle[4];
            e->animation.offset_y = 10 * e->move.noise_angle[5];
            float show5 = e->render_value(e->animation);

            e->pixel.red = show1 - show4;
            e->pixel.green = show2 - show5;
            e->pixel.blue = show3 - show2 + show1;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void SpiralMatrix1(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.02;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.0031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x / 2; x++) {
        for (int y = 0; y < e->num_y / 2; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 5 * e->move.noise_angle[0];
            e->animation.z = 5;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = 50 * e->move.linear[0];
            e->animation.offset_x = 150 * e->move.directional[0];
            e->animation.offset_y = 150 * e->move.directional[1];
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 4 * e->move.noise_angle[1];
            e->animation.z = 15;
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.15;
            e->animation.offset_z = 50 * e->move.linear[1];
            e->animation.offset_x = 150 * e->move.directional[1];
            e->animation.offset_y = 150 * e->move.directional[2];
            float show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 5 * e->move.noise_angle[2];
            e->animation.z = 25;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = 50 * e->move.linear[2];
            e->animation.offset_x = 150 * e->move.directional[2];
            e->animation.offset_y = 150 * e->move.directional[3];
            float show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 5 * e->move.noise_angle[3];
            e->animation.z = 35;
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.15;
            e->animation.offset_z = 50 * e->move.linear[3];
            e->animation.offset_x = 150 * e->move.directional[3];
            e->animation.offset_y = 150 * e->move.directional[4];
            float show4 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 5 * e->move.noise_angle[4];
            e->animation.z = 45;
            e->animation.scale_x = 0.2;
            e->animation.scale_y = 0.2;
            e->animation.offset_z = 50 * e->move.linear[4];
            e->animation.offset_x = 150 * e->move.directional[4];
            e->animation.offset_y = 150 * e->move.directional[5];
            float show5 = e->render_value(e->animation);

            e->pixel.red = show1 + show2;
            e->pixel.green = show3 + show4;
            e->pixel.blue = show5;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);

            e->setPixelColorInternal((e->num_x - 1) - x, y, e->pixel);
            e->setPixelColorInternal((e->num_x - 1) - x, (e->num_y - 1) - y, e->pixel);
            e->setPixelColorInternal(x, (e->num_y - 1) - y, e->pixel);
        }
    }
}
inline void SpiralMatrix2(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.03;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y] * (e->move.directional[0]);
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0];
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 5 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[1];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[1];
            e->animation.z = 50;
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.offset_z = 5 * e->move.linear[1];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[2];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[2];
            e->animation.z = 500;
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_z = 5 * e->move.linear[2];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show3 = e->render_value(e->animation);

            e->pixel.red = show1;
            e->pixel.green = show2;
            e->pixel.blue = show3;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void SpiralMatrix3(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.02;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.offset_y = -20 * e->move.linear[0];
            ;
            e->animation.low_limit = -1;
            e->animation.high_limit = 1;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 500;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.offset_y = -20 * e->move.linear[0];
            ;
            e->animation.low_limit = -1;
            e->animation.high_limit = 1;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 50;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 0;
            e->animation.offset_x = 500 + e->show1 / 20;
            e->animation.offset_y = -4 * e->move.linear[0] + e->show2 / 20;
            e->animation.low_limit = 0;
            e->animation.high_limit = 1;
            e->show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 50;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 0;
            e->animation.offset_x = 500 + e->show1 / 18;
            e->animation.offset_y = -4 * e->move.linear[0] + e->show2 / 18;
            e->animation.low_limit = 0;
            e->animation.high_limit = 1;
            e->show4 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 50;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 0;
            e->animation.offset_x = 500 + e->show1 / 19;
            e->animation.offset_y = -4 * e->move.linear[0] + e->show2 / 19;
            e->animation.low_limit = 0.3;
            e->animation.high_limit = 1;
            e->show5 = e->render_value(e->animation);

            e->pixel.red = e->show4;
            e->pixel.green = e->show3;
            e->pixel.blue = e->show5;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void SpiralMatrix4(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.02;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0033;
    e->timings.ratio[4] = 0.0036;
    e->timings.ratio[5] = 0.0039;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.offset_y = -20 * e->move.linear[0];
            ;
            e->animation.low_limit = 0;
            e->animation.high_limit = 1;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 500;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.offset_y = -40 * e->move.linear[0];
            ;
            e->animation.low_limit = 0;
            e->animation.high_limit = 1;
            e->show2 = e->render_value(e->animation);

            e->pixel.red = e->add(e->show2, e->show1);
            e->pixel.green = 0;
            e->pixel.blue = e->colordodge(e->show2, e->show1);

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void SpiralMatrix5(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.03;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0053;
    e->timings.ratio[4] = 0.0056;
    e->timings.ratio[5] = 0.0059;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y] * (e->move.directional[0]);
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0];
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 5 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[1];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[1];
            e->animation.z = 50;
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.offset_z = 5 * e->move.linear[1];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[2];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[2];
            e->animation.z = 500;
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_z = 5 * e->move.linear[2];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (e->move.directional[3]);
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[3];
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 5 * e->move.linear[3];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show4 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[4];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[4];
            e->animation.z = 50;
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.offset_z = 5 * e->move.linear[4];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show5 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[5];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[5];
            e->animation.z = 500;
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_z = 5 * e->move.linear[5];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show6 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * e->add(show1, show4);
            e->pixel.green = radial * e->colordodge(show2, show5);
            e->pixel.blue = radial * e->screen(show3, show6);

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void SpiralMatrix6(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.03;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0053;
    e->timings.ratio[4] = 0.0056;
    e->timings.ratio[5] = 0.0059;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 0.7;

            e->animation.dist = e->distance[x][y] * (e->move.directional[0]) * s;
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[0];
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 5 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[1] * s;
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[1];
            e->animation.z = 50;
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.offset_z = 5 * e->move.linear[1];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[2] * s;
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[2];
            e->animation.z = 500;
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_z = 5 * e->move.linear[2];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * (e->move.directional[3]) * s;
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[3];
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 5 * e->move.linear[3];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show4 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[4] * s;
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[4];
            e->animation.z = 50;
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.offset_z = 5 * e->move.linear[4];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show5 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * e->move.directional[5] * s;
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[5];
            e->animation.z = 500;
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_z = 5 * e->move.linear[5];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            float show6 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->show7 = e->screen(show1, show4);
            e->show8 = e->colordodge(show2, show5);
            e->show9 = e->screen(show3, show6);

            e->pixel.red = radial * (e->show7 + e->show8);
            e->pixel.green = 0;
            e->pixel.blue = radial * e->show9;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void SpiralMatrix8(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.005;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0053;
    e->timings.ratio[4] = 0.0056;
    e->timings.ratio[5] = 0.01;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 2;
            e->animation.z = 5;
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.15;
            e->animation.offset_z = 0;
            e->animation.offset_y = 50 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.low_limit = 0;
            float show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 2;
            e->animation.z = 150;
            e->animation.offset_x = -50 * e->move.linear[0];
            float show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 1;
            e->animation.z = 550;
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.15;
            e->animation.offset_x = 0;
            e->animation.offset_y = -50 * e->move.linear[1];
            float show4 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 1;
            e->animation.z = 1250;
            e->animation.scale_x = 0.15;
            e->animation.scale_y = 0.15;
            e->animation.offset_x = 0;
            e->animation.offset_y = 50 * e->move.linear[1];
            float show5 = e->render_value(e->animation);

            e->show3 = e->add(show1, show2);
            e->show6 = e->screen(show4, show5);

            e->pixel.red = e->show3;
            e->pixel.green = 0;
            e->pixel.blue = e->show6;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void SpiralMatrix9(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.005;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0053;
    e->timings.ratio[4] = 0.0056;
    e->timings.ratio[5] = 0.0059;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_y = -30 * e->move.linear[0];
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.low_limit = -1;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 50;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_y = -30 * e->move.linear[1];
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.low_limit = -1;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 2 + (e->show1 / 255) * PI;
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_y = -10 * e->move.linear[0];
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 2 + (e->show2 / 255) * PI;
            ;
            e->animation.z = 5;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_y = -20 * e->move.linear[0];
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.low_limit = 0;
            e->show4 = e->render_value(e->animation);

            e->show5 = e->screen(e->show4, e->show3);
            e->show6 = e->colordodge(e->show5, e->show3);

            float linear1 = y / 32.f;
            float linear2 = (32 - y) / 32.f;

            e->pixel.red = e->show5 * linear1;
            e->pixel.green = 0;
            e->pixel.blue = e->show6 * linear2;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void SpiralMatrix10(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.006;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0053;
    e->timings.ratio[4] = 0.0056;
    e->timings.ratio[5] = 0.0059;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float scale = 0.6;

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.09 * scale;
            e->animation.scale_y = 0.09 * scale;
            e->animation.offset_y = -30 * e->move.linear[0];
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.low_limit = -1;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 50;
            e->animation.scale_x = 0.09 * scale;
            e->animation.scale_y = 0.09 * scale;
            e->animation.offset_y = -30 * e->move.linear[1];
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.low_limit = -1;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 2 + (e->show1 / 255) * PI;
            e->animation.z = 5;
            e->animation.scale_x = 0.09 * scale;
            e->animation.scale_y = 0.09 * scale;
            e->animation.offset_y = -10 * e->move.linear[0];
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + 2 + (e->show2 / 255) * PI;
            ;
            e->animation.z = 5;
            e->animation.scale_x = 0.09 * scale;
            e->animation.scale_y = 0.09 * scale;
            e->animation.offset_y = -20 * e->move.linear[0];
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.low_limit = 0;
            e->show4 = e->render_value(e->animation);

            e->show5 = e->screen(e->show4, e->show3);
            e->show6 = e->colordodge(e->show5, e->show3);

            e->pixel.red = (e->show5 + e->show6) / 2;
            e->pixel.green = (e->show5 - 50) + (e->show6 / 16);
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Complex_Kaleido(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.009;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0053;
    e->timings.ratio[4] = 0.0056;
    e->timings.ratio[5] = 0.0059;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 5 * e->polar_theta[x][y] + 10 * e->move.radial[0] +
                                 e->animation.dist / 2;
            e->animation.z = 5;
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.offset_z = 0;
            e->animation.offset_x = -30 * e->move.linear[0];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = -5 * e->polar_theta[x][y] + 12 * e->move.radial[1] +
                                 e->animation.dist / 2;
            e->animation.z = 500;
            e->animation.scale_x = 0.07;
            e->animation.scale_y = 0.07;
            e->animation.offset_z = 0;
            e->animation.offset_x = -30 * e->move.linear[1];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = -5 * e->polar_theta[x][y] + 12 * e->move.radial[2] +
                                 e->animation.dist / 2;
            e->animation.z = 500;
            e->animation.scale_x = 0.05;
            e->animation.scale_y = 0.05;
            e->animation.offset_z = 0;
            e->animation.offset_x = -40 * e->move.linear[2];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 5 * e->polar_theta[x][y] + 12 * e->move.radial[3] +
                                 e->animation.dist / 2;
            e->animation.z = 500;
            e->animation.scale_x = 0.09;
            e->animation.scale_y = 0.09;
            e->animation.offset_z = 0;
            e->animation.offset_x = -35 * e->move.linear[3];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show4 = e->render_value(e->animation);

            e->show5 = e->screen(e->show4, e->show3);
            e->show6 = e->colordodge(e->show2, e->show3);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * (e->show1 + e->show2);
            e->pixel.green = 0.3 * radial * e->show6;
            e->pixel.blue = radial * e->show5;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Complex_Kaleido_2(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.009;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.0053;
    e->timings.ratio[4] = 0.0056;
    e->timings.ratio[5] = 0.0059;

    e->calculate_oscillators(e->timings);

    float size = 0.5;

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 5 * e->polar_theta[x][y] + 10 * e->move.radial[0] +
                                 e->animation.dist / 2;
            e->animation.z = 5;
            e->animation.scale_x = 0.07 * size;
            e->animation.scale_y = 0.07 * size;
            e->animation.offset_z = 0;
            e->animation.offset_x = -30 * e->move.linear[0];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = -5 * e->polar_theta[x][y] + 12 * e->move.radial[1] +
                                 e->animation.dist / 2;
            e->animation.z = 500;
            e->animation.scale_x = 0.07 * size;
            e->animation.scale_y = 0.07 * size;
            e->animation.offset_z = 0;
            e->animation.offset_x = -30 * e->move.linear[1];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = -5 * e->polar_theta[x][y] + 12 * e->move.radial[2] +
                                 e->animation.dist / 2;
            e->animation.z = 500;
            e->animation.scale_x = 0.05 * size;
            e->animation.scale_y = 0.05 * size;
            e->animation.offset_z = 0;
            e->animation.offset_x = -40 * e->move.linear[2];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 5 * e->polar_theta[x][y] + 12 * e->move.radial[3] +
                                 e->animation.dist / 2;
            e->animation.z = 500;
            e->animation.scale_x = 0.09 * size;
            e->animation.scale_y = 0.09 * size;
            e->animation.offset_z = 0;
            e->animation.offset_x = -35 * e->move.linear[3];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show4 = e->render_value(e->animation);

            e->show5 = e->screen(e->show4, e->show3);
            e->show6 = e->colordodge(e->show2, e->show3);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = radial * (e->show1 + e->show2);
            e->pixel.green = 0.3 * radial * e->show6;
            e->pixel.blue = radial * e->show5;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Complex_Kaleido_3(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.001;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.033;
    e->timings.ratio[4] = 0.037;
    e->timings.ratio[5] = 0.038;
    e->timings.ratio[5] = 0.041;

    e->calculate_oscillators(e->timings);

    float size = 0.4 + e->move.directional[0] * 0.1;

    float q = 2;

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle =
                5 * e->polar_theta[x][y] + 10 * e->move.radial[0] +
                e->animation.dist / (((e->move.directional[0] + 3) * 2)) +
                e->move.noise_angle[0] * q;
            e->animation.z = 5;
            e->animation.scale_x = 0.08 * size * (e->move.directional[0] + 1.5);
            e->animation.scale_y = 0.07 * size;
            e->animation.offset_z = -10 * e->move.linear[0];
            e->animation.offset_x = -30 * e->move.linear[0];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle =
                -5 * e->polar_theta[x][y] + 10 * e->move.radial[1] +
                e->animation.dist / (((e->move.directional[1] + 3) * 2)) +
                e->move.noise_angle[1] * q;
            e->animation.z = 500;
            e->animation.scale_x = 0.07 * size * (e->move.directional[1] + 1.1);
            e->animation.scale_y = 0.07 * size * (e->move.directional[2] + 1.3);
            ;
            e->animation.offset_z = -12 * e->move.linear[1];
            ;
            e->animation.offset_x = -(e->num_x - 1) * e->move.linear[1];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle =
                -5 * e->polar_theta[x][y] + 12 * e->move.radial[2] +
                e->animation.dist / (((e->move.directional[3] + 3) * 2)) +
                e->move.noise_angle[2] * q;
            e->animation.z = 500;
            e->animation.scale_x = 0.05 * size * (e->move.directional[3] + 1.5);
            ;
            e->animation.scale_y = 0.05 * size * (e->move.directional[4] + 1.5);
            ;
            e->animation.offset_z = -12 * e->move.linear[3];
            e->animation.offset_x = -40 * e->move.linear[3];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle =
                5 * e->polar_theta[x][y] + 12 * e->move.radial[3] +
                e->animation.dist / (((e->move.directional[5] + 3) * 2)) +
                e->move.noise_angle[3] * q;
            e->animation.z = 500;
            e->animation.scale_x = 0.09 * size * (e->move.directional[5] + 1.5);
            ;
            ;
            e->animation.scale_y = 0.09 * size * (e->move.directional[6] + 1.5);
            ;
            ;
            e->animation.offset_z = 0;
            e->animation.offset_x = -35 * e->move.linear[3];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show4 = e->render_value(e->animation);

            e->show5 = e->screen(e->show4, e->show3) - e->show2;
            e->show6 = e->colordodge(e->show4, e->show1);

            e->show7 = e->multiply(e->show1, e->show2);

            float linear1 = y / 32.f;

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->show7 = e->multiply(e->show1, e->show2) * linear1 * 2;
            e->show8 = e->subtract(e->show7, e->show5);

            e->pixel.green = 0.2 * e->show8;
            e->pixel.blue = e->show5 * radial;
            e->pixel.red = (1 * e->show1 + 1 * e->show2) - e->show7 / 2;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Complex_Kaleido_4(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.033;
    e->timings.ratio[4] = 0.037;
    e->timings.ratio[5] = 0.038;
    e->timings.ratio[6] = 0.041;

    e->calculate_oscillators(e->timings);

    float size = 0.6;

    float q = 1;

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 1 + e->move.directional[6] * 0.3;

            e->animation.dist = e->distance[x][y] * s;
            e->animation.angle =
                5 * e->polar_theta[x][y] + 1 * e->move.radial[0] -
                e->animation.dist / (3 + e->move.directional[0] * 0.5);
            e->animation.z = 5;
            e->animation.scale_x = 0.08 * size + (e->move.directional[0] * 0.01);
            e->animation.scale_y = 0.07 * size + (e->move.directional[1] * 0.01);
            e->animation.offset_z = -10 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] * s;
            e->animation.angle =
                5 * e->polar_theta[x][y] + 1 * e->move.radial[1] +
                e->animation.dist / (3 + e->move.directional[1] * 0.5);
            e->animation.z = 50;
            e->animation.scale_x = 0.08 * size + (e->move.directional[1] * 0.01);
            e->animation.scale_y = 0.07 * size + (e->move.directional[2] * 0.01);
            e->animation.offset_z = -10 * e->move.linear[1];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 1;
            e->animation.z = 500;
            e->animation.scale_x = 0.2 * size;
            e->animation.scale_y = 0.2 * size;
            e->animation.offset_z = 0;
            e->animation.offset_y = +7 * e->move.linear[3] + e->move.noise_angle[3];
            e->animation.offset_x = 0;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle =
                5 * e->polar_theta[x][y] + 12 * e->move.radial[3] +
                e->animation.dist / (((e->move.directional[5] + 3) * 2)) +
                e->move.noise_angle[3] * q;
            e->animation.z = 500;
            e->animation.scale_x = 0.09 * size * (e->move.directional[5] + 1.5);
            ;
            ;
            e->animation.scale_y = 0.09 * size * (e->move.directional[6] + 1.5);
            ;
            ;
            e->animation.offset_z = 0;
            e->animation.offset_x = -35 * e->move.linear[3];
            e->animation.offset_y = 0;
            e->animation.low_limit = 0;
            e->show4 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->show5 = ((e->show1 + e->show2)) - e->show3;
            if (e->show5 > 255)
                e->show5 = 255;
            if (e->show5 < 0)
                e->show5 = 0;

            e->show6 = e->colordodge(e->show1, e->show2);

            e->pixel.red = e->show5 * radial;
            e->pixel.blue = (64 - e->show5 - e->show3) * radial;
            e->pixel.green = 0.5 * (e->show6);

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Complex_Kaleido_5(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.033;
    e->timings.ratio[4] = 0.037;
    e->timings.ratio[5] = 0.0038;
    e->timings.ratio[6] = 0.041;

    e->calculate_oscillators(e->timings);

    float size = 0.6;

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 1 + e->move.directional[6] * 0.8;

            e->animation.dist = e->distance[x][y] * s;
            e->animation.angle = 10 * e->move.radial[6] +
                                 50 * e->move.directional[5] * e->polar_theta[x][y] -
                                 e->animation.dist / 3;
            e->animation.z = 5;
            e->animation.scale_x = 0.08 * size;
            e->animation.scale_y = 0.07 * size;
            e->animation.offset_z = -10 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_y = 0;
            e->animation.low_limit = -0.5;
            e->show1 = e->render_value(e->animation);

            float radius = e->radial_filter_radius;
            float radial = (radius - e->distance[x][y]) / e->distance[x][y];

            e->pixel.red = e->show1 * radial;
            e->pixel.green = 0;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Complex_Kaleido_6(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.033;
    e->timings.ratio[4] = 0.037;
    e->timings.ratio[5] = 0.0038;
    e->timings.ratio[6] = 0.041;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 16 * e->polar_theta[x][y] + 16 * e->move.radial[0];
            e->animation.z = 5;
            e->animation.scale_x = 0.06;
            e->animation.scale_y = 0.06;
            e->animation.offset_z = -10 * e->move.linear[0];
            e->animation.offset_y = 10 * e->move.noise_angle[0];
            e->animation.offset_x = 10 * e->move.noise_angle[4];
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y];
            e->animation.angle = 16 * e->polar_theta[x][y] + 16 * e->move.radial[1];
            e->animation.z = 500;
            e->animation.scale_x = 0.06;
            e->animation.scale_y = 0.06;
            e->animation.offset_z = -10 * e->move.linear[1];
            e->animation.offset_y = 10 * e->move.noise_angle[1];
            e->animation.offset_x = 10 * e->move.noise_angle[3];
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->pixel.red = e->show1;
            e->pixel.green = 0;
            e->pixel.blue = e->show2;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Water(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.037;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.031;
    e->timings.ratio[3] = 0.033;
    e->timings.ratio[4] = 0.037;
    e->timings.ratio[5] = 0.1;
    e->timings.ratio[6] = 0.41;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist =
                e->distance[x][y] +
                4 * fl::sinf(e->move.directional[5] * PI + (float)x / 2) +
                4 * fl::cosf(e->move.directional[6] * PI + float(y) / 2);
            e->animation.angle = 1 * e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.06;
            e->animation.scale_y = 0.06;
            e->animation.offset_z = -10 * e->move.linear[0];
            e->animation.offset_y = 10;
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = (10 + e->move.directional[0]) *
                                 fl::sinf(-e->move.radial[5] + e->move.radial[0] +
                                      (e->distance[x][y] / (3)));
            e->animation.angle = 1 * e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[0];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = (10 + e->move.directional[1]) *
                                 fl::sinf(-e->move.radial[5] + e->move.radial[1] +
                                      (e->distance[x][y] / (3)));
            e->animation.angle = 1 * e->polar_theta[x][y];
            e->animation.z = 500;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[1];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->animation.dist = (10 + e->move.directional[2]) *
                                 fl::sinf(-e->move.radial[5] + e->move.radial[2] +
                                      (e->distance[x][y] / (3)));
            e->animation.angle = 1 * e->polar_theta[x][y];
            e->animation.z = 500;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[2];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show4 = e->render_value(e->animation);

            e->pixel.blue = (0.7 * e->show2 + 0.6 * e->show3 + 0.5 * e->show4);
            e->pixel.red = e->pixel.blue - 40;
            e->pixel.green = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Parametric_Water(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.003;
    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.027;
    e->timings.ratio[2] = 0.029;
    e->timings.ratio[3] = 0.033;
    e->timings.ratio[4] = 0.037;
    e->timings.ratio[5] = 0.15;
    e->timings.ratio[6] = 0.41;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 4;
            float f = 10 + 2 * e->move.directional[0];

            e->animation.dist = (f + e->move.directional[0]) *
                                 fl::sinf(-e->move.radial[5] + e->move.radial[0] +
                                      (e->distance[x][y] / (s)));
            e->animation.angle = 1 * e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[0];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = (f + e->move.directional[1]) *
                                 fl::sinf(-e->move.radial[5] + e->move.radial[1] +
                                      (e->distance[x][y] / (s)));
            e->animation.angle = 1 * e->polar_theta[x][y];
            e->animation.z = 500;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[1];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->animation.dist = (f + e->move.directional[2]) *
                                 fl::sinf(-e->move.radial[5] + e->move.radial[2] +
                                      (e->distance[x][y] / (s)));
            e->animation.angle = 1 * e->polar_theta[x][y];
            e->animation.z = 5000;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[2];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show4 = e->render_value(e->animation);

            e->animation.dist = (f + e->move.directional[3]) *
                                 fl::sinf(-e->move.radial[5] + e->move.radial[3] +
                                      (e->distance[x][y] / (s)));
            e->animation.angle = 1 * e->polar_theta[x][y];
            e->animation.z = 2000;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[3];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show5 = e->render_value(e->animation);

            e->show6 = e->screen(e->show4, e->show5);
            e->show7 = e->screen(e->show2, e->show3);

            float radius = 40;
            float radial = (radius - e->distance[x][y]) / radius;

            e->pixel.red = e->pixel.blue - 40;
            e->pixel.green = 0;
            e->pixel.blue = (0.3 * e->show6 + 0.7 * e->show7) * radial;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Module_Experiment1(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.03;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.029;
    e->timings.ratio[3] = 0.033;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y] + 20 * e->move.directional[0];
            e->animation.angle = e->move.noise_angle[0] + e->move.noise_angle[1] +
                                 e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[2];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->pixel.red = 0;
            e->pixel.green = 0;
            e->pixel.blue = e->show1;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Module_Experiment2(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.02;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.029;
    e->timings.ratio[3] = 0.033;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist =
                e->distance[x][y] - (16 + e->move.directional[0] * 16);
            e->animation.angle = e->move.noise_angle[0] + e->move.noise_angle[1] +
                                 e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[2];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->pixel.red = e->show1;
            e->pixel.green = e->show1 - 80;
            e->pixel.blue = e->show1 - 150;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Module_Experiment3(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.029;
    e->timings.ratio[3] = 0.033;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist =
                e->distance[x][y] - (12 + e->move.directional[3] * 4);
            e->animation.angle = e->move.noise_angle[0] + e->move.noise_angle[1] +
                                 e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.1;
            e->animation.scale_y = 0.1;
            e->animation.offset_z = -10;
            e->animation.offset_y = 20 * e->move.linear[2];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->pixel.red = e->show1;
            e->pixel.green = e->show1 - 80;
            e->pixel.blue = e->show1 - 150;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Zoom2(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->run_default_oscillators();
    e->timings.master_speed = 0.003;
    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {
            e->animation.dist = (e->distance[x][y] * e->distance[x][y]) / 2;
            e->animation.angle = e->polar_theta[x][y];

            e->animation.scale_x = 0.005;
            e->animation.scale_y = 0.005;

            e->animation.offset_y = -10 * e->move.linear[0];
            e->animation.offset_x = 0;
            e->animation.offset_z = 0.1 * e->move.linear[0];

            e->animation.z = 0;
            e->animation.low_limit = 0;
            float show1 = e->render_value(e->animation);

            e->pixel.red = show1;
            e->pixel.green = 0;
            e->pixel.blue = 40 - show1;

            e->pixel = e->rgb_sanity_check(e->pixel);
            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Module_Experiment4(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.031;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.029;
    e->timings.ratio[3] = 0.033;
    e->timings.ratio[4] = 0.036;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 0.8;

            e->animation.dist = (e->distance[x][y] * e->distance[x][y]) * 0.7;
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.004 * s;
            e->animation.scale_y = 0.003 * s;
            e->animation.offset_z = 0.1 * e->move.linear[2];
            e->animation.offset_y = -20 * e->move.linear[2];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = (e->distance[x][y] * e->distance[x][y]) * 0.8;
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 50;
            e->animation.scale_x = 0.004 * s;
            e->animation.scale_y = 0.003 * s;
            e->animation.offset_z = 0.1 * e->move.linear[3];
            e->animation.offset_y = -20 * e->move.linear[3];
            e->animation.offset_x = 100;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist = (e->distance[x][y] * e->distance[x][y]) * 0.9;
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 5000;
            e->animation.scale_x = 0.004 * s;
            e->animation.scale_y = 0.003 * s;
            e->animation.offset_z = 0.1 * e->move.linear[4];
            e->animation.offset_y = -20 * e->move.linear[4];
            e->animation.offset_x = 1000;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->pixel.red = e->show1 - e->show2 - e->show3;
            e->pixel.blue = e->show2 - e->show1 - e->show3;
            e->pixel.green = e->show3 - e->show1 - e->show2;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Module_Experiment5(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.031;
    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.029;
    e->timings.ratio[3] = 0.33;
    e->timings.ratio[4] = 0.036;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 1.5;

            e->animation.dist = e->distance[x][y] +
                                 fl::sinf(0.5 * e->distance[x][y] - e->move.radial[3]);
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 0.1 * e->move.linear[0];
            e->animation.offset_y = -20 * e->move.linear[0];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->pixel.red = e->show1;
            e->pixel.green = 0;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Module_Experiment6(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;

    float w = 0.7;

    e->timings.ratio[0] = 0.0025;
    e->timings.ratio[1] = 0.0027;
    e->timings.ratio[2] = 0.029;
    e->timings.ratio[3] = 0.33 * w;
    e->timings.ratio[4] = 0.36 * w;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 0.8;

            e->animation.dist = e->distance[x][y] +
                                 fl::sinf(0.25 * e->distance[x][y] - e->move.radial[3]);
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 0.1 * e->move.linear[0];
            e->animation.offset_y = -20 * e->move.linear[0];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist = e->distance[x][y] +
                                 fl::sinf(0.24 * e->distance[x][y] - e->move.radial[4]);
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 10;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 0.1 * e->move.linear[1];
            e->animation.offset_y = -20 * e->move.linear[1];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->pixel.red = (e->show1 + e->show2);
            e->pixel.green = ((e->show1 + e->show2) * 0.6) - 30;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Module_Experiment7(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.005;

    float w = 0.3;

    e->timings.ratio[0] = 0.01;
    e->timings.ratio[1] = 0.011;
    e->timings.ratio[2] = 0.029;
    e->timings.ratio[3] = 0.33 * w;
    e->timings.ratio[4] = 0.36 * w;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 0.7;

            e->animation.dist =
                2 + e->distance[x][y] +
                2 * fl::sinf(0.25 * e->distance[x][y] - e->move.radial[3]);
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 10 * e->move.linear[0];
            e->animation.offset_y = -20 * e->move.linear[0];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist =
                2 + e->distance[x][y] +
                2 * fl::sinf(0.24 * e->distance[x][y] - e->move.radial[4]);
            e->animation.angle = e->polar_theta[x][y];
            e->animation.z = 10;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 0.1 * e->move.linear[1];
            e->animation.offset_y = -20 * e->move.linear[1];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->pixel.red = (e->show1 + e->show2);
            e->pixel.green = ((e->show1 + e->show2) * 0.6) - 50;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Module_Experiment8(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;

    float w = 0.3;

    e->timings.ratio[0] = 0.01;
    e->timings.ratio[1] = 0.011;
    e->timings.ratio[2] = 0.013;
    e->timings.ratio[3] = 0.33 * w;
    e->timings.ratio[4] = 0.36 * w;
    e->timings.ratio[5] = 0.38 * w;
    e->timings.ratio[6] = 0.0003;

    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;
    e->timings.offset[5] = 500;
    e->timings.offset[6] = 600;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 0.4;
            float r = 1.5;

            e->animation.dist =
                3 + e->distance[x][y] +
                3 * fl::sinf(0.25 * e->distance[x][y] - e->move.radial[3]);
            e->animation.angle = e->polar_theta[x][y] + e->move.noise_angle[0] +
                                 e->move.noise_angle[6];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 10 * e->move.linear[0];
            e->animation.offset_y = -5 * r * e->move.linear[0];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist =
                4 + e->distance[x][y] +
                4 * fl::sinf(0.24 * e->distance[x][y] - e->move.radial[4]);
            e->animation.angle = e->polar_theta[x][y] + e->move.noise_angle[1] +
                                 e->move.noise_angle[6];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 0.1 * e->move.linear[1];
            e->animation.offset_y = -5 * r * e->move.linear[1];
            e->animation.offset_x = 100;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist =
                5 + e->distance[x][y] +
                5 * fl::sinf(0.23 * e->distance[x][y] - e->move.radial[5]);
            e->animation.angle = e->polar_theta[x][y] + e->move.noise_angle[2] +
                                 e->move.noise_angle[6];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 0.1 * e->move.linear[2];
            e->animation.offset_y = -5 * r * e->move.linear[2];
            e->animation.offset_x = 1000;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->show4 = e->colordodge(e->show1, e->show2);

            float rad = fl::sinf(PI / 2 +
                             e->distance[x][y] / 14);

            e->pixel.red = rad * ((e->show1 + e->show2) + e->show3);
            e->pixel.green = (((e->show2 + e->show3) * 0.8) - 90) * rad;
            e->pixel.blue = e->show4 * 0.2;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Module_Experiment9(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.03;

    float w = 0.3;

    e->timings.ratio[0] = 0.1;
    e->timings.ratio[1] = 0.011;
    e->timings.ratio[2] = 0.013;
    e->timings.ratio[3] = 0.33 * w;
    e->timings.ratio[4] = 0.36 * w;
    e->timings.ratio[5] = 0.38 * w;
    e->timings.ratio[6] = 0.0003;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + e->move.radial[1];
            e->animation.z = 5;
            e->animation.scale_x = 0.001;
            e->animation.scale_y = 0.1;
            e->animation.scale_z = 0.1;
            e->animation.offset_y = -10 * e->move.linear[0];
            e->animation.offset_x = 20;
            e->animation.offset_z = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->pixel.red = 10 * e->show1;
            e->pixel.green = 0;
            e->pixel.blue = 0;

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}
inline void Module_Experiment10(Context &ctx) {
    auto *e = ctx.mEngine;
    e->get_ready();

    e->timings.master_speed = 0.01;

    float w = 1;

    e->timings.ratio[0] = 0.01;
    e->timings.ratio[1] = 0.011;
    e->timings.ratio[2] = 0.013;
    e->timings.ratio[3] = 0.33 * w;
    e->timings.ratio[4] = 0.36 * w;
    e->timings.ratio[5] = 0.38 * w;
    e->timings.ratio[6] = 0.0003;

    e->timings.offset[0] = 0;
    e->timings.offset[1] = 100;
    e->timings.offset[2] = 200;
    e->timings.offset[3] = 300;
    e->timings.offset[4] = 400;
    e->timings.offset[5] = 500;
    e->timings.offset[6] = 600;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            float s = 0.4;
            float r = 1.5;

            e->animation.dist =
                3 + e->distance[x][y] +
                3 * fl::sinf(0.25 * e->distance[x][y] - e->move.radial[3]);
            e->animation.angle = e->polar_theta[x][y] + e->move.noise_angle[0] +
                                 e->move.noise_angle[6];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 10 * e->move.linear[0];
            e->animation.offset_y = -5 * r * e->move.linear[0];
            e->animation.offset_x = 10;
            e->animation.low_limit = 0;
            e->show1 = e->render_value(e->animation);

            e->animation.dist =
                4 + e->distance[x][y] +
                4 * fl::sinf(0.24 * e->distance[x][y] - e->move.radial[4]);
            e->animation.angle = e->polar_theta[x][y] + e->move.noise_angle[1] +
                                 e->move.noise_angle[6];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 0.1 * e->move.linear[1];
            e->animation.offset_y = -5 * r * e->move.linear[1];
            e->animation.offset_x = 100;
            e->animation.low_limit = 0;
            e->show2 = e->render_value(e->animation);

            e->animation.dist =
                5 + e->distance[x][y] +
                5 * fl::sinf(0.23 * e->distance[x][y] - e->move.radial[5]);
            e->animation.angle = e->polar_theta[x][y] + e->move.noise_angle[2] +
                                 e->move.noise_angle[6];
            e->animation.z = 5;
            e->animation.scale_x = 0.1 * s;
            e->animation.scale_y = 0.1 * s;
            e->animation.offset_z = 0.1 * e->move.linear[2];
            e->animation.offset_y = -5 * r * e->move.linear[2];
            e->animation.offset_x = 1000;
            e->animation.low_limit = 0;
            e->show3 = e->render_value(e->animation);

            e->show4 = e->colordodge(e->show1, e->show2);

            float rad = fl::sinf(PI / 2 +
                             e->distance[x][y] / 14);

            CHSV(rad * ((e->show1 + e->show2) + e->show3), 255, 255);

            e->pixel = e->rgb_sanity_check(e->pixel);

            fl::u8 a = e->getTime() / 100;
            CRGB p = CRGB(CHSV(((a + e->show1 + e->show2) + e->show3), 255, 255));
            animartrix_detail::rgb pixel;
            pixel.red = p.red;
            pixel.green = p.green;
            pixel.blue = p.blue;
            e->setPixelColorInternal(x, y, pixel);
        }
    }
}
inline void Fluffy_Blobs(Context &ctx) {
    auto *e = ctx.mEngine;

    e->timings.master_speed = 0.015;
    float size = 0.15;
    float radial_speed = 1;
    float linear_speed = 5;

    e->timings.ratio[0] = 0.025;
    e->timings.ratio[1] = 0.026;
    e->timings.ratio[2] = 0.027;
    e->timings.ratio[3] = 0.028;
    e->timings.ratio[4] = 0.029;
    e->timings.ratio[5] = 0.030;
    e->timings.ratio[6] = 0.031;
    e->timings.ratio[7] = 0.032;
    e->timings.ratio[8] = 0.033;

    e->calculate_oscillators(e->timings);

    for (int x = 0; x < e->num_x; x++) {
        for (int y = 0; y < e->num_y; y++) {

            e->animation.dist = e->distance[x][y];
            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[0]);
            e->animation.z = 5;
            e->animation.scale_x = size;
            e->animation.scale_y = size;
            e->animation.offset_z = 0;
            e->animation.offset_x = 0;
            e->animation.offset_y = linear_speed * e->move.linear[0];
            e->animation.low_limit = 0;
            e->animation.high_limit = 1;
            e->show1 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[1]);
            e->animation.offset_y = linear_speed * e->move.linear[1];
            e->animation.offset_z = 200;
            e->animation.scale_x = size * 1.1;
            e->animation.scale_y = size * 1.1;
            e->show2 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[2]);
            e->animation.offset_y = linear_speed * e->move.linear[2];
            e->animation.offset_z = 400;
            e->animation.scale_x = size * 1.2;
            e->animation.scale_y = size * 1.2;
            e->show3 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[3]);
            e->animation.offset_y = linear_speed * e->move.linear[3];
            e->animation.offset_z = 600;
            e->animation.scale_x = size;
            e->animation.scale_y = size;
            e->show4 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[4]);
            e->animation.offset_y = linear_speed * e->move.linear[4];
            e->animation.offset_z = 800;
            e->animation.scale_x = size * 1.1;
            e->animation.scale_y = size * 1.1;
            e->show5 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[5]);
            e->animation.offset_y = linear_speed * e->move.linear[5];
            e->animation.offset_z = 1800;
            e->animation.scale_x = size * 1.2;
            e->animation.scale_y = size * 1.2;
            e->show6 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[6]);
            e->animation.offset_y = linear_speed * e->move.linear[6];
            e->animation.offset_z = 2800;
            e->animation.scale_x = size;
            e->animation.scale_y = size;
            e->show7 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[7]);
            e->animation.offset_y = linear_speed * e->move.linear[7];
            e->animation.offset_z = 3800;
            e->animation.scale_x = size * 1.1;
            e->animation.scale_y = size * 1.1;
            e->show8 = e->render_value(e->animation);

            e->animation.angle = e->polar_theta[x][y] + (radial_speed * e->move.radial[8]);
            e->animation.offset_y = linear_speed * e->move.linear[8];
            e->animation.offset_z = 4800;
            e->animation.scale_x = size * 1.2;
            e->animation.scale_y = size * 1.2;
            e->show9 = e->render_value(e->animation);

            e->pixel.red = 0.8 * (e->show1 + e->show2 + e->show3) + (e->show4 + e->show5 + e->show6);
            e->pixel.green = 0.8 * (e->show4 + e->show5 + e->show6);
            e->pixel.blue = 0.3 * (e->show7 + e->show8 + e->show9);

            e->pixel = e->rgb_sanity_check(e->pixel);

            e->setPixelColorInternal(x, y, e->pixel);
        }
    }
}


} // namespace animartrix2_detail

// Q31 optimized Chasing_Spirals extracted to its own file.
// Included after main namespace so all types are defined.
#define ANIMARTRIX2_CHASING_SPIRALS_INTERNAL
#include "fl/fx/2d/chasing_spirals.hpp" // allow-include-after-namespace

#if FL_ANIMARTRIX_USES_FAST_MATH
FL_OPTIMIZATION_LEVEL_O3_END
FL_FAST_MATH_END
#endif
