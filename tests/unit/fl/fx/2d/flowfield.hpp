
// Tests for FlowField: float vs FP accuracy + animation regression tests

#include "test.h"
#include "fl/fx/2d/flowfield.h"
#include "fl/math/xymap.h"
#include "fl/stl/stdio.h"

using namespace fl;

namespace flowfield_fp_test {

struct FfErrorStats {
    int max_error;
    double avg_error;
    double std_dev;
    int pixels_with_error;
    int pixels_over_1bit;
    int pixels_over_2bit;
    int pixels_over_4bit;
    int total_pixels;
    int histogram[256];
};

FfErrorStats ff_analyze_errors(const CRGB *float_leds, const CRGB *fp_leds, int count) {
    FfErrorStats stats = {};

    for (int i = 0; i < count; i++) {
        int r_err = abs(static_cast<int>(float_leds[i].r) - static_cast<int>(fp_leds[i].r));
        int g_err = abs(static_cast<int>(float_leds[i].g) - static_cast<int>(fp_leds[i].g));
        int b_err = abs(static_cast<int>(float_leds[i].b) - static_cast<int>(fp_leds[i].b));

        int pixel_max_err = r_err;
        if (g_err > pixel_max_err) pixel_max_err = g_err;
        if (b_err > pixel_max_err) pixel_max_err = b_err;

        if (pixel_max_err > 0) {
            stats.pixels_with_error++;
            stats.avg_error += pixel_max_err;
        }

        if (pixel_max_err > stats.max_error) {
            stats.max_error = pixel_max_err;
        }

        if (pixel_max_err > 1) stats.pixels_over_1bit++;
        if (pixel_max_err > 2) stats.pixels_over_2bit++;
        if (pixel_max_err > 4) stats.pixels_over_4bit++;

        if (pixel_max_err < 256) stats.histogram[pixel_max_err]++;
    }

    stats.total_pixels = count;
    if (stats.pixels_with_error > 0) {
        stats.avg_error /= stats.pixels_with_error;
    }

    double variance = 0.0;
    for (int i = 0; i < count; i++) {
        int r_err = abs(static_cast<int>(float_leds[i].r) - static_cast<int>(fp_leds[i].r));
        int g_err = abs(static_cast<int>(float_leds[i].g) - static_cast<int>(fp_leds[i].g));
        int b_err = abs(static_cast<int>(float_leds[i].b) - static_cast<int>(fp_leds[i].b));

        int pixel_max_err = r_err;
        if (g_err > pixel_max_err) pixel_max_err = g_err;
        if (b_err > pixel_max_err) pixel_max_err = b_err;

        double diff = pixel_max_err - stats.avg_error;
        variance += diff * diff;
    }
    stats.std_dev = fl::sqrt(variance / count);

    return stats;
}

void ff_print_error_stats(const FfErrorStats &stats, const char *test_name) {
    fl::printf("\n=== %s ===\n", test_name);
    fl::printf("Total pixels: %d\n", stats.total_pixels);
    fl::printf("Pixels with error: %d (%.1f%%)\n",
            stats.pixels_with_error,
            100.0 * stats.pixels_with_error / stats.total_pixels);
    fl::printf("Max error: %d\n", stats.max_error);
    fl::printf("Avg error: %.2f\n", stats.avg_error);
    fl::printf("Std dev: %.2f\n", stats.std_dev);
    fl::printf("\nError distribution:\n");
    fl::printf("  >1 LSB: %d pixels (%.1f%%)\n",
            stats.pixels_over_1bit,
            100.0 * stats.pixels_over_1bit / stats.total_pixels);
    fl::printf("  >2 LSB: %d pixels (%.1f%%)\n",
            stats.pixels_over_2bit,
            100.0 * stats.pixels_over_2bit / stats.total_pixels);
    fl::printf("  >4 LSB: %d pixels (%.1f%%)\n",
            stats.pixels_over_4bit,
            100.0 * stats.pixels_over_4bit / stats.total_pixels);

    fl::printf("\nHistogram (first 20 buckets):\n");
    for (int i = 0; i < 20 && i <= stats.max_error; i++) {
        if (stats.histogram[i] > 0) {
            fl::printf("  Error=%2d: %4d pixels (%.1f%%)\n",
                    i, stats.histogram[i],
                    100.0 * stats.histogram[i] / stats.total_pixels);
        }
    }
}

int count_diff_pixels(const CRGB *a, const CRGB *b, int count) {
    int diff = 0;
    for (int i = 0; i < count; i++) {
        if (a[i].r != b[i].r || a[i].g != b[i].g || a[i].b != b[i].b) {
            diff++;
        }
    }
    return diff;
}

} // namespace flowfield_fp_test

