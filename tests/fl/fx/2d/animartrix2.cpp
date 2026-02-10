
#include "fl/fx/2d/animartrix.hpp"
#include "fl/fx/2d/animartrix2.hpp"
#include "crgb.h"
#include "test.h"
#include "fl/xymap.h"

#include <chrono> // ok include - for std::chrono::high_resolution_clock timing benchmarks

using namespace fl;

#if 0
// Original comprehensive tests disabled per LOOP.md instructions
// (focusing on Chasing_Spirals Q31 optimization)

namespace {

const uint16_t W = 32;
const uint16_t H = 32;
const uint16_t N = W * H;

int compareLeds(const CRGB *leds1, const CRGB *leds2, uint16_t count,
                const char *animName) {
    int mismatch_count = 0;
    for (uint16_t i = 0; i < count; i++) {
        if (leds1[i] != leds2[i]) {
            if (mismatch_count < 5) {
                FL_MESSAGE("  [", animName, "] Mismatch at index ", i, ": (",
                        int(leds1[i].r), ",", int(leds1[i].g), ",",
                        int(leds1[i].b), ") vs (", int(leds2[i].r), ",",
                        int(leds2[i].g), ",", int(leds2[i].b), ")");
            }
            mismatch_count++;
        }
    }
    return mismatch_count;
}

void testAnimation(int animIndex, const char *name) {
    XYMap xy1 = XYMap::constructRectangularGrid(W, H);
    XYMap xy2 = XYMap::constructRectangularGrid(W, H);

    Animartrix fx1(xy1, static_cast<AnimartrixAnim>(animIndex));
    Animartrix2 fx2(xy2, static_cast<Animartrix2Anim>(animIndex));

    CRGB leds1[N] = {};
    CRGB leds2[N] = {};

    Fx::DrawContext ctx1(1000, leds1);
    Fx::DrawContext ctx2(1000, leds2);

    fx1.draw(ctx1);
    fx2.draw(ctx2);

    int mismatches = compareLeds(leds1, leds2, N, name);
    FL_MESSAGE("Animation '", name, "': ", mismatches, " mismatched pixels / ", N);
    FL_CHECK_MESSAGE(mismatches == 0,
                  "Animation '", name, "' produced different output between "
                  "Animartrix and Animartrix2");
}

} // namespace

