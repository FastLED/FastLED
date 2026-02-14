#include "fl/sin32.h"
#include "test.h"
#include "fl/int.h"

using namespace fl;

// Helper function for absolute value of integers
template<typename T>
static T abs_helper(T value) {
    return value < 0 ? -value : value;
}

FL_TEST_CASE("fl::sin32") {
    FL_SUBCASE("zero angle") {
        i32 result = sin32(0);
        FL_CHECK_EQ(result, 0);
    }

    FL_SUBCASE("quarter circle (90 degrees)") {
        // 16777216 / 4 = 4194304 (90 degrees)
        i32 result = sin32(4194304);
        // Should be close to maximum value (32767 * 65536 = 2147418112)
        FL_CHECK(result > 2147000000);
        FL_CHECK(result <= 2147418112);
    }

    FL_SUBCASE("half circle (180 degrees)") {
        // 16777216 / 2 = 8388608 (180 degrees)
        i32 result = sin32(8388608);
        // Should be close to zero
        FL_CHECK(abs_helper(result) < 100000);
    }

    FL_SUBCASE("three quarters circle (270 degrees)") {
        // 16777216 * 3 / 4 = 12582912 (270 degrees)
        i32 result = sin32(12582912);
        // Should be close to minimum value (-2147418112)
        FL_CHECK(result < -2147000000);
        FL_CHECK(result >= -2147418112);
    }

    FL_SUBCASE("full circle (360 degrees)") {
        // 16777216 (360 degrees, same as 0)
        i32 result = sin32(16777216);
        // Should be close to zero (same as 0 degrees)
        FL_CHECK(abs_helper(result) < 100000);
    }

    FL_SUBCASE("small angles") {
        // Small angle should give small positive value
        i32 result = sin32(1000);
        FL_CHECK(result > 0);
        FL_CHECK(result < 1000000);  // Small relative to max value of 2147418112
    }

    FL_SUBCASE("accuracy check at 30 degrees") {
        // 30 degrees = 16777216 / 12 = 1398101
        i32 result = sin32(1398101);
        // sin(30°) ≈ 0.5, so result should be ≈ 0.5 * 2147418112 ≈ 1073709056
        FL_CHECK(result > 1060000000);
        FL_CHECK(result < 1090000000);
    }

    FL_SUBCASE("accuracy check at 45 degrees") {
        // 45 degrees = 16777216 / 8 = 2097152
        i32 result = sin32(2097152);
        // sin(45°) ≈ 0.707, so result should be ≈ 0.707 * 2147418112 ≈ 1518224615
        FL_CHECK(result > 1510000000);
        FL_CHECK(result < 1530000000);
    }

    FL_SUBCASE("accuracy check at 60 degrees") {
        // 60 degrees = 16777216 / 6 = 2796202
        i32 result = sin32(2796202);
        // sin(60°) ≈ 0.866, so result should be ≈ 0.866 * 2147418112 ≈ 1859664086
        FL_CHECK(result > 1850000000);
        FL_CHECK(result < 1870000000);
    }

    FL_SUBCASE("symmetry test") {
        // sin(angle) should equal -sin(angle + 180°)
        u32 angle = 1000000;
        i32 sin_a = sin32(angle);
        i32 sin_a_plus_180 = sin32(angle + 8388608); // +180 degrees
        // Allow small error due to interpolation
        FL_CHECK(abs_helper(sin_a + sin_a_plus_180) < 1000);
    }
}

