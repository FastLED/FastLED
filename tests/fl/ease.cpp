#include "fl/ease.h"
#include "fl/stl/pair.h"
#include "lib8tion/intmap.h"
#include "test.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"
#include "test.h"
#include <cstdlib>  // ok include - for abs()
#include <cmath>  // ok include - for math functions
using namespace fl;
// Common array of easing types with names used across multiple test cases
static const fl::pair<fl::EaseType, const char*> ALL_EASING_TYPES[10] = {
    {fl::EASE_NONE, "EASE_NONE"},
    {fl::EASE_IN_QUAD, "EASE_IN_QUAD"},
    {fl::EASE_OUT_QUAD, "EASE_OUT_QUAD"},
    {fl::EASE_IN_OUT_QUAD, "EASE_IN_OUT_QUAD"},
    {fl::EASE_IN_CUBIC, "EASE_IN_CUBIC"},
    {fl::EASE_OUT_CUBIC, "EASE_OUT_CUBIC"},
    {fl::EASE_IN_OUT_CUBIC, "EASE_IN_OUT_CUBIC"},
    {fl::EASE_IN_SINE, "EASE_IN_SINE"},
    {fl::EASE_OUT_SINE, "EASE_OUT_SINE"},
    {fl::EASE_IN_OUT_SINE, "EASE_IN_OUT_SINE"}
};
static const size_t NUM_EASING_TYPES = sizeof(ALL_EASING_TYPES) / sizeof(ALL_EASING_TYPES[0]);

FL_TEST_CASE("8-bit easing functions") {
    FL_SUBCASE("easeInOutQuad8") {
        FL_SUBCASE("boundary values") {
            FL_CHECK_CLOSE(easeInOutQuad8(0), 0, 1);
            FL_CHECK_CLOSE(easeInOutQuad8(255), 255, 1);
            FL_CHECK_CLOSE(easeInOutQuad8(128), 128,
                        1); // midpoint should be unchanged
        }

        FL_SUBCASE("symmetry") {
            // ease-in-out should be symmetric around midpoint
            for (uint8_t i = 0; i < 128; ++i) {
                uint8_t forward = easeInOutQuad8(i);
                uint8_t backward = easeInOutQuad8(255 - i);
                FL_CHECK_CLOSE(forward, 255 - backward, 1);
            }
        }



        FL_SUBCASE("first quarter should be slower than linear") {
            // ease-in portion should start slower than linear
            uint8_t quarter = easeInOutQuad8(64); // 64 = 255/4
            FL_CHECK_LT(quarter, 64); // should be less than linear progression
        }
    }

    FL_SUBCASE("easeInOutCubic8") {
        FL_SUBCASE("boundary values") {
            FL_CHECK_CLOSE(easeInOutCubic8(0), 0, 1);
            FL_CHECK_CLOSE(easeInOutCubic8(255), 255, 1);
            FL_CHECK_CLOSE(easeInOutCubic8(128), 128, 1);
        }

        FL_SUBCASE("symmetry") {
            const int kTolerance = 2; // This is too high, come back to this.
            for (uint8_t i = 0; i < 128; ++i) {
                uint8_t forward = easeInOutCubic8(i);
                uint8_t backward = easeInOutCubic8(255 - i);
                FL_CHECK_CLOSE(forward, 255 - backward, kTolerance);
            }
        }



        FL_SUBCASE("more pronounced than quadratic") {
            // cubic should be more pronounced than quadratic in ease-in portion
            uint8_t quarter = 64;
            uint8_t quad_result = easeInOutQuad8(quarter);
            uint8_t cubic_result = easeInOutCubic8(quarter);
            FL_CHECK_LT(cubic_result, quad_result);
        }
    }
}