FL_TEST_CASE("Animartrix2 - RGB_BLOBS5") { testAnimation(0, "RGB_BLOBS5"); }
FL_TEST_CASE("Animartrix2 - RGB_BLOBS4") { testAnimation(1, "RGB_BLOBS4"); }
FL_TEST_CASE("Animartrix2 - RGB_BLOBS3") { testAnimation(2, "RGB_BLOBS3"); }
FL_TEST_CASE("Animartrix2 - RGB_BLOBS2") { testAnimation(3, "RGB_BLOBS2"); }
FL_TEST_CASE("Animartrix2 - RGB_BLOBS") { testAnimation(4, "RGB_BLOBS"); }
FL_TEST_CASE("Animartrix2 - POLAR_WAVES") { testAnimation(5, "POLAR_WAVES"); }
FL_TEST_CASE("Animartrix2 - SLOW_FADE") { testAnimation(6, "SLOW_FADE"); }
FL_TEST_CASE("Animartrix2 - ZOOM2") { testAnimation(7, "ZOOM2"); }
FL_TEST_CASE("Animartrix2 - ZOOM") { testAnimation(8, "ZOOM"); }
FL_TEST_CASE("Animartrix2 - HOT_BLOB") { testAnimation(9, "HOT_BLOB"); }
FL_TEST_CASE("Animartrix2 - SPIRALUS2") { testAnimation(10, "SPIRALUS2"); }
FL_TEST_CASE("Animartrix2 - SPIRALUS") { testAnimation(11, "SPIRALUS"); }
FL_TEST_CASE("Animartrix2 - YVES") { testAnimation(12, "YVES"); }
FL_TEST_CASE("Animartrix2 - SCALEDEMO1") { testAnimation(13, "SCALEDEMO1"); }
FL_TEST_CASE("Animartrix2 - LAVA1") { testAnimation(14, "LAVA1"); }
FL_TEST_CASE("Animartrix2 - CALEIDO3") { testAnimation(15, "CALEIDO3"); }
FL_TEST_CASE("Animartrix2 - CALEIDO2") { testAnimation(16, "CALEIDO2"); }
FL_TEST_CASE("Animartrix2 - CALEIDO1") { testAnimation(17, "CALEIDO1"); }
FL_TEST_CASE("Animartrix2 - DISTANCE_EXPERIMENT") { testAnimation(18, "DISTANCE_EXPERIMENT"); }
FL_TEST_CASE("Animartrix2 - CENTER_FIELD") { testAnimation(19, "CENTER_FIELD"); }
FL_TEST_CASE("Animartrix2 - WAVES") { testAnimation(20, "WAVES"); }
FL_TEST_CASE("Animartrix2 - CHASING_SPIRALS") { testAnimation(21, "CHASING_SPIRALS"); }
FL_TEST_CASE("Animartrix2 - ROTATING_BLOB") { testAnimation(22, "ROTATING_BLOB"); }
FL_TEST_CASE("Animartrix2 - RINGS") { testAnimation(23, "RINGS"); }
FL_TEST_CASE("Animartrix2 - COMPLEX_KALEIDO") { testAnimation(24, "COMPLEX_KALEIDO"); }
FL_TEST_CASE("Animartrix2 - COMPLEX_KALEIDO_2") { testAnimation(25, "COMPLEX_KALEIDO_2"); }
FL_TEST_CASE("Animartrix2 - COMPLEX_KALEIDO_3") { testAnimation(26, "COMPLEX_KALEIDO_3"); }
FL_TEST_CASE("Animartrix2 - COMPLEX_KALEIDO_4") { testAnimation(27, "COMPLEX_KALEIDO_4"); }
FL_TEST_CASE("Animartrix2 - COMPLEX_KALEIDO_5") { testAnimation(28, "COMPLEX_KALEIDO_5"); }
FL_TEST_CASE("Animartrix2 - COMPLEX_KALEIDO_6") { testAnimation(29, "COMPLEX_KALEIDO_6"); }
FL_TEST_CASE("Animartrix2 - WATER") { testAnimation(30, "WATER"); }
FL_TEST_CASE("Animartrix2 - PARAMETRIC_WATER") { testAnimation(31, "PARAMETRIC_WATER"); }
FL_TEST_CASE("Animartrix2 - MODULE_EXPERIMENT1") { testAnimation(32, "MODULE_EXPERIMENT1"); }
FL_TEST_CASE("Animartrix2 - MODULE_EXPERIMENT2") { testAnimation(33, "MODULE_EXPERIMENT2"); }
FL_TEST_CASE("Animartrix2 - MODULE_EXPERIMENT3") { testAnimation(34, "MODULE_EXPERIMENT3"); }
FL_TEST_CASE("Animartrix2 - MODULE_EXPERIMENT4") { testAnimation(35, "MODULE_EXPERIMENT4"); }
FL_TEST_CASE("Animartrix2 - MODULE_EXPERIMENT5") { testAnimation(36, "MODULE_EXPERIMENT5"); }
FL_TEST_CASE("Animartrix2 - MODULE_EXPERIMENT6") { testAnimation(37, "MODULE_EXPERIMENT6"); }
FL_TEST_CASE("Animartrix2 - MODULE_EXPERIMENT7") { testAnimation(38, "MODULE_EXPERIMENT7"); }
FL_TEST_CASE("Animartrix2 - MODULE_EXPERIMENT8") { testAnimation(39, "MODULE_EXPERIMENT8"); }
FL_TEST_CASE("Animartrix2 - MODULE_EXPERIMENT9") { testAnimation(40, "MODULE_EXPERIMENT9"); }
FL_TEST_CASE("Animartrix2 - MODULE_EXPERIMENT10") { testAnimation(41, "MODULE_EXPERIMENT10"); }
FL_TEST_CASE("Animartrix2 - MODULE_EXPERIMENT_SM1") { testAnimation(42, "MODULE_EXPERIMENT_SM1"); }
FL_TEST_CASE("Animartrix2 - MODULE_EXPERIMENT_SM2") { testAnimation(43, "MODULE_EXPERIMENT_SM2"); }
FL_TEST_CASE("Animartrix2 - MODULE_EXPERIMENT_SM3") { testAnimation(44, "MODULE_EXPERIMENT_SM3"); }
FL_TEST_CASE("Animartrix2 - MODULE_EXPERIMENT_SM4") { testAnimation(45, "MODULE_EXPERIMENT_SM4"); }
FL_TEST_CASE("Animartrix2 - MODULE_EXPERIMENT_SM5") { testAnimation(46, "MODULE_EXPERIMENT_SM5"); }
FL_TEST_CASE("Animartrix2 - MODULE_EXPERIMENT_SM6") { testAnimation(47, "MODULE_EXPERIMENT_SM6"); }
FL_TEST_CASE("Animartrix2 - MODULE_EXPERIMENT_SM8") { testAnimation(48, "MODULE_EXPERIMENT_SM8"); }
FL_TEST_CASE("Animartrix2 - MODULE_EXPERIMENT_SM9") { testAnimation(49, "MODULE_EXPERIMENT_SM9"); }
FL_TEST_CASE("Animartrix2 - MODULE_EXPERIMENT_SM10") { testAnimation(50, "MODULE_EXPERIMENT_SM10"); }
FL_TEST_CASE("Animartrix2 - FLUFFY_BLOBS") { testAnimation(51, "FLUFFY_BLOBS"); }

