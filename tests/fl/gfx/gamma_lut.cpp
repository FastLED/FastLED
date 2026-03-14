// Unit tests for compile-time PROGMEM gamma LUT generator.

#include "test.h"
#include "fl/gfx/gamma_lut.h"
#include "fl/gfx/ease.h"
#include <cmath>   // ok include - for pow()
#include <cstdio>  // ok include - for printf()

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

FL_TEST_CASE("gamma_lut - constexpr gamma helper") {
    // gamma<u8x24>(2.2f) should return the raw fixed-point value
    constexpr u32 raw = gamma<u8x24>(2.2f);
    // 2.2 * 2^24 = 2.2 * 16777216 = 36909875.2
    FL_CHECK_EQ(raw, u8x24(2.2f).raw());
    FL_CHECK_GT(raw, 0u);
}

FL_TEST_CASE("gamma_lut - GammaEval boundaries") {
    constexpr u32 g22 = gamma<u8x24>(2.2f);
    GammaEval<g22> eval;

    // x=0 must always map to 0
    FL_CHECK_EQ(eval(0), 0);
    // x=255 must always map to 255
    FL_CHECK_EQ(eval(255), 255);
}

FL_TEST_CASE("gamma_lut - GammaEval monotonicity") {
    constexpr u32 g22 = gamma<u8x24>(2.2f);
    GammaEval<g22> eval;

    // Gamma-corrected output must be monotonically non-decreasing
    u8 prev = 0;
    for (int i = 0; i < 256; ++i) {
        u8 val = eval(static_cast<u8>(i));
        FL_CHECK_GE(val, prev);
        prev = val;
    }
}

FL_TEST_CASE("gamma_lut - GammaEval accuracy vs float pow") {
    constexpr u32 g22 = gamma<u8x24>(2.2f);
    GammaEval<g22> eval;

    // Check accuracy against floating-point reference for a few values.
    // Allow +-2 tolerance for fixed-point approximation error.
    for (int i = 1; i < 255; ++i) {
        double expected = ::pow(i / 255.0, 2.2) * 255.0;
        u8 actual = eval(static_cast<u8>(i));
        int diff = static_cast<int>(actual) - static_cast<int>(expected + 0.5);
        if (diff < 0) diff = -diff;
        FL_CHECK_LE(diff, 2);
    }
}

FL_TEST_CASE("gamma_lut - ProgmemLUT read") {
    // Instantiate a 256-entry gamma 2.2 table and verify it can be read.
    typedef ProgmemLUT<GammaEval<gamma<u8x24>(2.2f)>, 256> G22;

    FL_CHECK_EQ(G22::read(0), 0);
    FL_CHECK_EQ(G22::read(255), 255);

    // Midpoint should be less than linear (gamma > 1 darkens midtones)
    FL_CHECK_LT(G22::read(128), 128);
}

FL_TEST_CASE("gamma_lut - Gamma22LUT predefined table") {
    // The predefined Gamma22LUT should be usable directly.
    FL_CHECK_EQ(Gamma22LUT::read(0), 0);
    FL_CHECK_EQ(Gamma22LUT::read(255), 255);
    FL_CHECK_LT(Gamma22LUT::read(128), 128);
}

FL_TEST_CASE("gamma_lut - Gamma28LUT predefined table") {
    FL_CHECK_EQ(Gamma28LUT::read(0), 0);
    FL_CHECK_EQ(Gamma28LUT::read(255), 255);
    // Gamma 2.8 darkens midtones more than 2.2
    FL_CHECK_LT(Gamma28LUT::read(128), Gamma22LUT::read(128));
}

FL_TEST_CASE("gamma_lut - GammaTable256 alias") {
    // GammaTable256 convenience alias should work
    constexpr u32 g18 = gamma<u8x24>(1.8f);
    typedef GammaTable256<g18> G18;

    FL_CHECK_EQ(G18::read(0), 0);
    FL_CHECK_EQ(G18::read(255), 255);
    // Gamma 1.8 is less aggressive than 2.2
    FL_CHECK_GT(G18::read(128), Gamma22LUT::read(128));
}

FL_TEST_CASE("gamma_lut - gamma 1.0 is identity") {
    constexpr u32 g10 = gamma<u8x24>(1.0f);
    typedef ProgmemLUT<GammaEval<g10>, 256> G10;

    // Gamma 1.0 should be (approximately) identity
    for (int i = 0; i < 256; ++i) {
        u8 val = G10::read(static_cast<u8>(i));
        int diff = static_cast<int>(val) - i;
        if (diff < 0) diff = -diff;
        FL_CHECK_LE(diff, 1);
    }
}

// ---- 16-bit LUT tests (for five-bit brightness / HD108 integration) ----