FL_TEST_CASE("easing function special values") {

    FL_SUBCASE("quarter points") {
        // Test specific values that are common in animations
        // 16-bit quarter points
        FL_CHECK_LT(easeInOutQuad16(16384), 16384);
        FL_CHECK_GT(easeInOutQuad16(49152), 49152);

        FL_CHECK_LT(easeInOutCubic16(16384), easeInOutQuad16(16384));
        FL_CHECK_GT(easeInOutCubic16(49152), easeInOutQuad16(49152));
    }
}

FL_TEST_CASE("easeInOutQuad16") {
    FL_SUBCASE("boundary values") {
        FL_CHECK_EQ(easeInOutQuad16(0), 0);
        FL_CHECK_EQ(easeInOutQuad16(65535), 65535);
        FL_CHECK_EQ(easeInOutQuad16(32768), 32768); // midpoint

        // Test values very close to boundaries
        FL_CHECK_EQ(easeInOutQuad16(1), 0);
        FL_CHECK_EQ(easeInOutQuad16(65534), 65535);

        // Test edge cases around midpoint
        FL_CHECK_EQ(easeInOutQuad16(32767), 32767);
        FL_CHECK_EQ(easeInOutQuad16(32769), 32770);
    }

    FL_SUBCASE("quartile values") {
        // Test specific quartile values for 16-bit quadratic easing
        FL_CHECK_EQ(easeInOutQuad16(16384), 8192); // 25% input -> 12.5% output
        FL_CHECK_EQ(easeInOutQuad16(32768),
                 32768); // 50% input -> 50% output (midpoint)
        FL_CHECK_EQ(easeInOutQuad16(49152),
                 57344); // 75% input -> actual measured output

        // Additional quartile boundary checks
        FL_CHECK_LT(easeInOutQuad16(16384),
                 16384); // ease-in should be slower than linear
        FL_CHECK_GT(easeInOutQuad16(49152),
                 49152); // ease-out should be faster than linear
    }

    FL_SUBCASE("symmetry") {
        for (uint16_t i = 0; i < 32768; i += 256) {
            uint16_t forward = easeInOutQuad16(i);
            uint16_t backward = easeInOutQuad16(65535 - i);
            FL_CHECK_EQ(forward, 65535 - backward);
        }
    }



    FL_SUBCASE("scaling consistency with 8-bit") {
        const int kTolerance = 2; // Note that this is too high.
        // 16-bit version should be consistent with 8-bit when scaled
        for (uint16_t i = 0; i <= 255; ++i) {
            uint8_t input8 = i;
            uint16_t input16 = map8_to_16(input8);

            uint8_t result8 = easeInOutQuad8(input8);
            uint16_t result16 = easeInOutQuad16(input16);
            uint8_t scaled_result16 = map16_to_8(result16);

            // Should be within 1 due to precision differences
            int16_t diff =
                abs((int16_t)result8 - (int16_t)scaled_result16);
            FL_CHECK_LE(diff, kTolerance);
        }
    }
}

FL_TEST_CASE("easeInOutCubic16") {

    FL_SUBCASE("boundary values") {
        FL_CHECK_EQ(easeInOutCubic16(0), 0);
        FL_CHECK_EQ(easeInOutCubic16(65535), 65535);
        FL_CHECK_EQ(easeInOutCubic16(32768), 32769);
    }

    FL_SUBCASE("quartile values") {
        FL_CHECK_EQ(easeInOutCubic16(16384), 4096);
        FL_CHECK_EQ(easeInOutCubic16(32768), 32769);
        FL_CHECK_EQ(easeInOutCubic16(49152), 61440);
    }

    FL_SUBCASE("symmetry") {
        const int kTolerance = 2; // Note that this is too high.
        for (uint16_t i = 0; i < 32768; i += 256) {
            uint16_t forward = easeInOutCubic16(i);
            uint16_t backward = easeInOutCubic16(65535 - i);
            // FL_CHECK_EQ(forward, 65535 - backward);
            FL_CHECK_CLOSE(forward, 65535 - backward, kTolerance);
        }
    }



    FL_SUBCASE("more pronounced than quadratic") {
        uint16_t quarter = 16384;
        uint16_t quad_result = easeInOutQuad16(quarter);
        uint16_t cubic_result = easeInOutCubic16(quarter);
        FL_CHECK_LT(cubic_result, quad_result);
    }

    FL_SUBCASE("scaling consistency with 8-bit") {
        for (uint16_t i = 0; i <= 255; ++i) {
            uint8_t input8 = i;
            uint16_t input16 = map8_to_16(input8);

            uint8_t result8 = easeInOutCubic8(input8);
            uint16_t result16 = easeInOutCubic16(input16);
            uint8_t scaled_result16 = map16_to_8(result16);

            // Should be within 2 due to precision differences in cubic
            // calculation
            int16_t diff =
                abs((int16_t)result8 - (int16_t)scaled_result16);
            FL_CHECK_LE(diff, 2);
        }
    }
}

