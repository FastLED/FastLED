
#include "fl/fx/2d/animartrix.hpp"
#include "fl/fx/2d/animartrix2.hpp"
#include "crgb.h"
#include "test.h"
#include "fl/xymap.h"

#include <chrono> // ok include - for std::chrono::high_resolution_clock timing benchmarks
#include "fl/stl/iostream.h" // ok include - for debug output

using namespace fl;

namespace test_animartrix2 {

const uint16_t W = 32;
const uint16_t H = 32;
const uint16_t N = W * H;

struct AnimResult {
    const char *name;
    int index;
    int mismatched_pixels;
    float avg_error;
    int max_error;
    float error_pct;
    double v1_us; // Animartrix draw time in microseconds
    double v2_us; // Animartrix2 draw time in microseconds
};

constexpr int WARMUP_FRAMES = 1;
constexpr int BENCH_FRAMES = 3;

AnimResult testAnimation(int animIndex, const char *name, uint32_t timeMs) {
    XYMap xy1 = XYMap::constructRectangularGrid(W, H);
    XYMap xy2 = XYMap::constructRectangularGrid(W, H);

    Animartrix fx1(xy1, static_cast<AnimartrixAnim>(animIndex));
    Animartrix2 fx2(xy2, static_cast<Animartrix2Anim>(animIndex));

    CRGB leds1[N] = {};
    CRGB leds2[N] = {};

    // Pre-warm both fx instances
    for (int w = 0; w < WARMUP_FRAMES; w++) {
        Fx::DrawContext wctx1(w, leds1);
        fx1.draw(wctx1);
        Fx::DrawContext wctx2(w, leds2);
        fx2.draw(wctx2);
    }

    // Benchmark Animartrix (v1) - average over multiple frames
    auto t0 = std::chrono::high_resolution_clock::now(); // okay std namespace
    for (int f = 0; f < BENCH_FRAMES; f++) {
        Fx::DrawContext ctx1(timeMs + f, leds1);
        fx1.draw(ctx1);
    }
    auto t1 = std::chrono::high_resolution_clock::now(); // okay std namespace

    // Benchmark Animartrix2 (v2) - average over multiple frames
    auto t2 = std::chrono::high_resolution_clock::now(); // okay std namespace
    for (int f = 0; f < BENCH_FRAMES; f++) {
        Fx::DrawContext ctx2(timeMs + f, leds2);
        fx2.draw(ctx2);
    }
    auto t3 = std::chrono::high_resolution_clock::now(); // okay std namespace

    double v1_us = static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()) / (BENCH_FRAMES * 1000.0); // okay std namespace
    double v2_us = static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(t3 - t2).count()) / (BENCH_FRAMES * 1000.0); // okay std namespace

    // Draw final frame at target time for accuracy comparison
    for (uint16_t i = 0; i < N; i++) { leds1[i] = CRGB::Black; }
    for (uint16_t i = 0; i < N; i++) { leds2[i] = CRGB::Black; }
    {
        Fx::DrawContext ctx1(timeMs, leds1);
        fx1.draw(ctx1);
        Fx::DrawContext ctx2(timeMs, leds2);
        fx2.draw(ctx2);
    }

    int mismatch_count = 0;
    int64_t total_error = 0;
    int max_err = 0;

    for (uint16_t i = 0; i < N; i++) {
        int dr = abs(int(leds1[i].r) - int(leds2[i].r));
        int dg = abs(int(leds1[i].g) - int(leds2[i].g));
        int db = abs(int(leds1[i].b) - int(leds2[i].b));

        total_error += dr + dg + db;

        int m = dr > dg ? dr : dg;
        m = m > db ? m : db;
        if (m > max_err) max_err = m;

        // Count pixels with differences beyond LSB noise (±1 per channel)
        if (dr > 1 || dg > 1 || db > 1) {
            mismatch_count++;
        }
    }

    float avg_err = static_cast<float>(total_error) / (N * 3);
    float error_pct = avg_err / 255.0f * 100.0f;

    return {name, animIndex, mismatch_count, avg_err, max_err, error_pct, v1_us, v2_us};
}