FL_TEST_CASE("fl::cos32") {
    FL_SUBCASE("zero angle") {
        i32 result = cos32(0);
        // cos(0) = 1, so result should be close to maximum
        FL_CHECK(result > 2147000000);
        FL_CHECK(result <= 2147418112);
    }

    FL_SUBCASE("quarter circle (90 degrees)") {
        // 16777216 / 4 = 4194304 (90 degrees)
        i32 result = cos32(4194304);
        // cos(90°) = 0
        FL_CHECK(abs_helper(result) < 100000);
    }

    FL_SUBCASE("half circle (180 degrees)") {
        // 16777216 / 2 = 8388608 (180 degrees)
        i32 result = cos32(8388608);
        // cos(180°) = -1
        FL_CHECK(result < -2147000000);
        FL_CHECK(result >= -2147418112);
    }

    FL_SUBCASE("three quarters circle (270 degrees)") {
        // 16777216 * 3 / 4 = 12582912 (270 degrees)
        i32 result = cos32(12582912);
        // cos(270°) = 0
        FL_CHECK(abs_helper(result) < 100000);
    }

    FL_SUBCASE("full circle (360 degrees)") {
        // 16777216 (360 degrees, same as 0)
        i32 result = cos32(16777216);
        // Should be close to 1 (same as 0 degrees)
        FL_CHECK(result > 2147000000);
        FL_CHECK(result <= 2147418112);
    }

    FL_SUBCASE("accuracy check at 30 degrees") {
        // 30 degrees = 16777216 / 12 = 1398101
        i32 result = cos32(1398101);
        // cos(30°) ≈ 0.866, so result should be ≈ 0.866 * 2147418112 ≈ 1859664086
        FL_CHECK(result > 1850000000);
        FL_CHECK(result < 1870000000);
    }

    FL_SUBCASE("accuracy check at 45 degrees") {
        // 45 degrees = 16777216 / 8 = 2097152
        i32 result = cos32(2097152);
        // cos(45°) ≈ 0.707, so result should be ≈ 0.707 * 2147418112 ≈ 1518224615
        FL_CHECK(result > 1510000000);
        FL_CHECK(result < 1530000000);
    }

    FL_SUBCASE("accuracy check at 60 degrees") {
        // 60 degrees = 16777216 / 6 = 2796202
        i32 result = cos32(2796202);
        // cos(60°) ≈ 0.5, so result should be ≈ 0.5 * 2147418112 ≈ 1073709056
        FL_CHECK(result > 1060000000);
        FL_CHECK(result < 1090000000);
    }

    FL_SUBCASE("cos-sin relationship") {
        // cos(angle) should equal sin(angle + 90°)
        u32 angle = 1000000;
        i32 cos_a = cos32(angle);
        i32 sin_a_plus_90 = sin32(angle + 4194304); // +90 degrees
        // Allow small error due to interpolation
        FL_CHECK(abs_helper(cos_a - sin_a_plus_90) < 1000);
    }
}

FL_TEST_CASE("fl::sin16lut") {
    FL_SUBCASE("zero angle") {
        i16 result = sin16lut(0);
        FL_CHECK_EQ(result, 0);
    }

    FL_SUBCASE("quarter circle (90 degrees)") {
        // 65536 / 4 = 16384 (90 degrees)
        i16 result = sin16lut(16384);
        // Should be close to maximum value (32767)
        FL_CHECK(result > 32700);
        FL_CHECK(result <= 32767);
    }

    FL_SUBCASE("half circle (180 degrees)") {
        // 65536 / 2 = 32768 (180 degrees)
        i16 result = sin16lut(32768);
        // Should be close to zero
        FL_CHECK(abs_helper(result) < 100);
    }

    FL_SUBCASE("three quarters circle (270 degrees)") {
        // 65536 * 3 / 4 = 49152 (270 degrees)
        i16 result = sin16lut(49152);
        // Should be close to minimum value (-32767)
        FL_CHECK(result < -32700);
        FL_CHECK(result >= -32767);
    }

    FL_SUBCASE("full circle (360 degrees)") {
        // 65536 wraps to 0 (u16 overflow), same as 0 degrees
        // Note: 65536 = 0 in u16
        i16 result = sin16lut(0);  // Equivalent to sin16lut(65536) due to u16 overflow
        // Should be close to zero (same as 0 degrees)
        FL_CHECK(abs_helper(result) < 100);
    }

    FL_SUBCASE("small angles") {
        // Small angle should give small positive value
        i16 result = sin16lut(100);
        FL_CHECK(result > 0);
        FL_CHECK(result < 1000);
    }

    FL_SUBCASE("accuracy check at 30 degrees") {
        // 30 degrees = 65536 / 12 = 5461
        i16 result = sin16lut(5461);
        // sin(30°) ≈ 0.5, so result should be ≈ 0.5 * 32767 ≈ 16383
        FL_CHECK(result > 16200);
        FL_CHECK(result < 16600);
    }

    FL_SUBCASE("accuracy check at 45 degrees") {
        // 45 degrees = 65536 / 8 = 8192
        i16 result = sin16lut(8192);
        // sin(45°) ≈ 0.707, so result should be ≈ 0.707 * 32767 ≈ 23169
        FL_CHECK(result > 23000);
        FL_CHECK(result < 23400);
    }

    FL_SUBCASE("accuracy check at 60 degrees") {
        // 60 degrees = 65536 / 6 = 10922
        i16 result = sin16lut(10922);
        // sin(60°) ≈ 0.866, so result should be ≈ 0.866 * 32767 ≈ 28376
        FL_CHECK(result > 28200);
        FL_CHECK(result < 28600);
    }

    FL_SUBCASE("symmetry test") {
        // sin(angle) should equal -sin(angle + 180°)
        u16 angle = 5000;
        i16 sin_a = sin16lut(angle);
        i16 sin_a_plus_180 = sin16lut(angle + 32768); // +180 degrees
        // Allow small error due to interpolation
        FL_CHECK(abs_helper(sin_a + sin_a_plus_180) < 10);
    }
}