FL_TEST_CASE("easing function ordering") {

    FL_SUBCASE("8-bit: cubic should be more pronounced than quadratic") {
        for (uint16_t i = 32; i < 128; i += 16) { // test ease-in portion
            uint8_t quad = easeInOutQuad8(i);
            uint8_t cubic = easeInOutCubic8(i);
            FL_CHECK_LE(cubic, quad);
        }

        for (uint16_t i = 128; i < 224; i += 16) { // test ease-out portion
            uint8_t quad = easeInOutQuad8(i);
            uint8_t cubic = easeInOutCubic8(i);
            FL_CHECK_GE(cubic, quad);
        }
    }

    FL_SUBCASE("16-bit: cubic should be more pronounced than quadratic") {
        for (uint32_t i = 8192; i < 32768; i += 4096) { // test ease-in portion
            uint16_t quad = easeInOutQuad16(i);
            uint16_t cubic = easeInOutCubic16(i);
            FL_CHECK_LE(cubic, quad);
        }

        for (uint32_t i = 32768; i < 57344;
             i += 4096) { // test ease-out portion
            uint16_t quad = easeInOutQuad16(i);
            uint16_t cubic = easeInOutCubic16(i);
            FL_CHECK_GE(cubic, quad);
        }
    }
}

FL_TEST_CASE("easeInQuad16") {
    FL_SUBCASE("boundary values") {
        FL_CHECK_EQ(easeInQuad16(0), 0);
        FL_CHECK_EQ(easeInQuad16(65535), 65535);

        // Test values very close to boundaries
        FL_CHECK_EQ(easeInQuad16(1), 0);         // (1 * 1) / 65535 = 0
        FL_CHECK_EQ(easeInQuad16(65534), 65533); // (65534 * 65534) / 65535 = 65533
    }

    FL_SUBCASE("quartile values") {
        // Test specific quartile values for 16-bit quadratic ease-in
        // Expected values calculated as (input * input) / 65535
        FL_CHECK_EQ(easeInQuad16(16384),
                 4096); // 25% input -> ~6.25% output: (16384^2)/65535 = 4096
        FL_CHECK_EQ(easeInQuad16(32768),
                 16384); // 50% input -> 25% output: (32768^2)/65535 = 16384
        FL_CHECK_EQ(easeInQuad16(49152),
                 36864); // 75% input -> ~56.25% output: (49152^2)/65535 = 36864

        // Additional test points
        FL_CHECK_EQ(easeInQuad16(8192), 1024);   // 12.5% input -> ~1.56% output
        FL_CHECK_EQ(easeInQuad16(57344), 50176); // 87.5% input -> ~76.56% output
    }

    FL_SUBCASE("mathematical precision") {
        // Test specific values where we can verify the exact mathematical
        // result
        FL_CHECK_EQ(easeInQuad16(256), 1);    // (256 * 256) / 65535 = 1
        FL_CHECK_EQ(easeInQuad16(512), 4);    // (512 * 512) / 65535 = 4
        FL_CHECK_EQ(easeInQuad16(1024), 16);  // (1024 * 1024) / 65535 = 16
        FL_CHECK_EQ(easeInQuad16(2048), 64);  // (2048 * 2048) / 65535 = 64
        FL_CHECK_EQ(easeInQuad16(4096), 256); // (4096 * 4096) / 65535 = 256
    }



    FL_SUBCASE("ease-in behavior") {
        // For ease-in, early values should be less than linear progression
        // while later values should be greater than linear progression

        // First quarter should be significantly slower than linear
        uint16_t quarter_linear = 16384; // 25% of 65535
        uint16_t quarter_eased = easeInQuad16(16384);
        FL_CHECK_LT(quarter_eased, quarter_linear);
        FL_CHECK_LT(quarter_eased, quarter_linear / 2); // Should be much slower

        // Third quarter should still be slower than linear for ease-in
        // (ease-in accelerates but doesn't surpass linear until very late)
        uint16_t three_quarter_linear = 49152; // 75% of 65535
        uint16_t three_quarter_eased = easeInQuad16(49152);
        FL_CHECK_LT(three_quarter_eased, three_quarter_linear);

        // The acceleration should be visible in the differences
        uint16_t early_diff = easeInQuad16(8192) - easeInQuad16(0); // 0-12.5%
        uint16_t late_diff =
            easeInQuad16(57344) - easeInQuad16(49152); // 75-87.5%
        FL_CHECK_GT(late_diff,
                 early_diff * 10); // Late differences should be much larger
    }

    FL_SUBCASE("specific known values") {
        // Test some additional specific values for regression testing
        FL_CHECK_EQ(easeInQuad16(65535 / 4), 4095);      // Quarter point
        FL_CHECK_EQ(easeInQuad16(65535 / 2), 16383);     // Half point
        FL_CHECK_EQ(easeInQuad16(65535 * 3 / 4), 36863); // Three-quarter point

        // Edge cases near boundaries
        FL_CHECK_EQ(easeInQuad16(255), 0);       // Small value should round to 0
        FL_CHECK_EQ(easeInQuad16(65280), 65025); // Near-max value
    }
}