FL_TEST_CASE("gamma_lut - GammaEval16 boundaries") {
    constexpr u32 g28 = gamma<u8x24>(2.8f);
    GammaEval16<g28> eval;

    FL_CHECK_EQ(eval(0), 0);
    FL_CHECK_EQ(eval(255), 65535);
}

FL_TEST_CASE("gamma_lut - GammaEval16 monotonicity") {
    constexpr u32 g28 = gamma<u8x24>(2.8f);
    GammaEval16<g28> eval;

    u16 prev = 0;
    for (int i = 0; i < 256; ++i) {
        u16 val = eval(static_cast<u8>(i));
        FL_CHECK_GE(val, prev);
        prev = val;
    }
}

FL_TEST_CASE("gamma_lut - 8-bit accuracy stats vs float pow") {
    // Compare 8-bit constexpr LUT against continuous double-precision reference.
    // Diff is measured against the unrounded float value to show true error.
    struct GammaTest {
        const char* name;
        float gamma_val;
    };
    const GammaTest tests[] = {
        {"gamma 2.2", 2.2f},
        {"gamma 2.8", 2.8f},
        {"gamma 1.8", 1.8f},
    };

    for (const auto& t : tests) {
        constexpr u32 g22 = gamma<u8x24>(2.2f);
        constexpr u32 g28 = gamma<u8x24>(2.8f);
        constexpr u32 g18 = gamma<u8x24>(1.8f);

        double max_diff = 0;
        int max_diff_idx = 0;
        double sum_diff = 0;

        for (int i = 0; i < 256; ++i) {
            double ref = ::pow(i / 255.0, static_cast<double>(t.gamma_val)) * 255.0;

            int actual;
            if (t.gamma_val == 2.2f) {
                actual = GammaEval<g22>()(static_cast<u8>(i));
            } else if (t.gamma_val == 2.8f) {
                actual = GammaEval<g28>()(static_cast<u8>(i));
            } else {
                actual = GammaEval<g18>()(static_cast<u8>(i));
            }

            double d = actual - ref;
            if (d < 0) d = -d;
            if (d > max_diff) {
                max_diff = d;
                max_diff_idx = i;
            }
            sum_diff += d;
        }

        // 8-bit LUT: max continuous error must be < 1.0
        FL_CHECK_LT(max_diff, 1.0);
        printf("  %s 8-bit:  max_diff=%.3f @i=%d  mean_diff=%.4f\n",
               t.name, max_diff, max_diff_idx, sum_diff / 256.0);
    }
}

FL_TEST_CASE("gamma_lut - 16-bit accuracy stats vs float pow") {
    // Compare 16-bit constexpr LUT against continuous double-precision reference.
    // Diff is measured against the unrounded float value.
    struct GammaTest {
        const char* name;
        float gamma_val;
    };
    const GammaTest tests[] = {
        {"gamma 2.2", 2.2f},
        {"gamma 2.8", 2.8f},
    };

    for (const auto& t : tests) {
        constexpr u32 g22 = gamma<u8x24>(2.2f);
        constexpr u32 g28 = gamma<u8x24>(2.8f);

        double max_diff = 0;
        int max_diff_idx = 0;
        double sum_diff = 0;

        for (int i = 0; i < 256; ++i) {
            double ref = ::pow(i / 255.0, static_cast<double>(t.gamma_val)) * 65535.0;

            int actual;
            if (t.gamma_val == 2.2f) {
                actual = GammaEval16<g22>()(static_cast<u8>(i));
            } else {
                actual = GammaEval16<g28>()(static_cast<u8>(i));
            }

            double d = actual - ref;
            if (d < 0) d = -d;
            if (d > max_diff) {
                max_diff = d;
                max_diff_idx = i;
            }
            sum_diff += d;
        }

        // 16-bit LUT: max continuous error must be < 53
        FL_CHECK_LT(max_diff, 53.0);
        printf("  %s 16-bit: max_diff=%.2f @i=%d  mean_diff=%.2f  max_pct=%.4f%%\n",
               t.name, max_diff, max_diff_idx,
               sum_diff / 256.0,
               max_diff / 65535.0 * 100.0);
    }
}

FL_TEST_CASE("gamma_lut - ProgmemLUT16 read") {
    FL_CHECK_EQ(Gamma28LUT16::read(0), 0);
    FL_CHECK_EQ(Gamma28LUT16::read(255), 65535);
    // Midpoint should be well below linear
    FL_CHECK_LT(Gamma28LUT16::read(128), 32768);
}

FL_TEST_CASE("gamma_lut - GammaTable16_256 alias") {
    constexpr u32 g22 = gamma<u8x24>(2.2f);
    typedef GammaTable16_256<g22> G22_16;

    FL_CHECK_EQ(G22_16::read(0), 0);
    FL_CHECK_EQ(G22_16::read(255), 65535);
}

} // FL_TEST_FILE