FL_TEST_CASE("fl::cos16lut") {
    FL_SUBCASE("zero angle") {
        i16 result = cos16lut(0);
        // cos(0) = 1, so result should be close to maximum
        FL_CHECK(result > 32700);
        FL_CHECK(result <= 32767);
    }

    FL_SUBCASE("quarter circle (90 degrees)") {
        // 65536 / 4 = 16384 (90 degrees)
        i16 result = cos16lut(16384);
        // cos(90°) = 0
        FL_CHECK(abs_helper(result) < 100);
    }

    FL_SUBCASE("half circle (180 degrees)") {
        // 65536 / 2 = 32768 (180 degrees)
        i16 result = cos16lut(32768);
        // cos(180°) = -1
        FL_CHECK(result < -32700);
        FL_CHECK(result >= -32767);
    }

    FL_SUBCASE("three quarters circle (270 degrees)") {
        // 65536 * 3 / 4 = 49152 (270 degrees)
        i16 result = cos16lut(49152);
        // cos(270°) = 0
        FL_CHECK(abs_helper(result) < 100);
    }

    FL_SUBCASE("full circle (360 degrees)") {
        // 65536 wraps to 0 (u16 overflow), same as 0 degrees
        // Note: 65536 = 0 in u16
        i16 result = cos16lut(0);  // Equivalent to cos16lut(65536) due to u16 overflow
        // Should be close to 1 (same as 0 degrees)
        FL_CHECK(result > 32700);
        FL_CHECK(result <= 32767);
    }

    FL_SUBCASE("accuracy check at 30 degrees") {
        // 30 degrees = 65536 / 12 = 5461
        i16 result = cos16lut(5461);
        // cos(30°) ≈ 0.866, so result should be ≈ 0.866 * 32767 ≈ 28376
        FL_CHECK(result > 28200);
        FL_CHECK(result < 28600);
    }

    FL_SUBCASE("accuracy check at 45 degrees") {
        // 45 degrees = 65536 / 8 = 8192
        i16 result = cos16lut(8192);
        // cos(45°) ≈ 0.707, so result should be ≈ 0.707 * 32767 ≈ 23169
        FL_CHECK(result > 23000);
        FL_CHECK(result < 23400);
    }

    FL_SUBCASE("accuracy check at 60 degrees") {
        // 60 degrees = 65536 / 6 = 10922
        i16 result = cos16lut(10922);
        // cos(60°) ≈ 0.5, so result should be ≈ 0.5 * 32767 ≈ 16383
        FL_CHECK(result > 16200);
        FL_CHECK(result < 16600);
    }

    FL_SUBCASE("cos-sin relationship") {
        // cos(angle) should equal sin(angle + 90°)
        u16 angle = 5000;
        i16 cos_a = cos16lut(angle);
        i16 sin_a_plus_90 = sin16lut(angle + 16384); // +90 degrees
        // Allow small error due to interpolation
        FL_CHECK(abs_helper(cos_a - sin_a_plus_90) < 10);
    }
}

FL_TEST_CASE("fl::sin32 vs sin16lut consistency") {
    FL_SUBCASE("compare at various angles") {
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
            FL_CHECK(abs_helper(result16 - result32_scaled) < 5);
        }
    }
}

FL_TEST_CASE("fl::cos32 vs cos16lut consistency") {
    FL_SUBCASE("compare at various angles") {
        // Compare cos32 and cos16lut at equivalent angles
        // Test a representative sample of angles (avoid u16 overflow)
        for (u16 angle16 = 0; angle16 <= 60000; angle16 += 5000) {
            u32 angle32 = angle16 * 256u;
            i16 result16 = cos16lut(angle16);
            i32 result32 = cos32(angle32);

            // Scale result32 to match result16's range
            i16 result32_scaled = static_cast<i16>(result32 / 65536);

            // Allow some error due to scaling and interpolation
            FL_CHECK(abs_helper(result16 - result32_scaled) < 5);
        }
    }
}

FL_TEST_CASE("fl::sin32 and cos32 pythagorean identity") {
    FL_SUBCASE("sin^2 + cos^2 should approximately equal 1") {
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
            FL_CHECK(sum_squares > 0.99);
            FL_CHECK(sum_squares < 1.01);
        }
    }
}