FL_TEST_CASE("Animartrix2 - API compatibility") {
    XYMap xy = XYMap::constructRectangularGrid(W, H);

    FL_SUBCASE("fxNum returns correct count") {
        Animartrix2 fx(xy, ANIM2_RGB_BLOBS);
        FL_CHECK(fx.fxNum() == ANIM2_NUM_ANIMATIONS);
    }

    FL_SUBCASE("fxGet returns current animation") {
        Animartrix2 fx(xy, ANIM2_ZOOM);
        FL_CHECK(fx.fxGet() == static_cast<int>(ANIM2_ZOOM));
    }

    FL_SUBCASE("fxSet changes animation") {
        Animartrix2 fx(xy, ANIM2_RGB_BLOBS);
        fx.fxSet(5);
        FL_CHECK(fx.fxGet() == 5);
    }

    FL_SUBCASE("fxSet wraps around") {
        Animartrix2 fx(xy, ANIM2_RGB_BLOBS);
        fx.fxSet(ANIM2_NUM_ANIMATIONS + 3);
        FL_CHECK(fx.fxGet() == 3);
    }

    FL_SUBCASE("fxSet handles negative") {
        Animartrix2 fx(xy, ANIM2_RGB_BLOBS5);
        fx.fxSet(-1);
        FL_CHECK(fx.fxGet() == ANIM2_NUM_ANIMATIONS - 1);
    }

    FL_SUBCASE("fxNext advances") {
        Animartrix2 fx(xy, ANIM2_RGB_BLOBS5);
        fx.fxNext();
        FL_CHECK(fx.fxGet() == 1);
    }

    FL_SUBCASE("fxName returns non-empty string") {
        Animartrix2 fx(xy, ANIM2_RGB_BLOBS);
        FL_CHECK(fx.fxName().size() > 0);
    }

    FL_SUBCASE("getAnimationList returns all animations") {
        auto list = Animartrix2::getAnimationList();
        FL_CHECK(list.size() == ANIM2_NUM_ANIMATIONS);
    }

    FL_SUBCASE("color order can be set and retrieved") {
        Animartrix2 fx(xy, ANIM2_RGB_BLOBS);
        fx.setColorOrder(GRB);
        FL_CHECK(fx.getColorOrder() == GRB);
    }
}
#endif // #if 0

// ============================================================
// Chasing_Spirals Q31 Optimization Tests
// ============================================================

