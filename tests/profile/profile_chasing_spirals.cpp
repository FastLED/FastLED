// standalone test
// Standalone profiling binary for Chasing Spirals: float (Animartrix) vs Q31 (Animartrix2)
// Build with profile mode (-Os -g) and run under valgrind --tool=callgrind
//
// Usage:
//   ./profile_chasing_spirals                   # Profile all variants
//   ./profile_chasing_spirals float             # Profile float path only
//   ./profile_chasing_spirals q31               # Profile Q31 original only
//   ./profile_chasing_spirals batch4            # Profile Batch4 only
//   ./profile_chasing_spirals batch4_color      # Profile Batch4 color-grouped only
//   ./profile_chasing_spirals batch8            # Profile Batch8 only
//   ./profile_chasing_spirals q16               # Profile Q16 variant only
//   ./profile_chasing_spirals i16               # Profile i16-optimized variant only

#include "FastLED.h"
#include "fl/fx/2d/animartrix.hpp"
#include "fl/fx/2d/animartrix2.hpp"
#include "fl/fx/2d/animartrix2_detail/chasing_spirals.hpp"
#include "fl/json.h"
#include "fl/stl/cstring.h"
#include "fl/stl/stdio.h"

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
void renderQ31_Direct(void (*func)(animartrix2_detail::Context&),
                      animartrix2_detail::Context &ctx,
                      int frames, int start_frame) {
    for (int i = 0; i < frames; i++) {
        uint32_t t = static_cast<uint32_t>((start_frame + i) * 16);
        animartrix2_detail::setTime(ctx, t);
        func(ctx);
    }
}