FL_TEST_CASE("fl::sincos32") {
    FL_SUBCASE("matches separate sin32 and cos32") {
        // sincos32 must produce identical results to calling sin32+cos32 separately
        for (u32 angle = 0; angle < 16777216; angle += 7919) {
            SinCos32 sc = sincos32(angle);
            i32 s = sin32(angle);
            i32 c = cos32(angle);
            FL_CHECK_EQ(sc.sin_val, s);
            FL_CHECK_EQ(sc.cos_val, c);
        }
    }

    FL_SUBCASE("key angles") {
        // 0 degrees: sin=0, cos=max
        SinCos32 sc0 = sincos32(0);
        FL_CHECK_EQ(sc0.sin_val, 0);
        FL_CHECK(sc0.cos_val > 2147000000);

        // 90 degrees (4194304): sin=max, cos=0
        SinCos32 sc90 = sincos32(4194304);
        FL_CHECK(sc90.sin_val > 2147000000);
        FL_CHECK(abs_helper(sc90.cos_val) < 100000);

        // 180 degrees (8388608): sin=0, cos=-max
        SinCos32 sc180 = sincos32(8388608);
        FL_CHECK(abs_helper(sc180.sin_val) < 100000);
        FL_CHECK(sc180.cos_val < -2147000000);

        // 270 degrees (12582912): sin=-max, cos=0
        SinCos32 sc270 = sincos32(12582912);
        FL_CHECK(sc270.sin_val < -2147000000);
        FL_CHECK(abs_helper(sc270.cos_val) < 100000);
    }

    FL_SUBCASE("pythagorean identity") {
        for (u32 angle = 0; angle < 16777216; angle += 100000) {
            SinCos32 sc = sincos32(angle);
            double sn = static_cast<double>(sc.sin_val) / 2147418112.0;
            double cn = static_cast<double>(sc.cos_val) / 2147418112.0;
            double sum = sn * sn + cn * cn;
            FL_CHECK(sum > 0.99);
            FL_CHECK(sum < 1.01);
        }
    }
}

FL_TEST_CASE("fl::sin16lut and cos16lut pythagorean identity") {
    FL_SUBCASE("sin^2 + cos^2 should approximately equal 1") {
        // Test at various angles (avoid u16 overflow)
        for (u16 angle = 0; angle <= 60000; angle += 5000) {
            i16 sin_val = sin16lut(angle);
            i16 cos_val = cos16lut(angle);

            // Convert to normalized float values (divide by max value)
            double sin_norm = static_cast<double>(sin_val) / 32767.0;
            double cos_norm = static_cast<double>(cos_val) / 32767.0;

            double sum_squares = sin_norm * sin_norm + cos_norm * cos_norm;

            // Should be very close to 1.0
            FL_CHECK(sum_squares > 0.99);
            FL_CHECK(sum_squares < 1.01);
        }
    }
}