FL_TEST_CASE("All easing functions boundary tests") {
    FL_SUBCASE("8-bit easing functions boundary conditions") {
        for (size_t i = 0; i < NUM_EASING_TYPES; ++i) {
            fl::EaseType type = ALL_EASING_TYPES[i].first;
            const char* name = ALL_EASING_TYPES[i].second;
            uint8_t result_0 = fl::ease8(type, 0);
            uint8_t result_255 = fl::ease8(type, 255);
            
            FL_DINFO("Testing EaseType " << name);
            FL_CHECK_EQ(result_0, 0);
            FL_CHECK_EQ(result_255, 255);
        }
    }
    
    FL_SUBCASE("16-bit easing functions boundary conditions") {
        for (size_t i = 0; i < NUM_EASING_TYPES; ++i) {
            fl::EaseType type = ALL_EASING_TYPES[i].first;
            const char* name = ALL_EASING_TYPES[i].second;
            uint16_t result_0 = fl::ease16(type, 0);
            uint16_t result_max = fl::ease16(type, 65535);
            
            FL_DINFO("Testing EaseType " << name);
            FL_CHECK_EQ(result_0, 0);
            FL_CHECK_EQ(result_max, 65535);
        }
    }
}

FL_TEST_CASE("All easing functions monotonicity tests") {
    FL_SUBCASE("8-bit easing functions monotonicity") {
        for (size_t i = 0; i < NUM_EASING_TYPES; ++i) {
            fl::EaseType type = ALL_EASING_TYPES[i].first;
            const char* name = ALL_EASING_TYPES[i].second;
            
            // Function should be non-decreasing
            uint8_t prev = 0;
            for (uint16_t input = 0; input <= 255; ++input) {
                uint8_t current = fl::ease8(type, input);
                FL_DINFO("Testing EaseType " << name << " at input " << input);
                FL_CHECK_GE(current, prev);
                prev = current;
            }
        }
    }
    
    FL_SUBCASE("16-bit easing functions monotonicity") {
        for (size_t i = 0; i < NUM_EASING_TYPES; ++i) {
            fl::EaseType type = ALL_EASING_TYPES[i].first;
            const char* name = ALL_EASING_TYPES[i].second;
            
            // Function should be non-decreasing
            uint16_t prev = 0;
            for (uint32_t input = 0; input <= 65535; input += 256) {
                uint16_t current = fl::ease16(type, input);
                FL_DINFO("Testing EaseType " << name << " at input " << input);
                FL_CHECK_GE(current, prev);
                prev = current;
            }
        }
    }
}