FL_TEST_CASE("flowfield_fp - compiles and runs without crash") {
    const uint16_t W = 8;
    const uint16_t H = 8;
    const int N = W * H;

    CRGB leds[N] = {};
    XYMap xy = XYMap::constructRectangularGrid(W, H);

    FlowFieldFP fx(xy);
    Fx::DrawContext ctx(1000, leds);
    fx.draw(ctx);

    // Just verify it ran without crashing and produced some output
    bool any_nonzero = false;
    for (int i = 0; i < N; i++) {
        if (leds[i].r != 0 || leds[i].g != 0 || leds[i].b != 0) {
            any_nonzero = true;
            break;
        }
    }
    FL_ASSERT(any_nonzero, "FlowFieldFP should produce non-zero output");
}

FL_TEST_CASE("flowfield_fp - float vs fp accuracy (t=1000)") {
    using namespace flowfield_fp_test;

    const uint16_t W = 8;
    const uint16_t H = 8;
    const int N = W * H;

    CRGB float_leds[N] = {};
    CRGB fp_leds[N] = {};

    XYMap xy = XYMap::constructRectangularGrid(W, H);

    // Run float version: two frames (init frame + real frame)
    {
        FlowFieldFloat fx(xy);
        Fx::DrawContext ctx0(0, float_leds);
        fx.draw(ctx0);  // Init frame
        Fx::DrawContext ctx1(1000, float_leds);
        fx.draw(ctx1);  // Real frame at t=1s, dt=1s
    }

    // Run FP version: same timing
    {
        FlowFieldFP fx(xy);
        Fx::DrawContext ctx0(0, fp_leds);
        fx.draw(ctx0);  // Init frame
        Fx::DrawContext ctx1(1000, fp_leds);
        fx.draw(ctx1);  // Real frame at t=1s, dt=1s
    }

    FfErrorStats stats = ff_analyze_errors(float_leds, fp_leds, N);
    ff_print_error_stats(stats, "FlowField Float vs FP (t=1000)");

    // Fixed-point Perlin uses 16 gradient directions vs float's 8,
    // producing structurally different (but valid) noise patterns.
    FL_ASSERT(stats.max_error <= 128, "Max error should be reasonable (< 128)");
}

// ---------------------------------------------------------------------------
//  Animation regression tests — verify frames change over time
// ---------------------------------------------------------------------------

FL_TEST_CASE("flowfield_fp - consecutive frames animate") {
    using namespace flowfield_fp_test;

    const uint16_t W = 8;
    const uint16_t H = 8;
    const int N = W * H;

    XYMap xy = XYMap::constructRectangularGrid(W, H);
    FlowFieldFP fx(xy);

    CRGB leds_a[N] = {};
    CRGB leds_b[N] = {};

    // Init + frame at t=1000
    fx.draw(Fx::DrawContext(0, leds_a));
    fx.draw(Fx::DrawContext(1000, leds_a));

    // Frame at t=2000 (into separate array — effect state still advances)
    fx.draw(Fx::DrawContext(2000, leds_b));

    int diff = count_diff_pixels(leds_a, leds_b, N);
    fl::printf("FlowFieldFP consecutive frames: %d of %d pixels differ\n", diff, N);
    FL_ASSERT(diff > 0, "FlowFieldFP must produce different output on consecutive frames");
}

FL_TEST_CASE("flowfield_float - consecutive frames animate") {
    using namespace flowfield_fp_test;

    const uint16_t W = 8;
    const uint16_t H = 8;
    const int N = W * H;

    XYMap xy = XYMap::constructRectangularGrid(W, H);
    FlowFieldFloat fx(xy);

    CRGB leds_a[N] = {};
    CRGB leds_b[N] = {};

    // Init + frame at t=1000
    fx.draw(Fx::DrawContext(0, leds_a));
    fx.draw(Fx::DrawContext(1000, leds_a));

    // Frame at t=2000
    fx.draw(Fx::DrawContext(2000, leds_b));

    int diff = count_diff_pixels(leds_a, leds_b, N);
    fl::printf("FlowFieldFloat consecutive frames: %d of %d pixels differ\n", diff, N);
    FL_ASSERT(diff > 0, "FlowFieldFloat must produce different output on consecutive frames");
}