FL_TEST_CASE("fl::sincos32_simd equivalence") {
    FL_SUBCASE("matches scalar sincos32 with random values") {
        // Test with deterministic "random" values (using LCG for reproducibility)
        u32 seed = 0x12345678;

        for (int test = 0; test < 100; test++) {
            // Generate 4 random angles using simple LCG
            u32 angles[4];
            for (int i = 0; i < 4; i++) {
                seed = seed * 1103515245u + 12345u;
                angles[i] = seed & 0xFFFFFFu;  // Limit to valid angle range [0, 16777216)
            }

            // Load angles into SIMD vector
            simd::simd_u32x4 angles_v = simd::load_u32_4(angles);

            // Call SIMD version
            SinCos32_simd simd_result = sincos32_simd(angles_v);

            // Extract SIMD results
            i32 simd_sins[4], simd_coss[4];
            simd::store_u32_4(reinterpret_cast<u32*>(simd_sins), simd_result.sin_vals); // ok reinterpret cast
            simd::store_u32_4(reinterpret_cast<u32*>(simd_coss), simd_result.cos_vals); // ok reinterpret cast

            // Compare with scalar version for each angle
            for (int i = 0; i < 4; i++) {
                SinCos32 scalar_result = sincos32(angles[i]);

                // SIMD and scalar must match exactly (same algorithm)
                FL_CHECK_EQ(simd_sins[i], scalar_result.sin_val);
                FL_CHECK_EQ(simd_coss[i], scalar_result.cos_val);
            }
        }
    }

    FL_SUBCASE("key angles in SIMD") {
        // Test known angles: 0°, 30°, 45°, 60°, 90°, 180°, 270°, 360°
        u32 angles[4] = {0, 1398101, 2097152, 2796202};  // 0°, 30°, 45°, 60°
        simd::simd_u32x4 angles_v = simd::load_u32_4(angles);
        SinCos32_simd result = sincos32_simd(angles_v);

        i32 sins[4], coss[4];
        simd::store_u32_4(reinterpret_cast<u32*>(sins), result.sin_vals); // ok reinterpret cast
        simd::store_u32_4(reinterpret_cast<u32*>(coss), result.cos_vals); // ok reinterpret cast

        // 0°: sin=0, cos=max
        FL_CHECK_EQ(sins[0], 0);
        FL_CHECK(coss[0] > 2147000000);

        // 30°: sin≈0.5, cos≈0.866
        FL_CHECK(sins[1] > 1060000000);
        FL_CHECK(sins[1] < 1090000000);
        FL_CHECK(coss[1] > 1850000000);
        FL_CHECK(coss[1] < 1870000000);

        // 45°: sin≈0.707, cos≈0.707
        FL_CHECK(sins[2] > 1510000000);
        FL_CHECK(sins[2] < 1530000000);
        FL_CHECK(coss[2] > 1510000000);
        FL_CHECK(coss[2] < 1530000000);

        // 60°: sin≈0.866, cos≈0.5
        FL_CHECK(sins[3] > 1850000000);
        FL_CHECK(sins[3] < 1870000000);
        FL_CHECK(coss[3] > 1060000000);
        FL_CHECK(coss[3] < 1090000000);
    }

    FL_SUBCASE("full angle range sweep") {
        // Test systematic coverage of full angle range
        for (u32 base_angle = 0; base_angle < 16777216; base_angle += 1048576) {  // 16 steps around circle
            u32 angles[4] = {
                base_angle,
                (base_angle + 262144u) & 0xFFFFFFu,   // +15°
                (base_angle + 524288u) & 0xFFFFFFu,   // +30°
                (base_angle + 1048576u) & 0xFFFFFFu   // +60°
            };

            simd::simd_u32x4 angles_v = simd::load_u32_4(angles);
            SinCos32_simd simd_result = sincos32_simd(angles_v);

            i32 simd_sins[4], simd_coss[4];
            simd::store_u32_4(reinterpret_cast<u32*>(simd_sins), simd_result.sin_vals); // ok reinterpret cast
            simd::store_u32_4(reinterpret_cast<u32*>(simd_coss), simd_result.cos_vals); // ok reinterpret cast

            for (int i = 0; i < 4; i++) {
                SinCos32 scalar_result = sincos32(angles[i]);
                FL_CHECK_EQ(simd_sins[i], scalar_result.sin_val);
                FL_CHECK_EQ(simd_coss[i], scalar_result.cos_val);
            }
        }
    }

    FL_SUBCASE("pythagorean identity SIMD") {
        // Test sin^2 + cos^2 = 1 for SIMD version
        u32 seed = 0x87654321;

        for (int test = 0; test < 50; test++) {
            u32 angles[4];
            for (int i = 0; i < 4; i++) {
                seed = seed * 1103515245u + 12345u;
                angles[i] = seed & 0xFFFFFFu;
            }

            simd::simd_u32x4 angles_v = simd::load_u32_4(angles);
            SinCos32_simd result = sincos32_simd(angles_v);

            i32 sins[4], coss[4];
            simd::store_u32_4(reinterpret_cast<u32*>(sins), result.sin_vals); // ok reinterpret cast
            simd::store_u32_4(reinterpret_cast<u32*>(coss), result.cos_vals); // ok reinterpret cast

            for (int i = 0; i < 4; i++) {
                double sn = static_cast<double>(sins[i]) / 2147418112.0;
                double cn = static_cast<double>(coss[i]) / 2147418112.0;
                double sum = sn * sn + cn * cn;
                FL_CHECK(sum > 0.99);
                FL_CHECK(sum < 1.01);
            }
        }
    }
}

FL_TEST_CASE("SinCos32_simd alignment") {
    FL_CHECK_EQ(alignof(SinCos32_simd), 16);
    SinCos32_simd result;
    FL_CHECK_EQ(reinterpret_cast<uintptr_t>(&result) % 16, 0);

    // Verify members are aligned
    FL_CHECK_EQ(reinterpret_cast<uintptr_t>(&result.sin_vals) % 16, 0);
    FL_CHECK_EQ(reinterpret_cast<uintptr_t>(&result.cos_vals) % 16, 0);
}