int main(int argc, char *argv[]) {
    bool do_float = true;
    bool do_q31 = true;
    bool do_batch4 = true;
    bool do_batch4_color = true;
    bool do_batch8 = true;
    bool do_q16 = true;
    bool do_i16 = true;
    bool json_output = false;  // Profile framework JSON output mode

    if (argc > 1) {
        // Check for "baseline" argument (profile framework mode)
        if (fl::strcmp(argv[1], "baseline") == 0) {
            json_output = true;
            // Run best performing variant (Q31 Batch8) for baseline measurements
            do_float = false;
            do_q31 = false;
            do_batch4 = false;
            do_batch4_color = false;
            do_batch8 = true;  // Best performance variant
            do_q16 = false;
            do_i16 = false;
        } else {
            // Disable all by default if specific variant requested
            do_float = false;
            do_q31 = false;
            do_batch4 = false;
            do_batch4_color = false;
            do_batch8 = false;
            do_q16 = false;
            do_i16 = false;

            if (fl::strcmp(argv[1], "float") == 0) {
                do_float = true;
            } else if (fl::strcmp(argv[1], "q31") == 0) {
                do_q31 = true;
            } else if (fl::strcmp(argv[1], "batch4") == 0) {
                do_batch4 = true;
            } else if (fl::strcmp(argv[1], "batch4_color") == 0) {
                do_batch4_color = true;
            } else if (fl::strcmp(argv[1], "batch8") == 0) {
                do_batch8 = true;
            } else if (fl::strcmp(argv[1], "q16") == 0) {
                do_q16 = true;
            } else if (fl::strcmp(argv[1], "i16") == 0) {
                do_i16 = true;
            } else {
                // Unknown argument - enable all
                do_float = true;
                do_q31 = true;
                do_batch4 = true;
                do_batch4_color = true;
                do_batch8 = true;
                do_q16 = true;
                do_i16 = true;
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
        printf("Float:              %d frames in %lu us (%.1f us/frame)\n",
               PROFILE_FRAMES, static_cast<unsigned long>(elapsed_us),
               static_cast<double>(elapsed_us) / PROFILE_FRAMES);
    }

    // ========================
    // Q31 (Animartrix2) path - Original
    // ========================
    if (do_q31) {
        XYMap xy = XYMap::constructRectangularGrid(W, H);

        // Create context
        animartrix2_detail::Context ctx;
        ctx.leds = leds;
        ctx.xyMapFn = [](u16 x, u16 y, void *userData) -> u16 {
            XYMap *map = static_cast<XYMap*>(userData);
            return map->mapToIndex(x, y);
        };
        ctx.xyMapUserData = &xy;

        animartrix2_detail::init(ctx, W, H);

        // Warmup (not profiled)
        renderQ31_Direct(&animartrix2_detail::q31::Chasing_Spirals_Q31, ctx, WARMUP_FRAMES, 0);

        // Profile
        u32 t0 = ::micros();
        renderQ31_Direct(&animartrix2_detail::q31::Chasing_Spirals_Q31, ctx, PROFILE_FRAMES, WARMUP_FRAMES);
        u32 t1 = ::micros();

        u32 elapsed_us = t1 - t0;
        printf("Q31 (original):     %d frames in %lu us (%.1f us/frame)\n",
               PROFILE_FRAMES, static_cast<unsigned long>(elapsed_us),
               static_cast<double>(elapsed_us) / PROFILE_FRAMES);
    }

    // ========================
    // Q31 Batch4 (pixel order)
    // ========================
    if (do_batch4) {
        XYMap xy = XYMap::constructRectangularGrid(W, H);

        // Create context
        animartrix2_detail::Context ctx;
        ctx.leds = leds;
        ctx.xyMapFn = [](u16 x, u16 y, void *userData) -> u16 {
            XYMap *map = static_cast<XYMap*>(userData);
            return map->mapToIndex(x, y);
        };
        ctx.xyMapUserData = &xy;

        animartrix2_detail::init(ctx, W, H);

        // Warmup (not profiled)
        renderQ31_Direct(&animartrix2_detail::q31::Chasing_Spirals_Q31_Batch4, ctx, WARMUP_FRAMES, 0);

        // Profile
        u32 t0 = ::micros();
        renderQ31_Direct(&animartrix2_detail::q31::Chasing_Spirals_Q31_Batch4, ctx, PROFILE_FRAMES, WARMUP_FRAMES);
        u32 t1 = ::micros();

        u32 elapsed_us = t1 - t0;
        printf("Q31 Batch4:         %d frames in %lu us (%.1f us/frame)\n",
               PROFILE_FRAMES, static_cast<unsigned long>(elapsed_us),
               static_cast<double>(elapsed_us) / PROFILE_FRAMES);
    }

    // ========================
    // Q31 Batch4 Color-Grouped
    // ========================
    if (do_batch4_color) {
        XYMap xy = XYMap::constructRectangularGrid(W, H);

        // Create context
        animartrix2_detail::Context ctx;
        ctx.leds = leds;
        ctx.xyMapFn = [](u16 x, u16 y, void *userData) -> u16 {
            XYMap *map = static_cast<XYMap*>(userData);
            return map->mapToIndex(x, y);
        };
        ctx.xyMapUserData = &xy;

        animartrix2_detail::init(ctx, W, H);

        // Warmup (not profiled)
        renderQ31_Direct(&animartrix2_detail::q31::Chasing_Spirals_Q31_Batch4_ColorGrouped, ctx, WARMUP_FRAMES, 0);

        // Profile
        u32 t0 = ::micros();
        renderQ31_Direct(&animartrix2_detail::q31::Chasing_Spirals_Q31_Batch4_ColorGrouped, ctx, PROFILE_FRAMES, WARMUP_FRAMES);
        u32 t1 = ::micros();

        u32 elapsed_us = t1 - t0;
        printf("Q31 Batch4 Color:   %d frames in %lu us (%.1f us/frame)\n",
               PROFILE_FRAMES, static_cast<unsigned long>(elapsed_us),
               static_cast<double>(elapsed_us) / PROFILE_FRAMES);
    }

    // ========================
    // Q31 Batch8
    // ========================
    if (do_batch8) {
        XYMap xy = XYMap::constructRectangularGrid(W, H);

        // Create context
        animartrix2_detail::Context ctx;
        ctx.leds = leds;
        ctx.xyMapFn = [](u16 x, u16 y, void *userData) -> u16 {
            XYMap *map = static_cast<XYMap*>(userData);
            return map->mapToIndex(x, y);
        };
        ctx.xyMapUserData = &xy;

        animartrix2_detail::init(ctx, W, H);

        // Warmup (not profiled)
        renderQ31_Direct(&animartrix2_detail::q31::Chasing_Spirals_Q31_Batch8, ctx, WARMUP_FRAMES, 0);

        // Profile
        u32 t0 = ::micros();
        renderQ31_Direct(&animartrix2_detail::q31::Chasing_Spirals_Q31_Batch8, ctx, PROFILE_FRAMES, WARMUP_FRAMES);
        u32 t1 = ::micros();

        u32 elapsed_us = t1 - t0;

        if (json_output) {
            // Profile framework JSON output using Json
            i64 elapsed_ns = static_cast<i64>(elapsed_us) * 1000LL;
            double ns_per_frame = static_cast<double>(elapsed_ns) / PROFILE_FRAMES;
            double frames_per_sec = 1e9 / ns_per_frame;

            Json result = Json::object();
            result.set("variant", "q31_batch8");
            result.set("target", "chasing_spirals");
            result.set("total_calls", PROFILE_FRAMES);
            result.set("total_time_ns", elapsed_ns);  // Use i64 directly
            result.set("ns_per_call", ns_per_frame);
            result.set("calls_per_sec", frames_per_sec);

            // Output with marker for profile framework
            printf("PROFILE_RESULT:%s\n", result.to_string().c_str());
        } else {
            // Human-readable output
            printf("Q31 Batch8:         %d frames in %lu us (%.1f us/frame)\n",
                   PROFILE_FRAMES, static_cast<unsigned long>(elapsed_us),
                   static_cast<double>(elapsed_us) / PROFILE_FRAMES);
        }
    }

    // ========================
    // Q16 Batch4 Color-Grouped
    // ========================
    if (do_q16) {
        XYMap xy = XYMap::constructRectangularGrid(W, H);

        // Create context
        animartrix2_detail::Context ctx;
        ctx.leds = leds;
        ctx.xyMapFn = [](u16 x, u16 y, void *userData) -> u16 {
            XYMap *map = static_cast<XYMap*>(userData);
            return map->mapToIndex(x, y);
        };
        ctx.xyMapUserData = &xy;

        animartrix2_detail::init(ctx, W, H);

        // Warmup (not profiled)
        renderQ31_Direct(&animartrix2_detail::q16::Chasing_Spirals_Q16_Batch4_ColorGrouped, ctx, WARMUP_FRAMES, 0);

        // Profile
        u32 t0 = ::micros();
        renderQ31_Direct(&animartrix2_detail::q16::Chasing_Spirals_Q16_Batch4_ColorGrouped, ctx, PROFILE_FRAMES, WARMUP_FRAMES);
        u32 t1 = ::micros();

        u32 elapsed_us = t1 - t0;
        printf("Q16 Batch4 Color:   %d frames in %lu us (%.1f us/frame)\n",
               PROFILE_FRAMES, static_cast<unsigned long>(elapsed_us),
               static_cast<double>(elapsed_us) / PROFILE_FRAMES);
    }

    // ========================
    // i16-optimized Batch4 Color-Grouped
    // ========================
    if (do_i16) {
        XYMap xy = XYMap::constructRectangularGrid(W, H);

        // Create context
        animartrix2_detail::Context ctx;
        ctx.leds = leds;
        ctx.xyMapFn = [](u16 x, u16 y, void *userData) -> u16 {
            XYMap *map = static_cast<XYMap*>(userData);
            return map->mapToIndex(x, y);
        };
        ctx.xyMapUserData = &xy;

        animartrix2_detail::init(ctx, W, H);

        // Warmup (not profiled)
        renderQ31_Direct(&animartrix2_detail::i16_opt::Chasing_Spirals_i16_Batch4_ColorGrouped, ctx, WARMUP_FRAMES, 0);

        // Profile
        u32 t0 = ::micros();
        renderQ31_Direct(&animartrix2_detail::i16_opt::Chasing_Spirals_i16_Batch4_ColorGrouped, ctx, PROFILE_FRAMES, WARMUP_FRAMES);
        u32 t1 = ::micros();

        u32 elapsed_us = t1 - t0;
        printf("i16 Batch4 Color:   %d frames in %lu us (%.1f us/frame)\n",
               PROFILE_FRAMES, static_cast<unsigned long>(elapsed_us),
               static_cast<double>(elapsed_us) / PROFILE_FRAMES);
    }

    return 0;
}