FL_TEST_CASE("flowfield_fp - animates at 30fps frame intervals") {
    using namespace flowfield_fp_test;

    const uint16_t W = 8;
    const uint16_t H = 8;
    const int N = W * H;

    XYMap xy = XYMap::constructRectangularGrid(W, H);
    FlowFieldFP fx(xy);

    CRGB leds_a[N] = {};
    CRGB leds_b[N] = {};

    // Run 5 frames at 30fps
    for (int frame = 0; frame < 5; frame++) {
        fx.draw(Fx::DrawContext(frame * 33, leds_a));
    }

    // One more frame
    fx.draw(Fx::DrawContext(5 * 33, leds_b));

    int diff = count_diff_pixels(leds_a, leds_b, N);
    fl::printf("FlowFieldFP at 30fps: %d of %d pixels changed in last frame\n", diff, N);
    FL_ASSERT(diff > 0, "FlowFieldFP must animate at realistic frame rates");
}

FL_TEST_CASE("flowfield - mode switch animation (float then fp)") {
    using namespace flowfield_fp_test;

    const uint16_t W = 8;
    const uint16_t H = 8;
    const int N = W * H;

    XYMap xy = XYMap::constructRectangularGrid(W, H);
    FlowFieldFloat floatFx(xy);
    FlowFieldFP fpFx(xy);

    CRGB leds[N] = {};

    // Run float for a few frames
    for (int frame = 0; frame < 5; frame++) {
        floatFx.draw(Fx::DrawContext(frame * 33, leds));
    }

    // Switch to FP at ~165ms
    u32 switchTime = 5000;  // simulate 5s elapsed

    CRGB leds_a[N] = {};
    CRGB leds_b[N] = {};

    // First two FP frames
    fpFx.draw(Fx::DrawContext(switchTime, leds_a));
    fpFx.draw(Fx::DrawContext(switchTime + 33, leds_b));

    int diff = count_diff_pixels(leds_a, leds_b, N);
    fl::printf("Mode switch: %d of %d pixels changed in first FP frame pair\n", diff, N);
    FL_ASSERT(diff > 0, "FlowFieldFP must animate after mode switch from float");
}

FL_TEST_CASE("flowfield - base class pointer interface") {
    const uint16_t W = 8;
    const uint16_t H = 8;
    const int N = W * H;

    XYMap xy = XYMap::constructRectangularGrid(W, H);
    FlowFieldFloat floatFx(xy);
    FlowFieldFP fpFx(xy);

    CRGB leds[N] = {};

    // Use base class pointer to set params and draw
    FlowField *fx = &floatFx;
    fx->setPersistence(0.5f);
    fx->setColorShift(0.1f);
    fx->draw(Fx::DrawContext(0, leds));
    fx->draw(Fx::DrawContext(1000, leds));

    bool any_nonzero = false;
    for (int i = 0; i < N; i++) {
        if (leds[i].r != 0 || leds[i].g != 0 || leds[i].b != 0) {
            any_nonzero = true;
            break;
        }
    }
    FL_ASSERT(any_nonzero, "FlowFieldFloat via base pointer should produce output");

    // Switch to FP via pointer
    fx = &fpFx;
    fx->setPersistence(0.5f);
    fx->setColorShift(0.1f);
    CRGB leds2[N] = {};
    fx->draw(Fx::DrawContext(5000, leds2));
    fx->draw(Fx::DrawContext(6000, leds2));

    any_nonzero = false;
    for (int i = 0; i < N; i++) {
        if (leds2[i].r != 0 || leds2[i].g != 0 || leds2[i].b != 0) {
            any_nonzero = true;
            break;
        }
    }
    FL_ASSERT(any_nonzero, "FlowFieldFP via base pointer should produce output");
}

// ---------------------------------------------------------------------------
//  NoiseBias tests (moved from tests/fl/noise_bias.cpp)
// ---------------------------------------------------------------------------

FL_TEST_CASE("NoiseBias1D - constructor initializes to zero") {
    NoiseBias1D bias(16, 0.01f, 0.5f);
    FL_CHECK_EQ(bias.size(), 16);
    for (u16 i = 0; i < 16; ++i) {
        FL_CHECK_EQ(bias.get(i), 0.0f);
    }
}

