// ok standalone
// Pixel accuracy test: Float vs Q31 Chasing_Spirals comparison
//
// Compares RGB output pixel-by-pixel to determine:
// 1. Error distribution (min, max, average, std dev)
// 2. Whether errors exceed 1 LSB (±1 in 8-bit RGB)
// 3. Where errors are coming from (which stage of computation)

#include "test.h"
#include "fl/fx/2d/animartrix.hpp"
#include "fl/fx/2d/animartrix2.hpp"
#include "fl/fx/2d/animartrix2_detail/viz/chasing_spirals.h"
#include "fl/xymap.h"
#include <stdio.h>  // ok include: needed for debug output

using namespace fl;

struct ErrorStats {
    int max_error;
    double avg_error;
    double std_dev;
    int pixels_with_error;
    int pixels_over_1bit;
    int pixels_over_2bit;
    int pixels_over_4bit;
    int total_pixels;

    // Histogram: count of pixels by error magnitude
    int histogram[256];  // histogram[i] = count of pixels with max channel error == i
};

ErrorStats analyze_errors(const CRGB *float_leds, const CRGB *q31_leds, int count) {
    ErrorStats stats = {};

    for (int i = 0; i < count; i++) {
        int r_err = abs(static_cast<int>(float_leds[i].r) - static_cast<int>(q31_leds[i].r));
        int g_err = abs(static_cast<int>(float_leds[i].g) - static_cast<int>(q31_leds[i].g));
        int b_err = abs(static_cast<int>(float_leds[i].b) - static_cast<int>(q31_leds[i].b));

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

        stats.histogram[pixel_max_err]++;
    }

    stats.total_pixels = count;
    if (stats.pixels_with_error > 0) {
        stats.avg_error /= stats.pixels_with_error;
    }

    // Calculate standard deviation
    double variance = 0.0;
    for (int i = 0; i < count; i++) {
        int r_err = abs(static_cast<int>(float_leds[i].r) - static_cast<int>(q31_leds[i].r));
        int g_err = abs(static_cast<int>(float_leds[i].g) - static_cast<int>(q31_leds[i].g));
        int b_err = abs(static_cast<int>(float_leds[i].b) - static_cast<int>(q31_leds[i].b));

        int pixel_max_err = r_err;
        if (g_err > pixel_max_err) pixel_max_err = g_err;
        if (b_err > pixel_max_err) pixel_max_err = b_err;

        double diff = pixel_max_err - stats.avg_error;
        variance += diff * diff;
    }
    stats.std_dev = fl::sqrt(variance / count);

    return stats;
}

void print_error_stats(const ErrorStats &stats, const char *test_name) {
    fprintf(stderr, "\n=== %s ===\n", test_name);
    fprintf(stderr, "Total pixels: %d\n", stats.total_pixels);
    fprintf(stderr, "Pixels with error: %d (%.1f%%)\n",
            stats.pixels_with_error,
            100.0 * stats.pixels_with_error / stats.total_pixels);
    fprintf(stderr, "Max error: %d (%.1f%%)\n", stats.max_error, 100.0 * stats.max_error / 255);
    fprintf(stderr, "Avg error: %.2f\n", stats.avg_error);
    fprintf(stderr, "Std dev: %.2f\n", stats.std_dev);
    fprintf(stderr, "\nError distribution:\n");
    fprintf(stderr, "  >1 LSB (>1):  %d pixels (%.1f%%)\n",
            stats.pixels_over_1bit,
            100.0 * stats.pixels_over_1bit / stats.total_pixels);
    fprintf(stderr, "  >2 LSB (>2):  %d pixels (%.1f%%)\n",
            stats.pixels_over_2bit,
            100.0 * stats.pixels_over_2bit / stats.total_pixels);
    fprintf(stderr, "  >4 LSB (>4):  %d pixels (%.1f%%)\n",
            stats.pixels_over_4bit,
            100.0 * stats.pixels_over_4bit / stats.total_pixels);

    fprintf(stderr, "\nHistogram (first 20 buckets):\n");
    for (int i = 0; i < 20 && i <= stats.max_error; i++) {
        if (stats.histogram[i] > 0) {
            fprintf(stderr, "  Error=%2d: %4d pixels (%.1f%%)\n",
                    i, stats.histogram[i],
                    100.0 * stats.histogram[i] / stats.total_pixels);
        }
    }
    fflush(stderr);
}