FL_TEST_CASE("All easing functions 8-bit vs 16-bit consistency tests") {
    // Define expected tolerances for different easing types
    const int tolerances[] = {
        1, // EASE_NONE - should be perfect
        2, // EASE_IN_QUAD - basic precision differences
        2, // EASE_OUT_QUAD - basic precision differences  
        2, // EASE_IN_OUT_QUAD - basic precision differences
        3, // EASE_IN_CUBIC - higher tolerance for cubic calculations
        3, // EASE_OUT_CUBIC - higher tolerance for cubic calculations
        3, // EASE_IN_OUT_CUBIC - higher tolerance for cubic calculations
        4, // EASE_IN_SINE - sine functions have more precision loss
        4, // EASE_OUT_SINE - sine functions have more precision loss
        4  // EASE_IN_OUT_SINE - sine functions have more precision loss
    };
    
    FL_SUBCASE("8-bit vs 16-bit scaling consistency") {
        for (size_t type_idx = 0; type_idx < NUM_EASING_TYPES; ++type_idx) {
            fl::EaseType type = ALL_EASING_TYPES[type_idx].first;
            const char* name = ALL_EASING_TYPES[type_idx].second;
            int tolerance = tolerances[type_idx];
            
            // Track maximum difference found for this easing type
            int max_diff = 0;
            uint8_t worst_input = 0;
            
            // Test all 8-bit input values
            for (uint16_t i = 0; i <= 255; ++i) {
                uint8_t input8 = i;
                uint16_t input16 = map8_to_16(input8);
                
                uint8_t result8 = fl::ease8(type, input8);
                uint16_t result16 = fl::ease16(type, input16);
                uint8_t scaled_result16 = map16_to_8(result16);
                
                int16_t diff = abs((int16_t)result8 - (int16_t)scaled_result16);
                
                // Track the worst case for reporting
                if (diff > max_diff) {
                    max_diff = diff;
                    worst_input = input8;
                }
                
                FL_DINFO("Testing EaseType " << name 
                     << " at input " << (int)input8 
                     << " (8-bit result: " << (int)result8
                     << ", 16-bit scaled result: " << (int)scaled_result16
                     << ", diff: " << diff << ")");
                FL_CHECK_LE(diff, tolerance);
            }
            
            // Log the maximum difference found for this easing type
            FL_DINFO("EaseType " << name << " maximum difference: " << max_diff 
                 << " at input " << (int)worst_input);
        }
    }
    
    FL_SUBCASE("Boundary values consistency") {
        for (size_t type_idx = 0; type_idx < NUM_EASING_TYPES; ++type_idx) {
            fl::EaseType type = ALL_EASING_TYPES[type_idx].first;
            const char* name = ALL_EASING_TYPES[type_idx].second;
            
            // Test boundary values (0 and max) - these should be exact
            uint8_t result8_0 = fl::ease8(type, 0);
            uint16_t result16_0 = fl::ease16(type, 0);
            uint8_t scaled_result16_0 = map16_to_8(result16_0);
            
            uint8_t result8_255 = fl::ease8(type, 255);
            uint16_t result16_65535 = fl::ease16(type, 65535);
            uint8_t scaled_result16_255 = map16_to_8(result16_65535);
            
            FL_DINFO("Testing EaseType " << name << " boundary values");
            FL_CHECK_EQ(result8_0, scaled_result16_0);
            FL_CHECK_EQ(result8_255, scaled_result16_255);
            
            // Both should map to exact boundaries
            FL_CHECK_EQ(result8_0, 0);
            FL_CHECK_EQ(result8_255, 255);
            FL_CHECK_EQ(scaled_result16_0, 0);
            FL_CHECK_EQ(scaled_result16_255, 255);
        }
    }
    
        FL_SUBCASE("Midpoint consistency") {
        for (size_t type_idx = 0; type_idx < NUM_EASING_TYPES; ++type_idx) {
            fl::EaseType type = ALL_EASING_TYPES[type_idx].first;
            const char* name = ALL_EASING_TYPES[type_idx].second;
            
            // Test midpoint values - should be relatively close
            uint8_t result8_mid = fl::ease8(type, 128);
            uint16_t result16_mid = fl::ease16(type, 32768);
            uint8_t scaled_result16_mid = map16_to_8(result16_mid);
            
            int16_t diff = abs((int16_t)result8_mid - (int16_t)scaled_result16_mid);
            
            FL_DINFO("Testing EaseType " << name << " midpoint consistency"
                 << " (8-bit: " << (int)result8_mid
                 << ", 16-bit scaled: " << (int)scaled_result16_mid
                 << ", diff: " << diff << ")");
            
            // Use the same tolerance as the general test
            FL_CHECK_LE(diff, tolerances[type_idx]);
        }
    }
}

