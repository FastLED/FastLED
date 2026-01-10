#include "fl/sin32.h"
#include "doctest.h"
#include "fl/int.h"

using namespace fl;

// Helper function for absolute value of integers
template<typename T>
static T abs_helper(T value) {
    return value < 0 ? -value : value;
}

TEST_CASE("fl::sin32") {
    SUBCASE("zero angle") {
        i32 result = sin32(0);
        CHECK_EQ(result, 0);
    }

    SUBCASE("quarter circle (90 degrees)") {
        // 16777216 / 4 = 4194304 (90 degrees)
        i32 result = sin32(4194304);
        // Should be close to maximum value (32767 * 65536 = 2147418112)
        CHECK(result > 2147000000);
        CHECK(result <= 2147418112);
    }

    SUBCASE("half circle (180 degrees)") {
        // 16777216 / 2 = 8388608 (180 degrees)
        i32 result = sin32(8388608);
        // Should be close to zero
        CHECK(abs_helper(result) < 100000);
    }

    SUBCASE("three quarters circle (270 degrees)") {
        // 16777216 * 3 / 4 = 12582912 (270 degrees)
        i32 result = sin32(12582912);
        // Should be close to minimum value (-2147418112)
        CHECK(result < -2147000000);
        CHECK(result >= -2147418112);
    }

    SUBCASE("full circle (360 degrees)") {
        // 16777216 (360 degrees, same as 0)
        i32 result = sin32(16777216);
        // Should be close to zero (same as 0 degrees)
        CHECK(abs_helper(result) < 100000);
    }

    SUBCASE("small angles") {
        // Small angle should give small positive value
        i32 result = sin32(1000);
        CHECK(result > 0);
        CHECK(result < 1000000);  // Small relative to max value of 2147418112
    }

    SUBCASE("accuracy check at 30 degrees") {
        // 30 degrees = 16777216 / 12 = 1398101
        i32 result = sin32(1398101);
        // sin(30°) ≈ 0.5, so result should be ≈ 0.5 * 2147418112 ≈ 1073709056
        CHECK(result > 1060000000);
        CHECK(result < 1090000000);
    }

    SUBCASE("accuracy check at 45 degrees") {
        // 45 degrees = 16777216 / 8 = 2097152
        i32 result = sin32(2097152);
        // sin(45°) ≈ 0.707, so result should be ≈ 0.707 * 2147418112 ≈ 1518224615
        CHECK(result > 1510000000);
        CHECK(result < 1530000000);
    }

    SUBCASE("accuracy check at 60 degrees") {
        // 60 degrees = 16777216 / 6 = 2796202
        i32 result = sin32(2796202);
        // sin(60°) ≈ 0.866, so result should be ≈ 0.866 * 2147418112 ≈ 1859664086
        CHECK(result > 1850000000);
        CHECK(result < 1870000000);
    }

    SUBCASE("symmetry test") {
        // sin(angle) should equal -sin(angle + 180°)
        u32 angle = 1000000;
        i32 sin_a = sin32(angle);
        i32 sin_a_plus_180 = sin32(angle + 8388608); // +180 degrees
        // Allow small error due to interpolation
        CHECK(abs_helper(sin_a + sin_a_plus_180) < 1000);
    }
}

TEST_CASE("fl::cos32") {
    SUBCASE("zero angle") {
        i32 result = cos32(0);
        // cos(0) = 1, so result should be close to maximum
        CHECK(result > 2147000000);
        CHECK(result <= 2147418112);
    }

    SUBCASE("quarter circle (90 degrees)") {
        // 16777216 / 4 = 4194304 (90 degrees)
        i32 result = cos32(4194304);
        // cos(90°) = 0
        CHECK(abs_helper(result) < 100000);
    }

    SUBCASE("half circle (180 degrees)") {
        // 16777216 / 2 = 8388608 (180 degrees)
        i32 result = cos32(8388608);
        // cos(180°) = -1
        CHECK(result < -2147000000);
        CHECK(result >= -2147418112);
    }

    SUBCASE("three quarters circle (270 degrees)") {
        // 16777216 * 3 / 4 = 12582912 (270 degrees)
        i32 result = cos32(12582912);
        // cos(270°) = 0
        CHECK(abs_helper(result) < 100000);
    }

    SUBCASE("full circle (360 degrees)") {
        // 16777216 (360 degrees, same as 0)
        i32 result = cos32(16777216);
        // Should be close to 1 (same as 0 degrees)
        CHECK(result > 2147000000);
        CHECK(result <= 2147418112);
    }

    SUBCASE("accuracy check at 30 degrees") {
        // 30 degrees = 16777216 / 12 = 1398101
        i32 result = cos32(1398101);
        // cos(30°) ≈ 0.866, so result should be ≈ 0.866 * 2147418112 ≈ 1859664086
        CHECK(result > 1850000000);
        CHECK(result < 1870000000);
    }

    SUBCASE("accuracy check at 45 degrees") {
        // 45 degrees = 16777216 / 8 = 2097152
        i32 result = cos32(2097152);
        // cos(45°) ≈ 0.707, so result should be ≈ 0.707 * 2147418112 ≈ 1518224615
        CHECK(result > 1510000000);
        CHECK(result < 1530000000);
    }

    SUBCASE("accuracy check at 60 degrees") {
        // 60 degrees = 16777216 / 6 = 2796202
        i32 result = cos32(2796202);
        // cos(60°) ≈ 0.5, so result should be ≈ 0.5 * 2147418112 ≈ 1073709056
        CHECK(result > 1060000000);
        CHECK(result < 1090000000);
    }

    SUBCASE("cos-sin relationship") {
        // cos(angle) should equal sin(angle + 90°)
        u32 angle = 1000000;
        i32 cos_a = cos32(angle);
        i32 sin_a_plus_90 = sin32(angle + 4194304); // +90 degrees
        // Allow small error due to interpolation
        CHECK(abs_helper(cos_a - sin_a_plus_90) < 1000);
    }
}

