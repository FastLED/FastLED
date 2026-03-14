// ok standalone
// Pixel accuracy test: Animartrix float vs fixed-point (Q31) Chasing_Spirals
//
// Compares RGB output pixel-by-pixel to determine:
// 1. Error distribution (min, max, average, std dev)
// 2. Whether float and Q31 fixed-point paths agree within tolerance
// 3. Where errors are coming from (which stage of computation)

#include "test.h"
#include "fl/fx/2d/animartrix.hpp"
#include "fl/fx/2d/animartrix_detail/viz/chasing_spirals.h"
#include "fl/gfx/xymap.h"
#include "fl/stl/stdio.h"

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
    fl::printf("\n=== %s ===\n", test_name);
    fl::printf("Total pixels: %d\n", stats.total_pixels);
    fl::printf("Pixels with error: %d (%.1f%%)\n",
            stats.pixels_with_error,
            100.0 * stats.pixels_with_error / stats.total_pixels);
    fl::printf("Max error: %d (%.1f%%)\n", stats.max_error, 100.0 * stats.max_error / 255);
    fl::printf("Avg error: %.2f\n", stats.avg_error);
    fl::printf("Std dev: %.2f\n", stats.std_dev);
    fl::printf("\nError distribution:\n");
    fl::printf("  >1 LSB (>1):  %d pixels (%.1f%%)\n",
            stats.pixels_over_1bit,
            100.0 * stats.pixels_over_1bit / stats.total_pixels);
    fl::printf("  >2 LSB (>2):  %d pixels (%.1f%%)\n",
            stats.pixels_over_2bit,
            100.0 * stats.pixels_over_2bit / stats.total_pixels);
    fl::printf("  >4 LSB (>4):  %d pixels (%.1f%%)\n",
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

FL_TEST_CASE("chasing_spirals - float vs q31 accuracy (t=1000)") {
    const uint16_t W = 32;
    const uint16_t H = 32;
    const int N = W * H;
    const uint32_t t = 1000;  // Low time value

    CRGB float_leds[N] = {};
    CRGB q31_leds[N] = {};

    XYMap xy = XYMap::constructRectangularGrid(W, H);

    // ========================
    // Float reference via Context + Chasing_Spirals_Float
    // ========================
    {
        Context ctx;
        ctx.leds = float_leds;
        ctx.xyMapFn = [](u16 x, u16 y, void *userData) -> u16 {
            XYMap *map = static_cast<XYMap*>(userData);
            return map->mapToIndex(x, y);
        };
        ctx.xyMapUserData = &xy;

        init(ctx, W, H);
        setTime(ctx, t);
        fl::Chasing_Spirals_Float().draw(ctx);
    }

    // ========================
    // Scalar Q31 fixed-point
    // ========================
    {
        Context ctx;
        ctx.leds = q31_leds;
        ctx.xyMapFn = [](u16 x, u16 y, void *userData) -> u16 {
            XYMap *map = static_cast<XYMap*>(userData);
            return map->mapToIndex(x, y);
        };
        ctx.xyMapUserData = &xy;

        init(ctx, W, H);
        setTime(ctx, t);
        fl::Chasing_Spirals_Q31().draw(ctx);
    }

    // ========================
    // Analyze errors
    // ========================
    ErrorStats stats = analyze_errors(float_leds, q31_leds, N);
    print_error_stats(stats, "Chasing Spirals Float vs Q31 (t=1000)");

    // ========================
    // Assertions
    // ========================
    if (stats.max_error > 10) {
        fl::printf("FAIL: Max error %d exceeds threshold of 10\n", stats.max_error);
        }
    FL_ASSERT(stats.max_error <= 10, "Max error exceeded threshold");

    if (stats.avg_error > 3.0) {
        fl::printf("FAIL: Avg error %.2f exceeds threshold of 3.0\n", stats.avg_error);
        }
    FL_ASSERT(stats.avg_error <= 3.0, "Average error exceeded threshold");

    if (stats.pixels_over_4bit >= N / 10) {
        fl::printf("FAIL: %.1f%% of pixels have >4 LSB error (threshold: 10%%)\n",
                100.0 * stats.pixels_over_4bit / N);
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
    // Float reference via Context + Chasing_Spirals_Float
    // ========================
    {
        Context ctx;
        ctx.leds = float_leds;
        ctx.xyMapFn = [](u16 x, u16 y, void *userData) -> u16 {
            XYMap *map = static_cast<XYMap*>(userData);
            return map->mapToIndex(x, y);
        };
        ctx.xyMapUserData = &xy;

        init(ctx, W, H);
        setTime(ctx, t);
        fl::Chasing_Spirals_Float().draw(ctx);
    }

    // ========================
    // Scalar Q31 fixed-point
    // ========================
    {
        Context ctx;
        ctx.leds = q31_leds;
        ctx.xyMapFn = [](u16 x, u16 y, void *userData) -> u16 {
            XYMap *map = static_cast<XYMap*>(userData);
            return map->mapToIndex(x, y);
        };
        ctx.xyMapUserData = &xy;

        init(ctx, W, H);
        setTime(ctx, t);
        fl::Chasing_Spirals_Q31().draw(ctx);
    }

    // ========================
    // Analyze errors
    // ========================
    ErrorStats stats = analyze_errors(float_leds, q31_leds, N);
    print_error_stats(stats, "Chasing Spirals Float vs Q31 (t=1000000)");

    // ========================
    // Assertions
    // ========================
    if (stats.max_error > 20) {
        fl::printf("FAIL: Max error %d exceeds threshold of 20\n", stats.max_error);
        }
    FL_ASSERT(stats.max_error <= 20, "Max error exceeded threshold");

    if (stats.avg_error > 5.0) {
        fl::printf("FAIL: Avg error %.2f exceeds threshold of 5.0\n", stats.avg_error);
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
        Context ctx;
        ctx.leds = float_leds;
        ctx.xyMapFn = [](u16 x, u16 y, void *userData) -> u16 {
            XYMap *map = static_cast<XYMap*>(userData);
            return map->mapToIndex(x, y);
        };
        ctx.xyMapUserData = &xy;

        init(ctx, W, H);
        setTime(ctx, t);
        fl::Chasing_Spirals_Float().draw(ctx);
    }

    {
        Context ctx;
        ctx.leds = q31_leds;
        ctx.xyMapFn = [](u16 x, u16 y, void *userData) -> u16 {
            XYMap *map = static_cast<XYMap*>(userData);
            return map->mapToIndex(x, y);
        };
        ctx.xyMapUserData = &xy;

        init(ctx, W, H);
        setTime(ctx, t);
        fl::Chasing_Spirals_Q31().draw(ctx);
    }

    // Print sample pixels to see error patterns
    fl::printf("\n=== Sample Pixel Comparison ===\n");
    fl::printf("Format: (x,y) Float RGB -> Q31 RGB (Error: R G B)\n\n");

    for (int i = 0; i < 10; i++) {
        int idx = (N / 10) * i;  // Sample evenly across grid
        int x = idx % W;
        int y = idx / W;

        CRGB f = float_leds[idx];
        CRGB q = q31_leds[idx];

        int r_err = static_cast<int>(f.r) - static_cast<int>(q.r);
        int g_err = static_cast<int>(f.g) - static_cast<int>(q.g);
        int b_err = static_cast<int>(f.b) - static_cast<int>(q.b);

        fl::printf("(%2d,%2d) (%3d,%3d,%3d) -> (%3d,%3d,%3d)  Err:(%+4d,%+4d,%+4d)\n",
                x, y, f.r, f.g, f.b, q.r, q.g, q.b, r_err, g_err, b_err);
    }
}