// --- Gamma8 tests ---
// Gamma8 is declared in fl/ease.h (already included above)

FL_TEST_CASE("Gamma8 - getOrCreate returns same instance for same gamma") {
    auto a = Gamma8::getOrCreate(2.8f);
    auto b = Gamma8::getOrCreate(2.8f);
    FL_CHECK(a.get() == b.get());
}

FL_TEST_CASE("Gamma8 - different gamma returns different instance") {
    auto a = Gamma8::getOrCreate(1.0f);
    auto b = Gamma8::getOrCreate(2.8f);
    FL_CHECK(a.get() != b.get());
}

FL_TEST_CASE("Gamma8 - u8 to u16 span overload") {
    auto g = Gamma8::getOrCreate(2.0f);
    u8 in[] = {0, 128, 255};
    u16 out[3];
    g->convert(fl::span<const u8>(in, 3), fl::span<u16>(out, 3));
    FL_CHECK(out[0] == 0);
    // 128/255 = 0.502, pow(0.502, 2.0) ~ 0.252 -> ~16516
    FL_CHECK(out[1] > 13000);
    FL_CHECK(out[1] < 20000);
    FL_CHECK(out[2] == 65535);
}

FL_TEST_CASE("Gamma8 - u8 to u16 gamma 1.0 is linear scale") {
    auto g = Gamma8::getOrCreate(1.0f);
    u8 in[] = {0, 1, 128, 255};
    u16 out[4];
    g->convert(fl::span<const u8>(in, 4), fl::span<u16>(out, 4));
    FL_CHECK(out[0] == 0);
    // gamma 1.0: lut[i] = round(i/255 * 65535) = round(i * 257.0)
    FL_CHECK(out[1] == 257);
    FL_CHECK_CLOSE(out[2], 32896, 1);
    FL_CHECK(out[3] == 65535);
}

FL_TEST_CASE("Gamma8 - cache expires when all shared_ptrs released") {
    const Gamma8* raw_ptr;
    {
        auto g = Gamma8::getOrCreate(3.5f);
        raw_ptr = g.get();
    }
    // g is out of scope — weak_ptr in cache should be expired.
    // getOrCreate must construct a new instance.
    auto g2 = Gamma8::getOrCreate(3.5f);
    // New instance (old one was destroyed), pointer must differ.
    FL_CHECK(g2.get() != raw_ptr);
}