// Animation names matching enum order
const char *ANIM_NAMES[] = {
    "RGB_BLOBS5",
    "RGB_BLOBS4",
    "RGB_BLOBS3",
    "RGB_BLOBS2",
    "RGB_BLOBS",
    "POLAR_WAVES",
    "SLOW_FADE",
    "ZOOM2",
    "ZOOM",
    "HOT_BLOB",
    "SPIRALUS2",
    "SPIRALUS",
    "YVES",
    "SCALEDEMO1",
    "LAVA1",
    "CALEIDO3",
    "CALEIDO2",
    "CALEIDO1",
    "DISTANCE_EXPERIMENT",
    "CENTER_FIELD",
    "WAVES",
    "CHASING_SPIRALS",
    "ROTATING_BLOB",
    "RINGS",
    "COMPLEX_KALEIDO",
    "COMPLEX_KALEIDO_2",
    "COMPLEX_KALEIDO_3",
    "COMPLEX_KALEIDO_4",
    "COMPLEX_KALEIDO_5",
    "COMPLEX_KALEIDO_6",
    "WATER",
    "PARAMETRIC_WATER",
    "MODULE_EXPERIMENT1",
    "MODULE_EXPERIMENT2",
    "MODULE_EXPERIMENT3",
    "MODULE_EXPERIMENT4",
    "MODULE_EXPERIMENT5",
    "MODULE_EXPERIMENT6",
    "MODULE_EXPERIMENT7",
    "MODULE_EXPERIMENT8",
    "MODULE_EXPERIMENT9",
    "MODULE_EXPERIMENT10",
    "MODULE_EXPERIMENT_SM1",
    "MODULE_EXPERIMENT_SM2",
    "MODULE_EXPERIMENT_SM3",
    "MODULE_EXPERIMENT_SM4",
    "MODULE_EXPERIMENT_SM5",
    "MODULE_EXPERIMENT_SM6",
    "MODULE_EXPERIMENT_SM8",
    "MODULE_EXPERIMENT_SM9",
    "MODULE_EXPERIMENT_SM10",
    "FLUFFY_BLOBS",
};

// Helper to pad a string with spaces to a given width
void printPadded(const char *s, int width) {
    int len = 0;
    for (const char *p = s; *p; p++) len++;
    fl::cout << s;
    for (int i = len; i < width; i++) fl::cout << " ";
}

// Helper to print an integer, right-aligned in a field
void printInt(int val, int width) {
    // Convert to string manually
    char buf[16];
    int pos = 0;
    if (val == 0) {
        buf[pos++] = '0';
    } else {
        int v = val < 0 ? -val : val;
        while (v > 0) {
            buf[pos++] = '0' + (v % 10);
            v /= 10;
        }
        if (val < 0) buf[pos++] = '-';
    }
    // Pad
    for (int i = pos; i < width; i++) fl::cout << " ";
    // Print reversed
    for (int i = pos - 1; i >= 0; i--) fl::cout << buf[i];
}

} // namespace test_animartrix2
using namespace test_animartrix2;

FL_TEST_CASE("Animartrix1 vs Animartrix2 - all visualizers") {
    const uint32_t timestamps[] = {0, 1};
    const int num_anims = NUM_ANIMATIONS;

    FL_CHECK(num_anims == ANIM2_NUM_ANIMATIONS);

    fl::cout << "\nNote: V1 is header-only (compiled into test TU), V2 is in fastled.dll." << fl::endl;
    fl::cout << "Perf comparison is unfair on shared-library builds (DLL call overhead)." << fl::endl;
    fl::cout << "On embedded targets (static linking), V2 float should match V1." << fl::endl;

    int total_failures = 0;

    for (uint32_t timeMs : timestamps) {
        fl::cout << "\n=== Timestamp " << timeMs << " ===" << fl::endl;

        // Table header
        fl::cout << "  ";
        printPadded("Animation", 25);
        fl::cout << " ";
        printPadded("Match", 5);
        fl::cout << " ";
        printPadded("Diff Pixels", 11);
        fl::cout << " ";
        printPadded("MaxErr", 6);
        fl::cout << " ";
        printPadded("AvgErr%", 7);
        fl::cout << " ";
        printPadded("V1 us", 8);
        fl::cout << " ";
        printPadded("V2 us", 8);
        fl::cout << " ";
        printPadded("Speedup", 8);
        fl::cout << fl::endl;

        fl::cout << "  ";
        for (int i = 0; i < 85; i++) fl::cout << "-";
        fl::cout << fl::endl;

        int failures_at_t = 0;

        for (int i = 0; i < num_anims; i++) {
            AnimResult r = testAnimation(i, ANIM_NAMES[i], timeMs);

            fl::cout << "  ";
            printPadded(r.name, 25);
            fl::cout << " ";

            if (r.mismatched_pixels > 0) {
                printPadded("DIFF", 5);
                failures_at_t++;
            } else {
                printPadded("OK", 5);
            }

            fl::cout << " ";
            printInt(r.mismatched_pixels, 5);
            fl::cout << "/" << N;

            fl::cout << " ";
            printInt(r.max_error, 6);

            fl::cout << " ";
            fl::cout << r.error_pct << "%";

            fl::cout << " ";
            fl::cout << r.v1_us;

            fl::cout << " ";
            fl::cout << r.v2_us;

            fl::cout << " ";
            if (r.v2_us > 0) {
                double speedup = r.v1_us / r.v2_us;
                fl::cout << speedup << "x";
            } else {
                fl::cout << "N/A";
            }

            fl::cout << fl::endl;
        }

        fl::cout << "  ";
        for (int i = 0; i < 85; i++) fl::cout << "-";
        fl::cout << fl::endl;
        fl::cout << "  " << failures_at_t << "/" << num_anims
                 << " animations differ" << fl::endl;

        total_failures += failures_at_t;
    }

    FL_CHECK_MESSAGE(total_failures == 0,
                     "All Animartrix1 vs Animartrix2 visualizers should match");
}
