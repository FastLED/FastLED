// ok standalone
// Standalone profiling binary for Chasing Spirals: Float vs Q31 (scalar) vs Q31_SIMD
// Build with profile mode (-Os -g) and run under valgrind --tool=callgrind
//
// Usage:
//   ./profile_chasing_spirals                   # Profile all 3 variants
//   ./profile_chasing_spirals float             # Profile float (Animartrix) only
//   ./profile_chasing_spirals q31               # Profile Q31 (scalar fixed-point) only
//   ./profile_chasing_spirals simd              # Profile Q31_SIMD (vectorized) only

#include "FastLED.h"
#include "fl/fx/2d/animartrix.hpp"
#include "fl/fx/2d/animartrix2.hpp"
#include "fl/fx/2d/animartrix2_detail/chasing_spirals.h"
#include "fl/stl/cstring.h"
#include "fl/stl/stdio.h"
#include "tests/profile/profile_result.h"

using namespace fl;

static const uint16_t W = 32;
static const uint16_t H = 32;
static const uint16_t N = W * H;

static const int WARMUP_FRAMES = 20;
static const int PROFILE_FRAMES = 200;

// These functions are named so callgrind --toggle-collect can target them.

__attribute__((noinline))
void renderFloat(Animartrix &fx, CRGB *leds, int frames, int start_frame) {
    for (int i = 0; i < frames; i++) {
        uint32_t t = static_cast<uint32_t>((start_frame + i) * 16);
        Fx::DrawContext ctx(t, leds);
        fx.draw(ctx);
    }
}

__attribute__((noinline))
void renderQ31(Animartrix2 &fx, CRGB *leds, int frames, int start_frame) {
    for (int i = 0; i < frames; i++) {
        uint32_t t = static_cast<uint32_t>((start_frame + i) * 16);
        Fx::DrawContext ctx(t, leds);
        fx.draw(ctx);
    }
}

// Direct function renderers for benchmarking specific implementations
__attribute__((noinline))
void renderQ31_Direct(void (*func)(Context&),
                      Context &ctx,
                      int frames, int start_frame) {
    for (int i = 0; i < frames; i++) {
        uint32_t t = static_cast<uint32_t>((start_frame + i) * 16);
        setTime(ctx, t);
        func(ctx);
    }
}

int main(int argc, char *argv[]) {
    bool do_float = true;
    bool do_q31 = true;
    bool do_q31_simd = true;
    bool json_output = false;  // Profile framework JSON output mode

    if (argc > 1) {
        // Check for "baseline" argument (profile framework mode)
        if (fl::strcmp(argv[1], "baseline") == 0) {
            json_output = true;
            // Run best performing variant (Q31_SIMD) for baseline measurements
            do_float = false;
            do_q31 = false;
            do_q31_simd = true;  // Best performance variant
        } else {
            // Disable all by default if specific variant requested
            do_float = false;
            do_q31 = false;
            do_q31_simd = false;

            if (fl::strcmp(argv[1], "float") == 0) {
                do_float = true;
            } else if (fl::strcmp(argv[1], "q31") == 0) {
                do_q31 = true;
            } else if (fl::strcmp(argv[1], "simd") == 0 || fl::strcmp(argv[1], "q31_simd") == 0) {
                do_q31_simd = true;
            } else {
                // Unknown argument - enable all
                do_float = true;
                do_q31 = true;
                do_q31_simd = true;
            }
        }
    }

    CRGB leds[N] = {};

    // ========================
    // Float (Animartrix) path
    // ========================
    if (do_float) {
        XYMap xy = XYMap::constructRectangularGrid(W, H);
        Animartrix fx(xy, CHASING_SPIRALS);

        // Warmup (not profiled)
        renderFloat(fx, leds, WARMUP_FRAMES, 0);

        // Profile
        u32 t0 = ::micros();
        renderFloat(fx, leds, PROFILE_FRAMES, WARMUP_FRAMES);
        u32 t1 = ::micros();

        u32 elapsed_us = t1 - t0;
        printf("Float:              %d frames in %u us (%.1f us/frame)\n",
               PROFILE_FRAMES, elapsed_us,
               static_cast<double>(elapsed_us) / PROFILE_FRAMES);
    }

    // ========================
    // Q31 (Animartrix2) path - Original
    // ========================
    if (do_q31) {
        XYMap xy = XYMap::constructRectangularGrid(W, H);

        // Create context
        Context ctx;
        ctx.leds = leds;
        ctx.xyMapFn = [](u16 x, u16 y, void *userData) -> u16 {
            XYMap *map = static_cast<XYMap*>(userData);
            return map->mapToIndex(x, y);
        };
        ctx.xyMapUserData = &xy;

        init(ctx, W, H);

        // Warmup (not profiled)
        renderQ31_Direct(&Chasing_Spirals_Q31, ctx, WARMUP_FRAMES, 0);

        // Profile
        u32 t0 = ::micros();
        renderQ31_Direct(&Chasing_Spirals_Q31, ctx, PROFILE_FRAMES, WARMUP_FRAMES);
        u32 t1 = ::micros();

        u32 elapsed_us = t1 - t0;
        printf("Q31 (original):     %d frames in %lu us (%.1f us/frame)\n",
               PROFILE_FRAMES, static_cast<unsigned long>(elapsed_us),
               static_cast<double>(elapsed_us) / PROFILE_FRAMES);
    }

    // ========================
    // Q31 SIMD (vectorized sincos)
    // ========================
    if (do_q31_simd) {
        XYMap xy = XYMap::constructRectangularGrid(W, H);

        // Create context
        Context ctx;
        ctx.leds = leds;
        ctx.xyMapFn = [](u16 x, u16 y, void *userData) -> u16 {
            XYMap *map = static_cast<XYMap*>(userData);
            return map->mapToIndex(x, y);
        };
        ctx.xyMapUserData = &xy;

        init(ctx, W, H);

        // Warmup (not profiled)
        renderQ31_Direct(&Chasing_Spirals_Q31_SIMD, ctx, WARMUP_FRAMES, 0);

        // Profile
        u32 t0 = ::micros();
        renderQ31_Direct(&Chasing_Spirals_Q31_SIMD, ctx, PROFILE_FRAMES, WARMUP_FRAMES);
        u32 t1 = ::micros();

        u32 elapsed_us = t1 - t0;

        if (json_output) {
            ProfileResultBuilder::print_result("q31_simd", "chasing_spirals",
                                              PROFILE_FRAMES, elapsed_us);
        } else {
            printf("Q31 SIMD:           %d frames in %lu us (%.1f us/frame)\n",
                   PROFILE_FRAMES, static_cast<unsigned long>(elapsed_us),
                   static_cast<double>(elapsed_us) / PROFILE_FRAMES);
        }
    }

    return 0;
}