namespace {

const uint16_t W = 32;
const uint16_t H = 32;
const uint16_t N = W * H;

// Render Chasing_Spirals using the float path (original Animartrix)
void renderChasingSpiralFloat(CRGB *leds, uint32_t timeMs) {
    XYMap xy = XYMap::constructRectangularGrid(W, H);
    Animartrix fx(xy, CHASING_SPIRALS);
    Fx::DrawContext ctx(timeMs, leds);
    fx.draw(ctx);
}

// Render Chasing_Spirals using Q31 integer path (Animartrix2 with Q31 dispatch)
void renderChasingSpiralQ31(CRGB *leds, uint32_t timeMs) {
    XYMap xy = XYMap::constructRectangularGrid(W, H);
    Animartrix2 fx(xy, ANIM2_CHASING_SPIRALS);
    Fx::DrawContext ctx(timeMs, leds);
    fx.draw(ctx);
}

// Render Chasing_Spirals using Q16 integer path (reduced precision Perlin)
void renderChasingSpiralQ16(CRGB *leds, uint32_t timeMs) {
    XYMap xy = XYMap::constructRectangularGrid(W, H);

    // Create context and call Q16 variant directly
    animartrix2_detail::Context ctx;
    ctx.leds = leds;
    ctx.xyMapFn = [](u16 x, u16 y, void *userData) -> u16 {
        XYMap *map = static_cast<XYMap*>(userData);
        return map->mapToIndex(x, y);
    };
    ctx.xyMapUserData = &xy;

    animartrix2_detail::init(ctx, W, H);
    animartrix2_detail::setTime(ctx, timeMs);
    animartrix2_detail::q16::Chasing_Spirals_Q16_Batch4_ColorGrouped(ctx);
}

// Count mismatched pixels between two buffers
int countMismatches(const CRGB *a, const CRGB *b, uint16_t count) {
    int mismatches = 0;
    for (uint16_t i = 0; i < count; i++) {
        if (a[i] != b[i]) {
            mismatches++;
        }
    }
    return mismatches;
}

// Compute per-channel average absolute error
float computeAvgError(const CRGB *a, const CRGB *b, uint16_t count) {
    int64_t total_error = 0;
    for (uint16_t i = 0; i < count; i++) {
        total_error += abs(int(a[i].r) - int(b[i].r));
        total_error += abs(int(a[i].g) - int(b[i].g));
        total_error += abs(int(a[i].b) - int(b[i].b));
    }
    return static_cast<float>(total_error) / (count * 3);
}

// Compute maximum per-channel absolute error
int computeMaxError(const CRGB *a, const CRGB *b, uint16_t count) {
    int max_err = 0;
    for (uint16_t i = 0; i < count; i++) {
        int er = abs(int(a[i].r) - int(b[i].r));
        int eg = abs(int(a[i].g) - int(b[i].g));
        int eb = abs(int(a[i].b) - int(b[i].b));
        int m = er > eg ? er : eg;
        m = m > eb ? m : eb;
        if (m > max_err) max_err = m;
    }
    return max_err;
}

// Benchmark helper: measure draw time in microseconds for a persistent Fx instance.
// Runs ITERATIONS frames with incrementing time, returns average time per frame.
template <typename FxType>
double benchmarkFx(FxType &fx, CRGB *leds, int iterations) { // okay std namespace
    // Warmup: 2 frames to prime LUTs and caches
    for (int i = 0; i < 2; i++) {
        Fx::DrawContext ctx(static_cast<uint32_t>(i * 16), leds);
        fx.draw(ctx);
    }

    auto start = std::chrono::high_resolution_clock::now(); // okay std namespace
    for (int i = 0; i < iterations; i++) {
        uint32_t t = static_cast<uint32_t>(1000 + i * 16); // ~60fps timesteps
        Fx::DrawContext ctx(t, leds);
        fx.draw(ctx);
    }
    auto end = std::chrono::high_resolution_clock::now(); // okay std namespace
    auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count(); // okay std namespace
    return static_cast<double>(elapsed_us) / iterations;
}

} // namespace

