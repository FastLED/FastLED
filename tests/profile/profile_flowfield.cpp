// ok standalone
// Standalone profiling binary for FlowField: Float vs FP (fixed-point)
//
// Usage:
//   ./profile_flowfield                    # Profile both variants
//   ./profile_flowfield float              # Profile float only
//   ./profile_flowfield fp                 # Profile FP only
//   ./profile_flowfield baseline           # JSON output mode (for profiling pipeline)

#include "fl/stl/compiler_control.h"  // FL_NO_INLINE
#include "FastLED.h"
#include "fl/fx/2d/flowfield.h"
#include "fl/stl/cstring.h"
#include "fl/stl/stdio.h"
#include "tests/profile/profile_result.h"

using namespace fl;

static const uint16_t W = 22;
static const uint16_t H = 22;
static const uint16_t N = W * H;

static const int WARMUP_FRAMES = 20;
static const int PROFILE_FRAMES = 200;

FL_NO_INLINE
void renderFloat(FlowFieldFloat &fx, CRGB *leds, int frames, int start_frame) {
    for (int i = 0; i < frames; i++) {
        uint32_t t = static_cast<uint32_t>((start_frame + i) * 33);
        Fx::DrawContext ctx(t, leds);
        fx.draw(ctx);
    }
}

FL_NO_INLINE
void renderFP(FlowFieldFP &fx, CRGB *leds, int frames, int start_frame) {
    for (int i = 0; i < frames; i++) {
        uint32_t t = static_cast<uint32_t>((start_frame + i) * 33);
        Fx::DrawContext ctx(t, leds);
        fx.draw(ctx);
    }
}

int main(int argc, char *argv[]) {
    bool do_float = true;
    bool do_fp = true;
    bool json_output = false;

    if (argc > 1) {
        if (fl::strcmp(argv[1], "baseline") == 0) {
            json_output = true;
        } else {
            do_float = false;
            do_fp = false;
            if (fl::strcmp(argv[1], "float") == 0) {
                do_float = true;
            } else if (fl::strcmp(argv[1], "fp") == 0) {
                do_fp = true;
            } else {
                do_float = true;
                do_fp = true;
            }
        }
    }

    CRGB leds[N] = {};
    XYMap xy = XYMap::constructRectangularGrid(W, H);

    u32 float_us = 0;
    u32 fp_us = 0;

    // ========================
    // Float FlowField
    // ========================
    if (do_float || json_output) {
        FlowFieldFloat fx(xy);

        renderFloat(fx, leds, WARMUP_FRAMES, 0);

        u32 t0 = ::micros();
        renderFloat(fx, leds, PROFILE_FRAMES, WARMUP_FRAMES);
        u32 t1 = ::micros();

        float_us = t1 - t0;

        if (json_output) {
            ProfileResultBuilder::print_result("float", "flowfield",
                                               PROFILE_FRAMES, float_us);
        } else {
            printf("Float FlowField:  %d frames in %u us (%.1f us/frame)\n",
                   PROFILE_FRAMES, float_us,
                   static_cast<double>(float_us) / PROFILE_FRAMES);
        }
    }

    // ========================
    // FP FlowFieldFP
    // ========================
    if (do_fp || json_output) {
        FlowFieldFP fx(xy);

        renderFP(fx, leds, WARMUP_FRAMES, 0);

        u32 t0 = ::micros();
        renderFP(fx, leds, PROFILE_FRAMES, WARMUP_FRAMES);
        u32 t1 = ::micros();

        fp_us = t1 - t0;

        if (json_output) {
            ProfileResultBuilder::print_result("fp", "flowfield",
                                               PROFILE_FRAMES, fp_us);
        } else {
            printf("FP FlowFieldFP: %d frames in %u us (%.1f us/frame)\n",
                   PROFILE_FRAMES, fp_us,
                   static_cast<double>(fp_us) / PROFILE_FRAMES);
        }
    }

    // Print speedup ratio
    if (!json_output && float_us > 0 && fp_us > 0) {
        double speedup = static_cast<double>(float_us) / fp_us;
        printf("\nSpeedup: %.2fx (%s)\n", speedup,
               speedup >= 2.0 ? "PASS >= 2.0x" : "BELOW 2.0x target");
    }

    return 0;
}