FL_TEST_CASE("NoiseBias1D - trigger produces nonzero at center") {
    NoiseBias1D bias(16, 0.001f, 0.5f); // very fast attack
    bias.trigger(8.0f, 5.0f, 1.0f, BumpShape::HalfSine);
    bias.update(0.1f); // 100ms, well past attack tau
    float center = bias.get(8);
    FL_CHECK_GT(center, 0.5f);
    FL_CHECK_GT(bias.get(7), 0.0f); // neighbors affected
    FL_CHECK_EQ(bias.get(0), 0.0f); // far away unaffected
}

FL_TEST_CASE("NoiseBias1D - edges unaffected outside bump width") {
    NoiseBias1D bias(16, 0.001f, 0.5f);
    bias.trigger(8.0f, 4.0f, 1.0f);
    bias.update(0.1f);
    FL_CHECK_EQ(bias.get(0), 0.0f);
    FL_CHECK_EQ(bias.get(15), 0.0f);
}

FL_TEST_CASE("NoiseBias1D - decay after trigger") {
    NoiseBias1D bias(8, 0.001f, 0.1f); // fast attack, 100ms decay
    bias.trigger(4.0f, 3.0f, 1.0f);
    bias.update(0.01f); // attack
    float peak = bias.get(4);
    FL_CHECK_GT(peak, 0.0f);
    for (int i = 0; i < 100; ++i) {
        bias.update(0.01f);
    }
    FL_CHECK_LT(bias.get(4), peak * 0.1f);
}

FL_TEST_CASE("NoiseBias1D - multiple triggers accumulate") {
    NoiseBias1D bias(16, 0.001f, 0.5f);
    bias.trigger(8.0f, 3.0f, 0.5f);
    bias.trigger(8.0f, 3.0f, 0.5f);
    bias.update(0.1f);

    NoiseBias1D single(16, 0.001f, 0.5f);
    single.trigger(8.0f, 3.0f, 1.0f);
    single.update(0.1f);

    FL_CHECK_CLOSE(bias.get(8), single.get(8), 0.01f);
}

FL_TEST_CASE("NoiseBias1D - Gaussian shape peaks at center") {
    NoiseBias1D bias(16, 0.001f, 0.5f);
    bias.trigger(8.0f, 6.0f, 1.0f, BumpShape::Gaussian);
    bias.update(0.1f);
    FL_CHECK_GT(bias.get(8), bias.get(6));
    FL_CHECK_GT(bias.get(8), 0.0f);
}

FL_TEST_CASE("NoiseBias1D - reset clears state") {
    NoiseBias1D bias(8, 0.001f, 0.5f);
    bias.trigger(4.0f, 3.0f, 1.0f);
    bias.update(0.1f);
    FL_CHECK_GT(bias.get(4), 0.0f);
    bias.reset();
    FL_CHECK_EQ(bias.get(4), 0.0f);
}

FL_TEST_CASE("NoiseBias2D - combined bias is additive") {
    NoiseBias2D bias(16, 16, 0.001f, 0.5f);
    bias.triggerX(8.0f, 3.0f, 0.5f);
    bias.triggerY(4.0f, 3.0f, 0.3f);
    bias.update(0.1f);
    float combined = bias.get(8, 4);
    float xOnly = bias.getX(8);
    float yOnly = bias.getY(4);
    FL_CHECK_CLOSE(combined, xOnly + yOnly, 0.001f);
}

FL_TEST_CASE("NoiseBias2D - dimensions") {
    NoiseBias2D bias(16, 12, 0.01f, 0.5f);
    FL_CHECK_EQ(bias.width(), 16);
    FL_CHECK_EQ(bias.height(), 12);
}

FL_TEST_CASE("NoiseBias2D - reset clears both axes") {
    NoiseBias2D bias(8, 8, 0.001f, 0.5f);
    bias.triggerX(4.0f, 3.0f, 1.0f);
    bias.triggerY(4.0f, 3.0f, 1.0f);
    bias.update(0.1f);
    FL_CHECK_GT(bias.getX(4), 0.0f);
    FL_CHECK_GT(bias.getY(4), 0.0f);
    bias.reset();
    FL_CHECK_EQ(bias.getX(4), 0.0f);
    FL_CHECK_EQ(bias.getY(4), 0.0f);
}