TEST_CASE("fl::sin16lut") {
    SUBCASE("zero angle") {
        i16 result = sin16lut(0);
        CHECK_EQ(result, 0);
    }

    SUBCASE("quarter circle (90 degrees)") {
        // 65536 / 4 = 16384 (90 degrees)
        i16 result = sin16lut(16384);
        // Should be close to maximum value (32767)
        CHECK(result > 32700);
        CHECK(result <= 32767);
    }

    SUBCASE("half circle (180 degrees)") {
        // 65536 / 2 = 32768 (180 degrees)
        i16 result = sin16lut(32768);
        // Should be close to zero
        CHECK(abs_helper(result) < 100);
    }

    SUBCASE("three quarters circle (270 degrees)") {
        // 65536 * 3 / 4 = 49152 (270 degrees)
        i16 result = sin16lut(49152);
        // Should be close to minimum value (-32767)
        CHECK(result < -32700);
        CHECK(result >= -32767);
    }

    SUBCASE("full circle (360 degrees)") {
        // 65536 wraps to 0 (u16 overflow), same as 0 degrees
        // Note: 65536 = 0 in u16
        i16 result = sin16lut(0);  // Equivalent to sin16lut(65536) due to u16 overflow
        // Should be close to zero (same as 0 degrees)
        CHECK(abs_helper(result) < 100);
    }

    SUBCASE("small angles") {
        // Small angle should give small positive value
        i16 result = sin16lut(100);
        CHECK(result > 0);
        CHECK(result < 1000);
    }

    SUBCASE("accuracy check at 30 degrees") {
        // 30 degrees = 65536 / 12 = 5461
        i16 result = sin16lut(5461);
        // sin(30°) ≈ 0.5, so result should be ≈ 0.5 * 32767 ≈ 16383
        CHECK(result > 16200);
        CHECK(result < 16600);
    }

    SUBCASE("accuracy check at 45 degrees") {
        // 45 degrees = 65536 / 8 = 8192
        i16 result = sin16lut(8192);
        // sin(45°) ≈ 0.707, so result should be ≈ 0.707 * 32767 ≈ 23169
        CHECK(result > 23000);
        CHECK(result < 23400);
    }

    SUBCASE("accuracy check at 60 degrees") {
        // 60 degrees = 65536 / 6 = 10922
        i16 result = sin16lut(10922);
        // sin(60°) ≈ 0.866, so result should be ≈ 0.866 * 32767 ≈ 28376
        CHECK(result > 28200);
        CHECK(result < 28600);
    }

    SUBCASE("symmetry test") {
        // sin(angle) should equal -sin(angle + 180°)
        u16 angle = 5000;
        i16 sin_a = sin16lut(angle);
        i16 sin_a_plus_180 = sin16lut(angle + 32768); // +180 degrees
        // Allow small error due to interpolation
        CHECK(abs_helper(sin_a + sin_a_plus_180) < 10);
    }
}