FL_TEST_CASE("chasing_spirals - float vs q31 accuracy (t=1000)") {
    const uint16_t W = 32;
    const uint16_t H = 32;
    const int N = W * H;
    const uint32_t t = 1000;  // Low time value

    CRGB float_leds[N] = {};
    CRGB q31_leds[N] = {};

    XYMap xy = XYMap::constructRectangularGrid(W, H);

    // ========================
    // Float version (Animartrix)
    // ========================
    {
        Animartrix fx(xy, CHASING_SPIRALS);
        Fx::DrawContext ctx(t, float_leds);
        fx.draw(ctx);
    }

    // ========================
    // Q31 version (Direct function call)
    // ========================
    {
        fl::Context ctx;
        ctx.leds = q31_leds;
        ctx.xyMapFn = [](u16 x, u16 y, void *userData) -> u16 {
            XYMap *map = static_cast<XYMap*>(userData);
            return map->mapToIndex(x, y);
        };
        ctx.xyMapUserData = &xy;

        fl::init(ctx, W, H);
        fl::setTime(ctx, t);
        fl::Chasing_Spirals_Q31(ctx);
    }

    // ========================
    // Analyze errors
    // ========================
    ErrorStats stats = analyze_errors(float_leds, q31_leds, N);
    print_error_stats(stats, "Chasing Spirals Accuracy (t=1000)");

    // ========================
    // Assertions
    // ========================
    if (stats.max_error > 10) {
        fprintf(stderr, "FAIL: Max error %d exceeds threshold of 10\n", stats.max_error);
        fflush(stderr);
    }
    FL_ASSERT(stats.max_error <= 10, "Max error exceeded threshold");

    if (stats.avg_error > 3.0) {
        fprintf(stderr, "FAIL: Avg error %.2f exceeds threshold of 3.0\n", stats.avg_error);
        fflush(stderr);
    }
    FL_ASSERT(stats.avg_error <= 3.0, "Average error exceeded threshold");

    if (stats.pixels_over_4bit >= N / 10) {
        fprintf(stderr, "FAIL: %.1f%% of pixels have >4 LSB error (threshold: 10%%)\n",
                100.0 * stats.pixels_over_4bit / N);
        fflush(stderr);
    }
    FL_ASSERT(stats.pixels_over_4bit < N / 10, "Too many pixels with >4 LSB error");
}

FL_TEST_CASE("chasing_spirals - float vs q31 accuracy (t=1000000)") {
    const uint16_t W = 32;
    const uint16_t H = 32;
    const int N = W * H;
    const uint32_t t = 1000000;  // High time value (stress test)

    CRGB float_leds[N] = {};
    CRGB q31_leds[N] = {};

    XYMap xy = XYMap::constructRectangularGrid(W, H);

    // ========================
    // Float version (Animartrix)
    // ========================
    {
        Animartrix fx(xy, CHASING_SPIRALS);
        Fx::DrawContext ctx(t, float_leds);
        fx.draw(ctx);
    }

    // ========================
    // Q31 version (Direct function call)
    // ========================
    {
        fl::Context ctx;
        ctx.leds = q31_leds;
        ctx.xyMapFn = [](u16 x, u16 y, void *userData) -> u16 {
            XYMap *map = static_cast<XYMap*>(userData);
            return map->mapToIndex(x, y);
        };
        ctx.xyMapUserData = &xy;

        fl::init(ctx, W, H);
        fl::setTime(ctx, t);
        fl::Chasing_Spirals_Q31(ctx);
    }

    // ========================
    // Analyze errors
    // ========================
    ErrorStats stats = analyze_errors(float_leds, q31_leds, N);
    print_error_stats(stats, "Chasing Spirals Accuracy (t=1000000)");

    // ========================
    // Assertions
    // ========================
    if (stats.max_error > 20) {
        fprintf(stderr, "FAIL: Max error %d exceeds threshold of 20\n", stats.max_error);
        fflush(stderr);
    }
    FL_ASSERT(stats.max_error <= 20, "Max error exceeded threshold");

    if (stats.avg_error > 5.0) {
        fprintf(stderr, "FAIL: Avg error %.2f exceeds threshold of 5.0\n", stats.avg_error);
        fflush(stderr);
    }
    FL_ASSERT(stats.avg_error <= 5.0, "Average error exceeded threshold");
}

FL_TEST_CASE("chasing_spirals - float vs q31 sample pixels") {
    const uint16_t W = 32;
    const uint16_t H = 32;
    const int N = W * H;
    const uint32_t t = 1000;

    CRGB float_leds[N] = {};
    CRGB q31_leds[N] = {};

    XYMap xy = XYMap::constructRectangularGrid(W, H);

    // Run both implementations
    {
        Animartrix fx(xy, CHASING_SPIRALS);
        Fx::DrawContext ctx(t, float_leds);
        fx.draw(ctx);
    }

    {
        fl::Context ctx;
        ctx.leds = q31_leds;
        ctx.xyMapFn = [](u16 x, u16 y, void *userData) -> u16 {
            XYMap *map = static_cast<XYMap*>(userData);
            return map->mapToIndex(x, y);
        };
        ctx.xyMapUserData = &xy;

        fl::init(ctx, W, H);
        fl::setTime(ctx, t);
        fl::Chasing_Spirals_Q31(ctx);
    }

    // Print sample pixels to see error patterns
    fprintf(stderr, "\n=== Sample Pixel Comparison ===\n");
    fprintf(stderr, "Format: (x,y) Float RGB -> Q31 RGB (Error: R G B)\n\n");

    for (int i = 0; i < 10; i++) {
        int idx = (N / 10) * i;  // Sample evenly across grid
        int x = idx % W;
        int y = idx / W;

        CRGB f = float_leds[idx];
        CRGB q = q31_leds[idx];

        int r_err = static_cast<int>(f.r) - static_cast<int>(q.r);
        int g_err = static_cast<int>(f.g) - static_cast<int>(q.g);
        int b_err = static_cast<int>(f.b) - static_cast<int>(q.b);

        fprintf(stderr, "(%2d,%2d) (%3d,%3d,%3d) -> (%3d,%3d,%3d)  Err:(%+4d,%+4d,%+4d)\n",
                x, y, f.r, f.g, f.b, q.r, q.g, q.b, r_err, g_err, b_err);
    }
    fflush(stderr);
}
