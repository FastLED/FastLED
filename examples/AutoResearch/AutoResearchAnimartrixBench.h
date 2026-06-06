// AutoResearchAnimartrixBench.h
//
// Animartrix-representative Perlin-noise benchmark: scalar float (`fl::pnoise`)
// vs s16x16 fixed-point (`fl::perlin_i16_optimized::pnoise2d`).
//
// Why Perlin noise: it's the workhorse of every Animartrix effect — every
// frame, every pixel goes through one or more `pnoise` calls to drive the
// polar-coordinate field that shapes the pattern. If a fixed-point variant
// wins here, it wins in Animartrix.
//
// Why s16x16 (and not s8x8): Animartrix's fade/lerp/grad inner loops need
// the precision of Q16.16 to match the float reference. The s8x8 Q8.8
// type loses too many bits in the multiplications inside the gradient
// step. This benchmark mirrors what the FastLED tree already ships
// (`fl/fx/2d/animartrix_detail/perlin_i16_optimized.cpp.hpp`).

#pragma once

#include <FastLED.h>
#include "fl/fx/2d/animartrix_detail/perlin_float.h"
#include "fl/fx/2d/animartrix_detail/perlin_i16_optimized.h"
#include "fl/math/fixed_point/s16x16.h"

namespace autoresearch {
namespace animartrix_check {

// Volatile sink to defeat dead-code elimination on both the float and
// the i16 outputs. Same trick the SIMD multiply benchmark uses.
static volatile int32_t g_animartrix_bench_sink;

struct PerlinBenchResult {
    int64_t iterations;
    int64_t pnoise_float_us;   // fl::pnoise(x, y, 0) total wall time
    int64_t pnoise_i16_us;     // fl::perlin_i16_optimized::pnoise2d total wall time
    // The same coordinate grid is fed to both versions so the comparison
    // measures purely the arithmetic cost of float vs i16 fixed-point.
};

// Sweep a 2D coordinate grid the same way an Animartrix render pass
// would (one Perlin lookup per output pixel). 16*16 = 256 pixels per
// outer iter, matching a 16x16 panel — small enough that the compiler
// won't unroll the world, large enough that any per-iteration fixed
// overhead is amortized.
inline PerlinBenchResult runPerlinBenchmark(int iters) {
    PerlinBenchResult r;
    r.iterations = iters;

    // Init the i16 implementation's fade lookup table once, off the
    // benchmark clock. Function-local static so the linter's
    // static-in-header rule stays happy and C++11's thread-safe init
    // guarantees a single initialization across calls. The float path
    // has no equivalent setup.
    struct FadeLut {
        int32_t table[257];
        FadeLut() { fl::perlin_i16_optimized::init_fade_lut(table); }
    };
    static const FadeLut fade_lut_holder;  // C++11 magic statics
    const int32_t* fade_lut = fade_lut_holder.table;

    constexpr int GRID = 16;          // 16x16 pixel pass per iteration
    constexpr float STEP_F = 0.05f;   // Animartrix-typical pixel step in noise space
    // Mirror in fixed-point: 0.05 in Q16.16 = 0.05 * 65536 ≈ 3277
    constexpr int32_t STEP_I = static_cast<int32_t>(0.05f * 65536.0f);

    // ── Float pnoise ──────────────────────────────────────────────
    {
        float ax = 0.0f, ay = 0.0f;
        int32_t sink = 0;
        uint32_t t0 = micros();
        for (int it = 0; it < iters; it++) {
            // Slowly drift the origin across the iter so the compiler
            // can't pre-compute. Same pattern Animartrix uses (the
            // origin advances with `time_speed * dt` each frame).
            ax += 0.011f;
            ay += 0.013f;
            for (int row = 0; row < GRID; row++) {
                float y = ay + row * STEP_F;
                for (int col = 0; col < GRID; col++) {
                    float x = ax + col * STEP_F;
                    float n = fl::pnoise(x, y, 0.0f);
                    // Map [-1, 1] → int8 the way Animartrix's output
                    // stage does. Sink to defeat DCE.
                    sink += static_cast<int32_t>(n * 127.0f);
                }
            }
        }
        uint32_t t1 = micros();
        g_animartrix_bench_sink = sink;
        r.pnoise_float_us = static_cast<int64_t>(t1 - t0);
    }

    // ── i16 fixed-point pnoise2d ──────────────────────────────────
    {
        int32_t ax_i = 0, ay_i = 0;
        // Q16.16 increment for the slow drift (matches 0.011, 0.013 in float)
        constexpr int32_t DRIFT_X = static_cast<int32_t>(0.011f * 65536.0f);
        constexpr int32_t DRIFT_Y = static_cast<int32_t>(0.013f * 65536.0f);
        int32_t sink = 0;
        uint32_t t0 = micros();
        for (int it = 0; it < iters; it++) {
            ax_i += DRIFT_X;
            ay_i += DRIFT_Y;
            for (int row = 0; row < GRID; row++) {
                int32_t y_i = ay_i + row * STEP_I;
                for (int col = 0; col < GRID; col++) {
                    int32_t x_i = ax_i + col * STEP_I;
                    // pnoise2d_raw returns i32 Q16.16. Scale to int8
                    // the same way the float path does — divide by
                    // (HP_ONE / 127) so the magnitudes line up.
                    int32_t n = fl::perlin_i16_optimized::pnoise2d_raw(
                        x_i, y_i, fade_lut, fl::PERLIN_NOISE);
                    sink += n >> 9;   // crude scale; the actual op cost,
                                       // not the value, is what we measure
                }
            }
        }
        uint32_t t1 = micros();
        g_animartrix_bench_sink = sink;
        r.pnoise_i16_us = static_cast<int64_t>(t1 - t0);
    }

    return r;
}

}  // namespace animartrix_check
}  // namespace autoresearch