TEST_CASE("fl::cos16lut") {
    SUBCASE("zero angle") {
        i16 result = cos16lut(0);
        // cos(0) = 1, so result should be close to maximum
        CHECK(result > 32700);
        CHECK(result <= 32767);
    }

    SUBCASE("quarter circle (90 degrees)") {
        // 65536 / 4 = 16384 (90 degrees)
        i16 result = cos16lut(16384);
        // cos(90°) = 0
        CHECK(abs_helper(result) < 100);
    }

    SUBCASE("half circle (180 degrees)") {
        // 65536 / 2 = 32768 (180 degrees)
        i16 result = cos16lut(32768);
        // cos(180°) = -1
        CHECK(result < -32700);
        CHECK(result >= -32767);
    }

    SUBCASE("three quarters circle (270 degrees)") {
        // 65536 * 3 / 4 = 49152 (270 degrees)
        i16 result = cos16lut(49152);
        // cos(270°) = 0
        CHECK(abs_helper(result) < 100);
    }

    SUBCASE("full circle (360 degrees)") {
        // 65536 wraps to 0 (u16 overflow), same as 0 degrees
        // Note: 65536 = 0 in u16
        i16 result = cos16lut(0);  // Equivalent to cos16lut(65536) due to u16 overflow
        // Should be close to 1 (same as 0 degrees)
        CHECK(result > 32700);
        CHECK(result <= 32767);
    }

    SUBCASE("accuracy check at 30 degrees") {
        // 30 degrees = 65536 / 12 = 5461
        i16 result = cos16lut(5461);
        // cos(30°) ≈ 0.866, so result should be ≈ 0.866 * 32767 ≈ 28376
        CHECK(result > 28200);
        CHECK(result < 28600);
    }

    SUBCASE("accuracy check at 45 degrees") {
        // 45 degrees = 65536 / 8 = 8192
        i16 result = cos16lut(8192);
        // cos(45°) ≈ 0.707, so result should be ≈ 0.707 * 32767 ≈ 23169
        CHECK(result > 23000);
        CHECK(result < 23400);
    }

    SUBCASE("accuracy check at 60 degrees") {
        // 60 degrees = 65536 / 6 = 10922
        i16 result = cos16lut(10922);
        // cos(60°) ≈ 0.5, so result should be ≈ 0.5 * 32767 ≈ 16383
        CHECK(result > 16200);
        CHECK(result < 16600);
    }

    SUBCASE("cos-sin relationship") {
        // cos(angle) should equal sin(angle + 90°)
        u16 angle = 5000;
        i16 cos_a = cos16lut(angle);
        i16 sin_a_plus_90 = sin16lut(angle + 16384); // +90 degrees
        // Allow small error due to interpolation
        CHECK(abs_helper(cos_a - sin_a_plus_90) < 10);
    }
}

TEST_CASE("fl::sin32 vs sin16lut consistency") {
    SUBCASE("compare at various angles") {
        // Compare sin32 and sin16lut at equivalent angles
        // sin32 uses 0-16777216 for full circle
        // sin16lut uses 0-65536 for full circle
        // Conversion: angle16 * 256 = angle32

        // Test a representative sample of angles (avoid u16 overflow)
        for (u16 angle16 = 0; angle16 <= 60000; angle16 += 5000) {
            u32 angle32 = angle16 * 256u;
            i16 result16 = sin16lut(angle16);
            i32 result32 = sin32(angle32);

            // Scale result32 to match result16's range
            // result32 is in range [-2147418112, 2147418112]
            // result16 is in range [-32767, 32767]
            // Scale factor: 2147418112 / 32767 = 65536
            i16 result32_scaled = static_cast<i16>(result32 / 65536);

            // Allow some error due to scaling and interpolation
            CHECK(abs_helper(result16 - result32_scaled) < 5);
        }
    }
}

TEST_CASE("fl::cos32 vs cos16lut consistency") {
    SUBCASE("compare at various angles") {
        // Compare cos32 and cos16lut at equivalent angles
        // Test a representative sample of angles (avoid u16 overflow)
        for (u16 angle16 = 0; angle16 <= 60000; angle16 += 5000) {
            u32 angle32 = angle16 * 256u;
            i16 result16 = cos16lut(angle16);
            i32 result32 = cos32(angle32);

            // Scale result32 to match result16's range
            i16 result32_scaled = static_cast<i16>(result32 / 65536);

            // Allow some error due to scaling and interpolation
            CHECK(abs_helper(result16 - result32_scaled) < 5);
        }
    }
}

TEST_CASE("fl::sin32 and cos32 pythagorean identity") {
    SUBCASE("sin^2 + cos^2 should approximately equal 1") {
        // Test at various angles
        for (u32 angle = 0; angle < 16777216; angle += 1000000) {
            i32 sin_val = sin32(angle);
            i32 cos_val = cos32(angle);

            // Convert to normalized float values (divide by max value)
            // max value is 2147418112
            double sin_norm = static_cast<double>(sin_val) / 2147418112.0;
            double cos_norm = static_cast<double>(cos_val) / 2147418112.0;

            double sum_squares = sin_norm * sin_norm + cos_norm * cos_norm;

            // Should be very close to 1.0
            CHECK(sum_squares > 0.99);
            CHECK(sum_squares < 1.01);
        }
    }
}

TEST_CASE("fl::sin16lut and cos16lut pythagorean identity") {
    SUBCASE("sin^2 + cos^2 should approximately equal 1") {
        // Test at various angles (avoid u16 overflow)
        for (u16 angle = 0; angle <= 60000; angle += 5000) {
            i16 sin_val = sin16lut(angle);
            i16 cos_val = cos16lut(angle);

            // Convert to normalized float values (divide by max value)
            double sin_norm = static_cast<double>(sin_val) / 32767.0;
            double cos_norm = static_cast<double>(cos_val) / 32767.0;

            double sum_squares = sin_norm * sin_norm + cos_norm * cos_norm;

            // Should be very close to 1.0
            CHECK(sum_squares > 0.99);
            CHECK(sum_squares < 1.01);
        }
    }
}