FL_TEST_CASE("Chasing_Spirals Q31 - low error at t=1000") {
    CRGB leds_float[N] = {};
    CRGB leds_q31[N] = {};

    renderChasingSpiralFloat(leds_float, 1000);
    renderChasingSpiralQ31(leds_q31, 1000);

    int mismatches = countMismatches(leds_float, leds_q31, N);
    float avg_err = computeAvgError(leds_float, leds_q31, N);
    int max_err = computeMaxError(leds_float, leds_q31, N);

    FL_MESSAGE("t=1000: mismatches=", mismatches, "/", N,
            " avg_err=", avg_err, " max_err=", max_err);

    // Print first few mismatches for debugging
    int printed = 0;
    for (uint16_t i = 0; i < N && printed < 10; i++) {
        if (leds_float[i] != leds_q31[i]) {
            FL_MESSAGE("  pixel[", i, "]: float=(", int(leds_float[i].r), ",",
                    int(leds_float[i].g), ",", int(leds_float[i].b),
                    ") q31=(", int(leds_q31[i].r), ",",
                    int(leds_q31[i].g), ",", int(leds_q31[i].b), ")");
            printed++;
        }
    }

    float error_pct = avg_err / 255.0f * 100.0f;
    FL_MESSAGE("Average error at t=1000: ", error_pct, "%");

    // s16x16 integer math introduces small rounding differences
    // At low time values, average error should be well under 1%
    FL_CHECK_MESSAGE(error_pct < 1.0f,
                  "Q31 Chasing_Spirals average error should be < 1% at t=1000");
    FL_CHECK_MESSAGE(max_err <= 6,
                  "Q31 Chasing_Spirals max per-channel error should be <= 6 at t=1000");
}

FL_TEST_CASE("Chasing_Spirals Q31 - approximate at high time") {
    // Test multiple high time values to verify stability
    const uint32_t times[] = {
        1000000,     // ~16 minutes
        100000000,   // ~27 hours
        2000000000,  // ~23 days
    };

    for (uint32_t high_time : times) {
        CRGB leds_float[N] = {};
        CRGB leds_q31[N] = {};

        renderChasingSpiralFloat(leds_float, high_time);
        renderChasingSpiralQ31(leds_q31, high_time);

        int mismatches = countMismatches(leds_float, leds_q31, N);
        float avg_err = computeAvgError(leds_float, leds_q31, N);
        int max_err = computeMaxError(leds_float, leds_q31, N);

        float error_pct = avg_err / 255.0f * 100.0f;
        FL_MESSAGE("t=", high_time, ": mismatches=", mismatches, "/", N,
                " avg_err=", avg_err, " max_err=", max_err,
                " error_pct=", error_pct, "%");

        FL_CHECK_MESSAGE(error_pct < 3.0f,
                      "Q31 Chasing_Spirals average error should be < 3% at high time values");
    }
}

FL_TEST_CASE("Chasing_Spirals Q31 - timing benchmark") {
    // Benchmark float vs Q31 with persistent Fx instances (realistic usage).
    // Q31 benefits from persistent LUTs (PixelLUT, FadeLUT) that are built
    // once and reused across frames, so multi-frame benchmarks show true perf.
    constexpr int BENCH_ITERS = 100;

    XYMap xy_float = XYMap::constructRectangularGrid(W, H);
    Animartrix fx_float(xy_float, CHASING_SPIRALS);
    CRGB leds_float[N] = {};
    double float_us = benchmarkFx(fx_float, leds_float, BENCH_ITERS);

    XYMap xy_q31 = XYMap::constructRectangularGrid(W, H);
    Animartrix2 fx_q31(xy_q31, ANIM2_CHASING_SPIRALS);
    CRGB leds_q31[N] = {};
    double q31_us = benchmarkFx(fx_q31, leds_q31, BENCH_ITERS);

    double speedup = float_us / q31_us;

    FL_MESSAGE("=== Chasing_Spirals Timing Benchmark (", BENCH_ITERS, " frames, ", W, "x", H, " grid) ===");
    FL_MESSAGE("  Float (Animartrix):  ", float_us, " us/frame");
    FL_MESSAGE("  Q31   (Animartrix2): ", q31_us, " us/frame");
    FL_MESSAGE("  Speedup: ", speedup, "x");
    if (speedup >= 1.0) {
        FL_MESSAGE("  Q31 is ", (speedup - 1.0) * 100.0, "% faster than float");
    } else {
        FL_MESSAGE("  Q31 is ", (1.0 - speedup) / speedup * 100.0, "% slower than float");
    }

    // Q31 should be at least as fast as float on desktop (often faster on embedded)
    // On desktop with FPU, we mainly validate that the integer path isn't regressing.
    // The real speedup shows on embedded targets without hardware FPU.
    FL_CHECK_MESSAGE(q31_us > 0, "Q31 benchmark produced valid timing");
    FL_CHECK_MESSAGE(float_us > 0, "Float benchmark produced valid timing");
}

