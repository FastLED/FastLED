
#include "fl/fx/2d/animartrix.hpp"
#include "fl/fx/2d/animartrix2.hpp"
#include "crgb.h"
#include "fl/numeric_limits.h"
#include "test.h"
#include "fl/xymap.h"

#include <chrono> // ok include - for std::chrono::high_resolution_clock timing benchmarks
#include "fl/stl/iostream.h" // ok include - for debug output

using namespace fl;

// Debug: Print message during static initialization
struct DebugInit {
    DebugInit() {
        fl::cout << "DEBUG: animartrix2.cpp static initialization started" << fl::endl;
    }
} debug_init_instance;

// Simple test to verify test registration works
FL_TEST_CASE("Simple test - registration check") {
    fl::cout << "DEBUG: Simple test is running!" << fl::endl;
    FL_CHECK(1 + 1 == 2);
    fl::cout << "DEBUG: Simple test check passed!" << fl::endl;
}

FL_TEST_CASE("Simple test 2 - verify multiple tests") {
    fl::cout << "DEBUG: Simple test 2 is running!" << fl::endl;
    FL_CHECK(2 + 2 == 4);
    fl::cout << "DEBUG: Simple test 2 passed!" << fl::endl;
}

FL_TEST_CASE("Test animartrix2 instantiation") {
    fl::cout << "DEBUG: Testing animartrix2 instantiation..." << fl::endl;
    XYMap xy = XYMap::constructRectangularGrid(32, 32);
    fl::cout << "DEBUG: XYMap created" << fl::endl;
    Animartrix2 fx(xy, ANIM2_RGB_BLOBS);
    fl::cout << "DEBUG: Animartrix2 created, fxNum() = " << fx.fxNum() << fl::endl;
    FL_CHECK(fx.fxNum() > 0);
    fl::cout << "DEBUG: Animartrix2 test passed!" << fl::endl;
}

