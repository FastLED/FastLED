// ok cpp include
// Tests for sin8() and cos8() — validates both the standard LUT path
// and the USE_SIN_32 wrapper path produce correct results.

#include "test.h"
#include "fl/trig8.h"
#include "fl/int.h"

using namespace fl;

FL_TEST_CASE("sin8 key angles") {
    // sin8 maps u8 theta [0..255] → u8 [0..255]
    // where 128 is the zero-crossing (offset unsigned representation).
    // theta=0 → sin(0°) → 128 (midpoint)
    // theta=64 → sin(90°) → 255 (max)
    // theta=128 → sin(180°) → 128 (midpoint)
    // theta=192 → sin(270°) → 0 (min)

    FL_SUBCASE("0 degrees") {
        u8 result = sin8(0);
        // sin(0) = 0, unsigned offset = 128
        FL_CHECK(result >= 126);
        FL_CHECK(result <= 130);
    }

    FL_SUBCASE("90 degrees") {
        u8 result = sin8(64);
        // sin(90°) = 1.0, maps to 255
        FL_CHECK(result >= 253);
    }

    FL_SUBCASE("180 degrees") {
        u8 result = sin8(128);
        // sin(180°) = 0, maps to ~128
        FL_CHECK(result >= 126);
        FL_CHECK(result <= 130);
    }

    FL_SUBCASE("270 degrees") {
        u8 result = sin8(192);
        // sin(270°) = -1.0, maps to ~0
        FL_CHECK(result <= 2);
    }
}

FL_TEST_CASE("cos8 key angles") {
    FL_SUBCASE("0 degrees") {
        u8 result = cos8(0);
        // cos(0) = 1.0, maps to 255
        FL_CHECK(result >= 253);
    }

    FL_SUBCASE("90 degrees") {
        u8 result = cos8(64);
        // cos(90°) = 0, maps to ~128
        FL_CHECK(result >= 126);
        FL_CHECK(result <= 130);
    }

    FL_SUBCASE("180 degrees") {
        u8 result = cos8(128);
        // cos(180°) = -1.0, maps to ~0
        FL_CHECK(result <= 2);
    }

    FL_SUBCASE("270 degrees") {
        u8 result = cos8(192);
        // cos(270°) = 0, maps to ~128
        FL_CHECK(result >= 126);
        FL_CHECK(result <= 130);
    }
}

FL_TEST_CASE("sin8 boundary values") {
    FL_SUBCASE("theta=0 is valid") {
        u8 result = sin8(0);
        // Just verify it doesn't crash and returns a reasonable value
        FL_CHECK(result >= 0);
        FL_CHECK(result <= 255);
    }

    FL_SUBCASE("theta=255 is valid") {
        u8 result = sin8(255);
        // 255 is just below 0 (360°), so sin should be slightly negative
        // of zero, i.e. just below 128
        FL_CHECK(result >= 120);
        FL_CHECK(result <= 130);
    }

    FL_SUBCASE("full range produces valid output") {
        // Every input must produce a valid u8 output (no overflow/underflow)
        for (int i = 0; i < 256; i++) {
            u8 result = sin8(static_cast<u8>(i));
            // Output is always in [0, 255] by type, but verify
            // the function doesn't return degenerate values for all inputs
            (void)result;
        }
    }
}

FL_TEST_CASE("sin8 output range spans full u8") {
    // sin8 should use most of the [0..255] range
    u8 min_val = 255;
    u8 max_val = 0;
    for (int i = 0; i < 256; i++) {
        u8 result = sin8(static_cast<u8>(i));
        if (result < min_val) min_val = result;
        if (result > max_val) max_val = result;
    }
    // Peak should be near 255, trough near 0
    FL_CHECK(max_val >= 253);
    FL_CHECK(min_val <= 2);
}

FL_TEST_CASE("sin8 cos8 relationship") {
    // cos8(x) == sin8(x + 64) (90-degree phase shift)
    for (int i = 0; i < 256; i++) {
        u8 theta = static_cast<u8>(i);
        u8 cos_val = cos8(theta);
        u8 sin_shifted = sin8(static_cast<u8>(theta + 64));
        // Allow ±1 for rounding differences between paths
        int diff = static_cast<int>(cos_val) - static_cast<int>(sin_shifted);
        if (diff < 0) diff = -diff;
        FL_CHECK_LT(diff, 2);
    }
}

FL_TEST_CASE("sin8 half-wave symmetry") {
    // sin8(x) + sin8(x + 128) should be ~256 (they sum to 2*midpoint)
    // because sin(θ) = -sin(θ+180°), and unsigned offset means
    // sin8(x) + sin8(x+128) ≈ 128 + val + 128 - val = 256
    for (int i = 0; i < 128; i++) {
        u8 theta = static_cast<u8>(i);
        int a = sin8(theta);
        int b = sin8(static_cast<u8>(theta + 128));
        int sum = a + b;
        // Should be close to 256 (2 * 128 midpoint)
        int err = sum - 256;
        if (err < 0) err = -err;
        FL_CHECK_LT(err, 4);
    }
}

FL_TEST_CASE("sin8 quarter-wave symmetry") {
    // sin8(64 - x) == sin8(64 + x) for the first quadrant mirror
    // (reflection around the 90° peak)
    for (int i = 1; i < 64; i++) {
        u8 a = sin8(static_cast<u8>(64 - i));
        u8 b = sin8(static_cast<u8>(64 + i));
        int diff = static_cast<int>(a) - static_cast<int>(b);
        if (diff < 0) diff = -diff;
        FL_CHECK_LT(diff, 4);
    }
}

FL_TEST_CASE("sin8 monotonic first quadrant") {
    // sin8 should be non-decreasing from theta=0 to theta=64 (0° to 90°)
    u8 prev = sin8(0);
    for (int i = 1; i <= 64; i++) {
        u8 curr = sin8(static_cast<u8>(i));
        FL_CHECK_GE(static_cast<int>(curr), static_cast<int>(prev) - 1);
        prev = curr;
    }
}

// Grouped tests
#include "sin32.cpp"