FL_TEST_CASE("Gamma8 - fixed_point<8,8> convert with lerp interpolation") {
    using FP = fl::ufixed_point<8, 8>;
    auto g = Gamma8::getOrCreate(2.0f);

    // Integer inputs should match u8 LUT exactly
    FP fp_in[] = {FP(0), FP(128), FP(255)};
    FP fp_out[3];
    g->convert(fl::span<const FP>(fp_in, 3), fl::span<FP>(fp_out, 3));

    // Also get u8 results for comparison
    u8 u8_in[] = {0, 128, 255};
    u16 u16_out[3];
    g->convert(fl::span<const u8>(u8_in, 3), fl::span<u16>(u16_out, 3));

    // Integer-valued fixed-point should produce same raw as u8 LUT
    FL_CHECK(fp_out[0].raw() == u16_out[0]);
    FL_CHECK(fp_out[1].raw() == u16_out[1]);
    FL_CHECK(fp_out[2].raw() == u16_out[2]);
}

FL_TEST_CASE("Gamma8 - fixed_point<8,8> lerp interpolates between LUT entries") {
    using FP = fl::ufixed_point<8, 8>;
    auto g = Gamma8::getOrCreate(2.0f);

    // Get LUT values at index 100 and 101 via u8 convert
    u8 u8_in[] = {100, 101};
    u16 u16_out[2];
    g->convert(fl::span<const u8>(u8_in, 2), fl::span<u16>(u16_out, 2));
    u16 lut100 = u16_out[0];
    u16 lut101 = u16_out[1];

    // Midpoint: 100.5 = raw 100*256 + 128 = 25728
    FP mid = FP::from_raw(100 * 256 + 128);
    FP fp_out[1];
    g->convert(fl::span<const FP>(&mid, 1), fl::span<FP>(fp_out, 1));

    // Result should be approximately halfway between lut[100] and lut[101]
    u16 expected_mid = static_cast<u16>(
        lut100 + ((static_cast<i32>(lut101) - static_cast<i32>(lut100)) * 128 >> 8));
    FL_CHECK(fp_out[0].raw() == expected_mid);

    // Sanity: interpolated value is strictly between the two LUT entries
    FL_CHECK(fp_out[0].raw() > lut100);
    FL_CHECK(fp_out[0].raw() < lut101);
}

FL_TEST_CASE("Gamma8 - fixed_point<8,8> to u16 span overload") {
    using FP = fl::ufixed_point<8, 8>;
    auto g = Gamma8::getOrCreate(2.0f);

    // Integer inputs via fixed-point -> u16 should match u8 -> u16 exactly
    FP fp_in[] = {FP(0), FP(128), FP(255)};
    u16 fp_u16_out[3];
    g->convert(fl::span<const FP>(fp_in, 3), fl::span<u16>(fp_u16_out, 3));

    u8 u8_in[] = {0, 128, 255};
    u16 u8_u16_out[3];
    g->convert(fl::span<const u8>(u8_in, 3), fl::span<u16>(u8_u16_out, 3));

    FL_CHECK(fp_u16_out[0] == u8_u16_out[0]);
    FL_CHECK(fp_u16_out[1] == u8_u16_out[1]);
    FL_CHECK(fp_u16_out[2] == u8_u16_out[2]);

    // Fractional input: 100.5 -> u16, should match fixed_point output raw
    FP mid = FP::from_raw(100 * 256 + 128);
    u16 mid_u16_out[1];
    g->convert(fl::span<const FP>(&mid, 1), fl::span<u16>(mid_u16_out, 1));

    FP mid_fp_out[1];
    g->convert(fl::span<const FP>(&mid, 1), fl::span<FP>(mid_fp_out, 1));

    // Both overloads should produce the same lerped value
    FL_CHECK(mid_u16_out[0] == mid_fp_out[0].raw());
}