namespace {

const uint16_t W = 32;
const uint16_t H = 32;
const uint16_t N = W * H;

// Compare LEDs allowing only LSB bit differences (±1 per channel)
// Also builds a histogram of per-component differences
int compareLeds(const CRGB *leds1, const CRGB *leds2, uint16_t count,
                const char *animName) {
    int mismatch_count = 0;
    fl::array<u32, 256> diff_histogram = {}; // Initialize all bins to 0

    for (uint16_t i = 0; i < count; i++) {
        // Compute per-component differences
        int dr = abs(int(leds1[i].r) - int(leds2[i].r));
        int dg = abs(int(leds1[i].g) - int(leds2[i].g));
        int db = abs(int(leds1[i].b) - int(leds2[i].b));

        // Add to histogram
        diff_histogram[dr]++;
        diff_histogram[dg]++;
        diff_histogram[db]++;

        if (dr > 1 || dg > 1 || db > 1) {
            if (mismatch_count < 5) {
                FL_MESSAGE("  [", animName, "] Mismatch at index ", i, ": (",
                        int(leds1[i].r), ",", int(leds1[i].g), ",",
                        int(leds1[i].b), ") vs (", int(leds2[i].r), ",",
                        int(leds2[i].g), ",", int(leds2[i].b),
                        ") diff=(", dr, ",", dg, ",", db, ")");
            }
            mismatch_count++;
        }
    }

    // Print histogram of differences
    fl::cout << "Difference histogram for " << animName << ":" << fl::endl;
    for (int i = 0; i < 256; i++) {
        if (diff_histogram[i] > 0) {
            fl::cout << "  diff[" << i << "] = " << diff_histogram[i] << fl::endl;
        }
    }

    return mismatch_count;
}

void testAnimation(int animIndex, const char *name) {
    fl::cout << "DEBUG: Testing " << name << "..." << fl::endl;
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
    FL_MESSAGE("Animation '", name, "': ", mismatches, " mismatched pixels / ", N, " (LSB tolerance ±1)");
    if (mismatches > 0) {
        FL_MESSAGE("  FAILED: Animation has differences > LSB tolerance (±1 per channel)");
        fl::cout << "DEBUG: " << name << " FAILED with " << mismatches << " mismatches" << fl::endl;
    } else {
        fl::cout << "DEBUG: " << name << " PASSED!" << fl::endl;
    }
    FL_CHECK(mismatches == 0);
}

// ============================================================
// Chasing_Spirals Q31 Optimization Test Helpers
// (same namespace, reuses W, H, N constants)
// ============================================================

// Render Chasing_Spirals using the float path (original Animartrix)
void renderChasingSpiralFloat(CRGB *leds, uint32_t timeMs) {
    XYMap xy = XYMap::constructRectangularGrid(W, H);
    Animartrix fx(xy, CHASING_SPIRALS);
    Fx::DrawContext ctx(timeMs, leds);
    fx.draw(ctx);
}

// Render Chasing_Spirals using the v2 float path (Animartrix2 decoupled float)
void renderChasingSpiralFloat_v2(CRGB *leds, uint32_t timeMs) {
    XYMap xy = XYMap::constructRectangularGrid(W, H);

    Context ctx;
    ctx.leds = leds;
    ctx.xyMapFn = [](u16 x, u16 y, void *userData) -> u16 {
        XYMap *map = static_cast<XYMap*>(userData);
        return map->mapToIndex(x, y);
    };
    ctx.xyMapUserData = &xy;

    init(ctx, W, H);
    setTime(ctx, timeMs);
    fl::Chasing_Spirals_Float(ctx);
}

// Render Chasing_Spirals using Q31 integer path (non-SIMD)
void renderChasingSpiralQ31(CRGB *leds, uint32_t timeMs) {
    XYMap xy = XYMap::constructRectangularGrid(W, H);

    Context ctx;
    ctx.leds = leds;
    ctx.xyMapFn = [](u16 x, u16 y, void *userData) -> u16 {
        XYMap *map = static_cast<XYMap*>(userData);
        return map->mapToIndex(x, y);
    };
    ctx.xyMapUserData = &xy;

    init(ctx, W, H);
    setTime(ctx, timeMs);
    fl::Chasing_Spirals_Q31(ctx);
}

// Render Chasing_Spirals using Q31 SIMD path
void renderChasingSpiralQ31_SIMD(CRGB *leds, uint32_t timeMs) {
    XYMap xy = XYMap::constructRectangularGrid(W, H);

    Context ctx;
    ctx.leds = leds;
    ctx.xyMapFn = [](u16 x, u16 y, void *userData) -> u16 {
        XYMap *map = static_cast<XYMap*>(userData);
        return map->mapToIndex(x, y);
    };
    ctx.xyMapUserData = &xy;

    init(ctx, W, H);
    setTime(ctx, timeMs);
    fl::Chasing_Spirals_Q31_SIMD(ctx);
}

// Render Chasing_Spirals using Q16 integer path (reduced precision Perlin)
void renderChasingSpiralQ16(CRGB *leds, uint32_t timeMs) {
    XYMap xy = XYMap::constructRectangularGrid(W, H);

    // Create context and call Q16 variant directly
    Context ctx;
    ctx.leds = leds;
    ctx.xyMapFn = [](u16 x, u16 y, void *userData) -> u16 {
        XYMap *map = static_cast<XYMap*>(userData);
        return map->mapToIndex(x, y);
    };
    ctx.xyMapUserData = &xy;

    init(ctx, W, H);
    setTime(ctx, timeMs);
    q16::Chasing_Spirals_Q16_Batch4_ColorGrouped(ctx);
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

FL_TEST_CASE("Animartrix2 - RGB_BLOBS5") {
    fl::cout << "DEBUG: Running RGB_BLOBS5 test..." << fl::endl;
    testAnimation(0, "RGB_BLOBS5");
    fl::cout << "DEBUG: RGB_BLOBS5 test completed!" << fl::endl;
}
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
FL_TEST_CASE("Animartrix2 - CHASING_SPIRALS_1x1_DEBUG") {
    fl::cout << "\n=== Testing CHASING_SPIRALS 1x1 DEBUG ===" << fl::endl;

    // Use 1x1 grid for simple debugging
    const uint16_t W_DEBUG = 1;
    const uint16_t H_DEBUG = 1;
    const uint16_t N_DEBUG = 1;

    XYMap xy_scalar = XYMap::constructRectangularGrid(W_DEBUG, H_DEBUG);
    XYMap xy_simd = XYMap::constructRectangularGrid(W_DEBUG, H_DEBUG);

    CRGB leds_scalar[N_DEBUG] = {};
    CRGB leds_simd[N_DEBUG] = {};

    // Render using both paths
    Context ctx_scalar;
    ctx_scalar.leds = leds_scalar;
    ctx_scalar.xyMapFn = [](u16 x, u16 y, void *userData) -> u16 {
        XYMap *map = static_cast<XYMap*>(userData);
        return map->mapToIndex(x, y);
    };
    ctx_scalar.xyMapUserData = &xy_scalar;
    init(ctx_scalar, W_DEBUG, H_DEBUG);
    setTime(ctx_scalar, 1000);

    Context ctx_simd;
    ctx_simd.leds = leds_simd;
    ctx_simd.xyMapFn = [](u16 x, u16 y, void *userData) -> u16 {
        XYMap *map = static_cast<XYMap*>(userData);
        return map->mapToIndex(x, y);
    };
    ctx_simd.xyMapUserData = &xy_simd;
    init(ctx_simd, W_DEBUG, H_DEBUG);
    setTime(ctx_simd, 1000);

    fl::cout << "Running scalar Q31..." << fl::endl;
    fl::Chasing_Spirals_Q31(ctx_scalar);

    fl::cout << "Running SIMD Q31..." << fl::endl;
    fl::Chasing_Spirals_Q31_SIMD(ctx_simd);

    fl::cout << "\n=== 1x1 Results ===" << fl::endl;
    fl::cout << "Scalar: RGB(" << (int)leds_scalar[0].r << "," << (int)leds_scalar[0].g << "," << (int)leds_scalar[0].b << ")" << fl::endl;
    fl::cout << "SIMD:   RGB(" << (int)leds_simd[0].r << "," << (int)leds_simd[0].g << "," << (int)leds_simd[0].b << ")" << fl::endl;

    int diff = abs(leds_scalar[0].r - leds_simd[0].r) +
               abs(leds_scalar[0].g - leds_simd[0].g) +
               abs(leds_scalar[0].b - leds_simd[0].b);
    fl::cout << "Total difference: " << diff << fl::endl;

    if (diff <= 3) {
        fl::cout << "✓ PASS: 1x1 test within tolerance" << fl::endl;
    } else {
        fl::cout << "✗ FAIL: 1x1 test exceeded tolerance (diff=" << diff << ")" << fl::endl;
    }
}

FL_TEST_CASE("Animartrix2 - CHASING_SPIRALS") {
    // A/B comparison: float (Animartrix v1) vs Q31 and Q31_SIMD (Animartrix2).
    // Both fixed-point variants must be within ±1 LSB of the float reference.
    XYMap xy = XYMap::constructRectangularGrid(W, H);
    Animartrix fx_float(xy, CHASING_SPIRALS);
    CRGB leds_float[N] = {};
    CRGB leds_q31[N] = {};
    CRGB leds_q31_simd[N] = {};

    Fx::DrawContext ctx_float(1000, leds_float);
    fx_float.draw(ctx_float);
    renderChasingSpiralQ31(leds_q31, 1000);
    renderChasingSpiralQ31_SIMD(leds_q31_simd, 1000);

    int mismatches_q31  = compareLeds(leds_float, leds_q31,      N, "CHASING_SPIRALS_Q31");
    int mismatches_simd = compareLeds(leds_float, leds_q31_simd, N, "CHASING_SPIRALS_Q31_SIMD");

    FL_MESSAGE("Q31 (non-SIMD): ", mismatches_q31,  " pixels outside ±1 LSB / ", N);
    FL_MESSAGE("Q31_SIMD:       ", mismatches_simd, " pixels outside ±1 LSB / ", N);

    FL_CHECK_MESSAGE(mismatches_q31  == 0, "Q31 must be within ±1 LSB of float at t=1000");
    FL_CHECK_MESSAGE(mismatches_simd == 0, "Q31_SIMD must be within ±1 LSB of float at t=1000");
}

FL_TEST_CASE("Animartrix2 - CHASING_SPIRALS_SIMD accuracy at multiple times") {
    // Verify Q31_SIMD maintains low error vs float across low and high time values.
    // Since both float and Q31 now apply fmodf(offset_x, 2560) period reduction,
    // float32 precision is maintained at all time values including extreme uptimes.
    // Previous max_err at t=2B was 10; after the fix it is ≤ 1.
    const uint32_t times[] = {
        1000,
        1000000,
        100000000,
        2000000000,
    };
    const float avg_thresholds[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    const int   max_thresholds[] = {    6,    3,    3,    3 };

    for (int ti = 0; ti < 4; ti++) {
        uint32_t t = times[ti];
        CRGB leds_float[N] = {};
        CRGB leds_simd[N] = {};

        renderChasingSpiralFloat(leds_float, t);
        renderChasingSpiralQ31_SIMD(leds_simd, t);

        float avg_err = computeAvgError(leds_float, leds_simd, N);
        int   max_err = computeMaxError(leds_float, leds_simd, N);
        float error_pct = avg_err / 255.0f * 100.0f;

        FL_MESSAGE("Q31_SIMD t=", t, ": avg_err=", avg_err,
                   " max_err=", max_err, " error_pct=", error_pct, "%");

        FL_CHECK_MESSAGE(error_pct < avg_thresholds[ti],
                         "Q31_SIMD avg error exceeded threshold");
        FL_CHECK_MESSAGE(max_err <= max_thresholds[ti],
                         "Q31_SIMD max error exceeded threshold");
    }
}
FL_TEST_CASE("Animartrix2 - CHASING_SPIRALS_FLOAT bit-perfect") {
    // Verify Chasing_Spirals_Float (animartrix2 v2) is bit-perfect with
    // Chasing_Spirals (animartrix v1) at all times, including across the
    // first period-reduction wrap (channel-0 wraps at t=2,560,000 ms).
    // Both v1 and v2 apply the same fmodf(offset_x, 2560) fix, so they
    // must produce identical output at all time values.
    const uint32_t test_times[] = {
        1000,
        5000,
        100000,
        1000000,
        2560000,   // channel-0 wrap boundary
        2560001,   // just past the wrap
        5120000,   // channel-0 second wrap
    };

    for (uint32_t t : test_times) {
        CRGB leds_v1[N] = {};
        CRGB leds_v2[N] = {};

        renderChasingSpiralFloat(leds_v1, t);
        renderChasingSpiralFloat_v2(leds_v2, t);

        int mismatches = 0;
        for (uint16_t i = 0; i < N; i++) {
            if (leds_v1[i] != leds_v2[i]) {
                if (mismatches < 5) {
                    FL_MESSAGE("  t=", t, " pixel[", i, "]: v1=(",
                            int(leds_v1[i].r), ",", int(leds_v1[i].g), ",", int(leds_v1[i].b),
                            ") v2=(", int(leds_v2[i].r), ",", int(leds_v2[i].g), ",", int(leds_v2[i].b), ")");
                }
                mismatches++;
            }
        }
        FL_MESSAGE("t=", t, ": ", mismatches, " mismatched pixels / ", N);
        FL_CHECK_MESSAGE(mismatches == 0,
                      "Chasing_Spirals_Float (v2) must be bit-perfect with v1 float");
    }
}

FL_TEST_CASE("Chasing_Spirals - period reduction: no jump at wrap boundary") {
    // The fmodf(offset_x, kPerlinPeriod) reduction is mathematically equivalent to
    // the unreduced value because Perlin noise is periodic with period 256 at
    // integer coordinates, and scale_x=0.1 gives an effective period of
    // 256/0.1 = 2560 for offset_x. This means:
    //   render_value(offset_x) == render_value(offset_x + 2560)  (exactly)
    //
    // C0 continuity at the wrap: Perlin noise is always 0 at exact integer
    // lattice points, so both sides of the wrap → 0 as delta_t → 0.
    //
    // Channel-0 wrap: move.linear[0] = t * 0.01 * 0.1 = t * 0.001
    //   → wraps at t = 2560 / 0.001 = 2,560,000 ms (~43 min)
    // Channel-1 wrap: t = 2560 / (0.01*0.13) ≈ 1,969,231 ms (~33 min)
    // Channel-2 wrap: t = 2560 / (0.01*0.16) = 1,600,000 ms (~27 min)
    //
    // This test verifies that no pixel jump larger than a normal animation step
    // occurs at the wrap boundary.

    constexpr uint32_t dt = 50;  // 50 ms step on each side of wrap

    // -- Channel 0 wrap (t = 2,560,000) --
    {
        constexpr uint32_t t_wrap = 2560000;
        CRGB leds_pre[N] = {}, leds_post[N] = {};
        renderChasingSpiralFloat(leds_pre,  t_wrap - dt);
        renderChasingSpiralFloat(leds_post, t_wrap + dt);
        int max_jump_wrap = computeMaxError(leds_pre, leds_post, N);

        // Reference: same 2*dt window at a non-wrap point
        CRGB leds_ref_a[N] = {}, leds_ref_b[N] = {};
        renderChasingSpiralFloat(leds_ref_a, 500000);
        renderChasingSpiralFloat(leds_ref_b, 500000 + 2 * dt);
        int max_jump_ref = computeMaxError(leds_ref_a, leds_ref_b, N);

        FL_MESSAGE("Ch0 wrap (2*dt=", 2*dt, "ms): jump=", max_jump_wrap,
                   " ref=", max_jump_ref);
        FL_CHECK_MESSAGE(max_jump_wrap <= max_jump_ref + 5,
                         "Channel-0 wrap produces no larger jump than a normal step");
    }

    // -- Channel 1 wrap (t ≈ 1,969,232) --
    {
        constexpr uint32_t t_wrap = 1969232;
        CRGB leds_pre[N] = {}, leds_post[N] = {};
        renderChasingSpiralFloat(leds_pre,  t_wrap - dt);
        renderChasingSpiralFloat(leds_post, t_wrap + dt);
        int max_jump_wrap = computeMaxError(leds_pre, leds_post, N);

        CRGB leds_ref_a[N] = {}, leds_ref_b[N] = {};
        renderChasingSpiralFloat(leds_ref_a, 500000);
        renderChasingSpiralFloat(leds_ref_b, 500000 + 2 * dt);
        int max_jump_ref = computeMaxError(leds_ref_a, leds_ref_b, N);

        FL_MESSAGE("Ch1 wrap (2*dt=", 2*dt, "ms): jump=", max_jump_wrap,
                   " ref=", max_jump_ref);
        FL_CHECK_MESSAGE(max_jump_wrap <= max_jump_ref + 5,
                         "Channel-1 wrap produces no larger jump than a normal step");
    }

    // -- Channel 2 wrap (t = 1,600,000) --
    {
        constexpr uint32_t t_wrap = 1600000;
        CRGB leds_pre[N] = {}, leds_post[N] = {};
        renderChasingSpiralFloat(leds_pre,  t_wrap - dt);
        renderChasingSpiralFloat(leds_post, t_wrap + dt);
        int max_jump_wrap = computeMaxError(leds_pre, leds_post, N);

        CRGB leds_ref_a[N] = {}, leds_ref_b[N] = {};
        renderChasingSpiralFloat(leds_ref_a, 500000);
        renderChasingSpiralFloat(leds_ref_b, 500000 + 2 * dt);
        int max_jump_ref = computeMaxError(leds_ref_a, leds_ref_b, N);

        FL_MESSAGE("Ch2 wrap (2*dt=", 2*dt, "ms): jump=", max_jump_wrap,
                   " ref=", max_jump_ref);
        FL_CHECK_MESSAGE(max_jump_wrap <= max_jump_ref + 5,
                         "Channel-2 wrap produces no larger jump than a normal step");
    }
}

FL_TEST_CASE("Chasing_Spirals - period reduction: C0 continuity as delta_t -> 0") {
    // At the wrap boundary, both sides must converge to the same value as dt→0.
    // Perlin noise is zero at all integer lattice coordinates, so:
    //   - Just before wrap: offset_x → 2560 → x_perlin → 256 → Perlin → 0
    //   - Just after  wrap: offset_x →    0 → x_perlin →   0 → Perlin → 0
    // Both sides of the boundary produce Perlin output approaching 0,
    // guaranteeing C0 continuity (no value discontinuity).
    //
    // This test verifies that at successively finer time steps around the
    // channel-0 wrap, the pixel delta gets smaller (proves C0, not just C1).

    constexpr uint32_t t_wrap = 2560000;  // channel-0 wrap point

    uint32_t dts[] = {1000, 100, 10, 1};
    int prev_max = fl::numeric_limits<int32_t>::max();

    for (uint32_t dt : dts) {
        CRGB leds_pre[N] = {}, leds_post[N] = {};
        renderChasingSpiralFloat(leds_pre,  t_wrap - dt);
        renderChasingSpiralFloat(leds_post, t_wrap + dt);
        int max_jump = computeMaxError(leds_pre, leds_post, N);

        FL_MESSAGE("C0 continuity at wrap: dt=", dt, " ms, max_jump=", max_jump);

        // Smaller dt should produce smaller (or equal) max pixel change
        FL_CHECK_MESSAGE(max_jump <= prev_max,
                         "Pixel delta shrinks as dt approaches 0 at wrap boundary");
        prev_max = max_jump;
    }

    // At dt=1 (the minimum), the jump should be very small
    FL_CHECK_MESSAGE(prev_max <= 5, "Near zero dt: minimal pixel change at wrap boundary");
}

FL_TEST_CASE("Chasing_Spirals - period reduction: float32 precision fix at t=2B") {
    // Without fmodf(offset_x, 2560): float32 loses per-pixel coordinate
    // precision when offset_x grows large. At t=2B:
    //   offset_x ≈ 2,000,000  →  x_perlin ≈ 200,000 + small_per_pixel_term
    //   float32 ULP at 200,000 ≈ 0.024, coarser than per-pixel step (0.1)
    //   → coordinates quantized coarsely → Q31 and float disagree: max_err=10
    //
    // With fmodf(offset_x, 2560): offset_x ∈ [0, 2560)
    //   x_perlin ∈ [0, 256) + small_per_pixel_term
    //   float32 ULP at 256 ≈ 3e-5, much finer than per-pixel step
    //   → float and Q31 agree within ±1 LSB at all time values.
    const uint32_t times[] = {
        1000,
        1000000,
        100000000,
        2000000000,  // ~23 days
    };

    for (uint32_t t : times) {
        CRGB leds_float[N] = {}, leds_q31[N] = {};
        renderChasingSpiralFloat(leds_float, t);
        renderChasingSpiralQ31(leds_q31, t);

        int max_err = computeMaxError(leds_float, leds_q31, N);
        FL_MESSAGE("t=", t, ": float vs Q31 max_err=", max_err);

        FL_CHECK_MESSAGE(max_err <= 3,
                         "Period reduction keeps float and Q31 in agreement at all uptimes");
    }
}

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
    fl::cout << "DEBUG: Running API compatibility test..." << fl::endl;
    const uint16_t W = 32;
    const uint16_t H = 32;
    XYMap xy = XYMap::constructRectangularGrid(W, H);
    fl::cout << "DEBUG: XYMap created for API test" << fl::endl;

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

// ============================================================
// Chasing_Spirals Q31 Optimization Tests
// ============================================================

FL_TEST_CASE("Animartrix2 - CHASING_SPIRALS Single Pixel Debug") {
    fl::cout << "\n=== Single Pixel Debug Trace (t=1000, pixel 0) ===" << fl::endl;

    constexpr uint32_t TEST_TIME = 1000;

    // Render all three versions
    CRGB leds_scalar[N] = {};
    CRGB leds_simd[N] = {};
    CRGB leds_float[N] = {};

    fl::cout << "\n--- Float (Reference) ---" << fl::endl;
    XYMap xy_float = XYMap::constructRectangularGrid(W, H);
    Animartrix fx_float(xy_float, CHASING_SPIRALS);
    Fx::DrawContext ctx_float(TEST_TIME, leds_float);
    fx_float.draw(ctx_float);

    fl::cout << "\n--- Scalar Q31 (Non-SIMD) ---" << fl::endl;
    renderChasingSpiralQ31(leds_scalar, TEST_TIME);

    fl::cout << "\n--- SIMD Q31 ---" << fl::endl;
    renderChasingSpiralQ31_SIMD(leds_simd, TEST_TIME);

    // Compare pixel 0
    fl::cout << "\n=== Pixel 0 Comparison ===" << fl::endl;
    fl::cout << "Float:  RGB(" << (int)leds_float[0].r << "," << (int)leds_float[0].g << "," << (int)leds_float[0].b << ")" << fl::endl;
    fl::cout << "Scalar: RGB(" << (int)leds_scalar[0].r << "," << (int)leds_scalar[0].g << "," << (int)leds_scalar[0].b << ")" << fl::endl;
    fl::cout << "SIMD:   RGB(" << (int)leds_simd[0].r << "," << (int)leds_simd[0].g << "," << (int)leds_simd[0].b << ")" << fl::endl;

    int diff_scalar = abs(leds_float[0].r - leds_scalar[0].r) +
                      abs(leds_float[0].g - leds_scalar[0].g) +
                      abs(leds_float[0].b - leds_scalar[0].b);
    int diff_simd = abs(leds_float[0].r - leds_simd[0].r) +
                    abs(leds_float[0].g - leds_simd[0].g) +
                    abs(leds_float[0].b - leds_simd[0].b);

    fl::cout << "Scalar error: " << diff_scalar << fl::endl;
    fl::cout << "SIMD error: " << diff_simd << fl::endl;

    // Debug output will appear above showing intermediate values
}

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
    // Test multiple high time values to verify stability.
    // After the period reduction fix (fmodf(offset_x, 2560)) applied to both
    // float and Q31 paths, max_err is now ≤ 1 at all tested time values
    // including t=2B (was max_err=10 before the fix).
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

        FL_CHECK_MESSAGE(error_pct < 1.0f,
                      "Q31 Chasing_Spirals average error should be < 1% at high time values");
        FL_CHECK_MESSAGE(max_err <= 3,
                      "Q31 Chasing_Spirals max error should be <= 3 at high time values");
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