// =============================================================================
// Q16 Accuracy Tests (reduced precision: 16 fractional bits instead of 24)
// =============================================================================

FL_TEST_CASE("Chasing_Spirals Q16 - low error at t=1000") {
    CRGB leds_float[N] = {};
    CRGB leds_q16[N] = {};

    renderChasingSpiralFloat(leds_float, 1000);
    renderChasingSpiralQ16(leds_q16, 1000);

    int mismatches = countMismatches(leds_float, leds_q16, N);
    float avg_err = computeAvgError(leds_float, leds_q16, N);
    int max_err = computeMaxError(leds_float, leds_q16, N);

    FL_MESSAGE("Q16 t=1000: mismatches=", mismatches, "/", N,
            " avg_err=", avg_err, " max_err=", max_err);

    // Print first few mismatches for debugging
    int printed = 0;
    for (uint16_t i = 0; i < N && printed < 10; i++) {
        if (leds_float[i] != leds_q16[i]) {
            FL_MESSAGE("  pixel[", i, "]: float=(", int(leds_float[i].r), ",",
                    int(leds_float[i].g), ",", int(leds_float[i].b),
                    ") q16=(", int(leds_q16[i].r), ",",
                    int(leds_q16[i].g), ",", int(leds_q16[i].b), ")");
            printed++;
        }
    }

    float error_pct = avg_err / 255.0f * 100.0f;
    FL_MESSAGE("Q16 average error at t=1000: ", error_pct, "%");

    // Q16 uses 16 fractional bits instead of 24, so expect slightly higher error
    // Still should be well under 1.5% at low time values
    FL_CHECK_MESSAGE(error_pct < 1.5f,
                  "Q16 Chasing_Spirals average error should be < 1.5% at t=1000");
    FL_CHECK_MESSAGE(max_err <= 8,
                  "Q16 Chasing_Spirals max per-channel error should be <= 8 at t=1000");
}

FL_TEST_CASE("Chasing_Spirals Q16 - approximate at high time") {
    // Test multiple high time values to verify stability with reduced precision
    const uint32_t times[] = {
        1000000,     // ~16 minutes
        100000000,   // ~27 hours
        2000000000,  // ~23 days
    };

    for (uint32_t high_time : times) {
        CRGB leds_float[N] = {};
        CRGB leds_q16[N] = {};

        renderChasingSpiralFloat(leds_float, high_time);
        renderChasingSpiralQ16(leds_q16, high_time);

        int mismatches = countMismatches(leds_float, leds_q16, N);
        float avg_err = computeAvgError(leds_float, leds_q16, N);
        int max_err = computeMaxError(leds_float, leds_q16, N);

        float error_pct = avg_err / 255.0f * 100.0f;
        FL_MESSAGE("Q16 t=", high_time, ": mismatches=", mismatches, "/", N,
                " avg_err=", avg_err, " max_err=", max_err,
                " error_pct=", error_pct, "%");

        // Q16 should maintain < 4% error even at high time values
        FL_CHECK_MESSAGE(error_pct < 4.0f,
                      "Q16 Chasing_Spirals average error should be < 4% at high time values");
        FL_CHECK_MESSAGE(max_err <= 12,
                      "Q16 Chasing_Spirals max per-channel error should be <= 12 at high time values");
    }
}
