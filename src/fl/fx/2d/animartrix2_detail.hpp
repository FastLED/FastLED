#pragma once
// IWYU pragma: private, include "fl/fx/2d/animartrix2.hpp"
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
//
// NOTE: This is an internal implementation header. Do not include directly.
//       Include "fl/fx/2d/animartrix2.hpp" instead.

// Include standalone Animartrix2 core functionality
#include "fl/fx/2d/animartrix2_detail/core_types.hpp"
#include "fl/fx/2d/animartrix2_detail/perlin_float.hpp"
#include "fl/fx/2d/animartrix2_detail/engine_core.hpp"

#include "crgb.h"
#include "fl/stl/vector.h"
#include "fl/stl/math.h"
#include "fl/stl/optional.h"
#include "fl/stl/stdint.h"
#include "fl/sin32.h"
#include "fl/fixed_point/s16x16.h"
#include "fl/simd.h"


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

// Helper structs extracted to separate headers for better organization
#include "fl/fx/2d/animartrix2_detail/context.h"
#include "fl/fx/2d/animartrix2_detail/chasing_spiral_pixel_lut.h"
#include "fl/fx/2d/animartrix2_detail/perlin_s16x16.h"
#include "fl/fx/2d/animartrix2_detail/perlin_s16x16_simd.h"
#include "fl/fx/2d/animartrix2_detail/perlin_q16.h"
#include "fl/fx/2d/animartrix2_detail/perlin_s8x8.h"
#include "fl/fx/2d/animartrix2_detail/perlin_i16_optimized.h"

// Visualizer declarations (implementations in viz/*.cpp.hpp)
#include "fl/fx/2d/animartrix2_detail/viz/big_caleido.h"
#include "fl/fx/2d/animartrix2_detail/viz/caleido1.h"
#include "fl/fx/2d/animartrix2_detail/viz/caleido2.h"
#include "fl/fx/2d/animartrix2_detail/viz/caleido3.h"
#include "fl/fx/2d/animartrix2_detail/viz/center_field.h"
#include "fl/fx/2d/animartrix2_detail/viz/chasing_spirals.h"
#include "fl/fx/2d/animartrix2_detail/viz/complex_kaleido.h"
#include "fl/fx/2d/animartrix2_detail/viz/complex_kaleido_2.h"
#include "fl/fx/2d/animartrix2_detail/viz/complex_kaleido_3.h"
#include "fl/fx/2d/animartrix2_detail/viz/complex_kaleido_4.h"
#include "fl/fx/2d/animartrix2_detail/viz/complex_kaleido_5.h"
#include "fl/fx/2d/animartrix2_detail/viz/complex_kaleido_6.h"
#include "fl/fx/2d/animartrix2_detail/viz/distance_experiment.h"
#include "fl/fx/2d/animartrix2_detail/viz/fluffy_blobs.h"
#include "fl/fx/2d/animartrix2_detail/viz/hot_blob.h"
#include "fl/fx/2d/animartrix2_detail/viz/lava1.h"
#include "fl/fx/2d/animartrix2_detail/viz/module_experiment1.h"
#include "fl/fx/2d/animartrix2_detail/viz/module_experiment10.h"
#include "fl/fx/2d/animartrix2_detail/viz/module_experiment2.h"
#include "fl/fx/2d/animartrix2_detail/viz/module_experiment3.h"
#include "fl/fx/2d/animartrix2_detail/viz/module_experiment4.h"
#include "fl/fx/2d/animartrix2_detail/viz/module_experiment5.h"
#include "fl/fx/2d/animartrix2_detail/viz/module_experiment6.h"
#include "fl/fx/2d/animartrix2_detail/viz/module_experiment7.h"
#include "fl/fx/2d/animartrix2_detail/viz/module_experiment8.h"
#include "fl/fx/2d/animartrix2_detail/viz/module_experiment9.h"
#include "fl/fx/2d/animartrix2_detail/viz/parametric_water.h"
#include "fl/fx/2d/animartrix2_detail/viz/polar_waves.h"
#include "fl/fx/2d/animartrix2_detail/viz/rgb_blobs.h"
#include "fl/fx/2d/animartrix2_detail/viz/rgb_blobs2.h"
#include "fl/fx/2d/animartrix2_detail/viz/rgb_blobs3.h"
#include "fl/fx/2d/animartrix2_detail/viz/rgb_blobs4.h"
#include "fl/fx/2d/animartrix2_detail/viz/rgb_blobs5.h"
#include "fl/fx/2d/animartrix2_detail/viz/rings.h"
#include "fl/fx/2d/animartrix2_detail/viz/rotating_blob.h"
#include "fl/fx/2d/animartrix2_detail/viz/scaledemo1.h"
#include "fl/fx/2d/animartrix2_detail/viz/slow_fade.h"
#include "fl/fx/2d/animartrix2_detail/viz/spiral_matrix1.h"
#include "fl/fx/2d/animartrix2_detail/viz/spiral_matrix10.h"
#include "fl/fx/2d/animartrix2_detail/viz/spiral_matrix2.h"
#include "fl/fx/2d/animartrix2_detail/viz/spiral_matrix3.h"
#include "fl/fx/2d/animartrix2_detail/viz/spiral_matrix4.h"
#include "fl/fx/2d/animartrix2_detail/viz/spiral_matrix5.h"
#include "fl/fx/2d/animartrix2_detail/viz/spiral_matrix6.h"
#include "fl/fx/2d/animartrix2_detail/viz/spiral_matrix8.h"
#include "fl/fx/2d/animartrix2_detail/viz/spiral_matrix9.h"
#include "fl/fx/2d/animartrix2_detail/viz/spiralus.h"
#include "fl/fx/2d/animartrix2_detail/viz/spiralus2.h"
#include "fl/fx/2d/animartrix2_detail/viz/water.h"
#include "fl/fx/2d/animartrix2_detail/viz/waves.h"
#include "fl/fx/2d/animartrix2_detail/viz/yves.h"
#include "fl/fx/2d/animartrix2_detail/viz/zoom.h"
#include "fl/fx/2d/animartrix2_detail/viz/zoom2.h"

namespace fl {

// Engine: Standalone implementation (no inheritance from ANIMartRIX)
struct Context::Engine {
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

// Namespace wrappers for backwards compatibility with test code
namespace q31 {
    using fl::Chasing_Spirals_Q31;
    using fl::Chasing_Spirals_Q31_SIMD;
}

namespace q16 {
    // Q16 implementation aliased to Q31 (Q16 was removed, use Q31 instead)
    inline void Chasing_Spirals_Q16_Batch4_ColorGrouped(Context &ctx) {
        fl::Chasing_Spirals_Q31(ctx);
    }
}

}  // namespace fl

#if FL_ANIMARTRIX_USES_FAST_MATH
FL_OPTIMIZATION_LEVEL_O3_END
FL_FAST_MATH_END
#endif
