// Test suite for blend8 function - validates the fix for GitHub issue #1633
// Tests proper rounding and accurate color interpolation

#include "fl/stl/stdint.h"
#include "doctest.h"
#include "platforms/shared/math8.h"

using namespace fl;

TEST_CASE("blend8_endpoints") {
    // Test that blend8 properly handles endpoint values
    // When amountOfB = 0, result should be a
    CHECK_EQ(blend8(100, 200, 0), 100);
    CHECK_EQ(blend8(0, 255, 0), 0);
    CHECK_EQ(blend8(255, 0, 0), 255);

    // When amountOfB = 255, result should be b (or very close due to rounding)
    // With proper implementation, blend8(a, b, 255) should equal b
    CHECK_EQ(blend8(100, 200, 255), 200);
    CHECK_EQ(blend8(0, 255, 255), 255);
    CHECK_EQ(blend8(255, 0, 255), 0);
}

TEST_CASE("blend8_midpoint") {
    // Test that blend8 properly handles midpoint blending
    // blend8(a, b, 128) should be approximately (a + b) / 2
    // With proper rounding, we expect correct results

    // Simple midpoint cases
    CHECK_EQ(blend8(0, 255, 128), 128);     // Should be exactly halfway
    CHECK_EQ(blend8(0, 100, 128), 50);      // 0 + (100-0)*128/256 + rounding
    CHECK_EQ(blend8(100, 200, 128), 150);   // Should be 150
}

TEST_CASE("blend8_low_value_interpolation") {
    // This is the key test case from issue #1633
    // Old implementation: If A=0, B=1, result is mostly 0 due to truncation
    // New implementation: Should properly interpolate with rounding

    // Test case from issue: A=0, B=1
    // For amountOfB=128: Expected result with rounding: round(0 + (1-0)*128/256) = round(0.5) = 1
    CHECK_EQ(blend8(0, 1, 128), 1);

    // For amountOfB=64: Expected: round(0 + (1-0)*64/256) = round(0.25) = 0
    CHECK_EQ(blend8(0, 1, 64), 0);

    // For amountOfB=192: Expected: round(0 + (1-0)*192/256) = round(0.75) = 1
    CHECK_EQ(blend8(0, 1, 192), 1);

    // More low-value tests
    CHECK_EQ(blend8(0, 2, 128), 1);  // Should be 1
    CHECK_EQ(blend8(0, 4, 128), 2);  // Should be 2
}

TEST_CASE("blend8_rounding_accuracy") {
    // Test that rounding works correctly across the range
    // The +0x80 (or +0x8000 for 16-bit) ensures proper rounding

    // These values should round correctly with the new implementation
    CHECK_EQ(blend8(0, 10, 25), 1);   // 0 + 10*25/256 + rounding ≈ 0.98 + 0.5 = 1
    CHECK_EQ(blend8(0, 10, 26), 1);   // 0 + 10*26/256 + rounding ≈ 1.02 + 0.5 = 2

    // Check that we properly reach the overlay color
    CHECK_EQ(blend8(10, 20, 255), 20);
    CHECK_EQ(blend8(100, 150, 255), 150);
}

TEST_CASE("blend8_full_range") {
    // Test blending across the full range of values
    // Verify that results are within expected bounds

    for (int a = 0; a < 256; a += 51) {  // Test at 0, 51, 102, 153, 204, 255
        for (int b = 0; b < 256; b += 51) {
            for (int m = 0; m < 256; m += 32) {  // Test at intervals
                uint8_t result = blend8(a, b, m);

                // Result should always be between min(a,b) and max(a,b)
                uint8_t min_val = (a < b) ? a : b;
                uint8_t max_val = (a > b) ? a : b;

                // Allow for rounding tolerance (split checks to avoid doctest || restriction)
                bool min_check = (result >= min_val) || (result == (min_val - 1));
                bool max_check = (result <= max_val) || (result == (max_val + 1));
                CHECK(min_check);
                CHECK(max_check);
            }
        }
    }
}

TEST_CASE("blend8_iterative_convergence") {
    // Test that iterative blending converges to the target color
    // This was a problem with the old implementation - successive blends
    // failed to reach overlay color

    uint8_t color = 0;
    uint8_t target = 255;

    // Apply blend multiple times with high blend amount
    for (int i = 0; i < 10; i++) {
        color = blend8(color, target, 200);
    }

    // After 10 iterations with 200/255 blend, should be very close to target
    // With proper rounding, we should reach or be very close to 255
    CHECK(color >= 250);  // Should be at least 250 out of 255
}

TEST_CASE("blend8_symmetry") {
    // Test that blend8 exhibits expected symmetry properties
    // blend8(a, b, m) and blend8(b, a, 255-m) should be similar

    uint8_t result1 = blend8(50, 200, 100);
    uint8_t result2 = blend8(200, 50, 155);  // 255 - 100 = 155

    // Results should be close (within 1 due to rounding)
    int diff = result1 > result2 ? result1 - result2 : result2 - result1;
    CHECK(diff <= 1);
}

TEST_CASE("blend8_no_overflow") {
    // Ensure that blend8 doesn't produce values outside [0, 255]
    // Test extreme cases

    CHECK_EQ(blend8(255, 255, 128), 255);
    CHECK_EQ(blend8(0, 0, 128), 0);
    CHECK_EQ(blend8(255, 0, 128), 127);
    CHECK_EQ(blend8(0, 255, 128), 128);
}

TEST_CASE("blend8_8bit_vs_16bit") {
    // Test both implementations separately to ensure they're both correct
    // This validates that both Option 1 (8-bit) and Option 2 (16-bit) work

    // Test some key values with both implementations
    CHECK_EQ(blend8_8bit(0, 255, 128), 128);
    CHECK_EQ(blend8_16bit(0, 255, 128), 128);

    CHECK_EQ(blend8_8bit(100, 200, 128), 150);
    CHECK_EQ(blend8_16bit(100, 200, 128), 150);

    CHECK_EQ(blend8_8bit(0, 1, 128), 1);
    CHECK_EQ(blend8_16bit(0, 1, 128), 1);

    // The 16-bit version should be slightly more accurate in edge cases
    // but for most cases they should match
}

TEST_CASE("blend8_comparison_values") {
    // Additional test cases to verify correct mathematical behavior

    // Quarter blends
    CHECK_EQ(blend8(0, 255, 64), 64);   // 0 + 255*64/256 + rounding ≈ 64
    CHECK_EQ(blend8(0, 255, 192), 192); // 0 + 255*192/256 + rounding ≈ 192

    // Test with middle values
    CHECK_EQ(blend8(100, 200, 0), 100);
    CHECK_EQ(blend8(100, 200, 64), 125);
    CHECK_EQ(blend8(100, 200, 128), 150);
    CHECK_EQ(blend8(100, 200, 192), 175);
    CHECK_EQ(blend8(100, 200, 255), 200);
}
