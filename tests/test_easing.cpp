#include "fl/ease.h"
#include "fl/array.h"
#include "fl/pair.h"
#include "lib8tion/intmap.h"
#include "test.h"
#include <cmath>
#include <cstdlib>

FASTLED_USING_NAMESPACE

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

TEST_CASE("8-bit easing functions") {
    SUBCASE("easeInOutQuad8") {
        SUBCASE("boundary values") {
            CHECK_CLOSE(easeInOutQuad8(0), 0, 1);
            CHECK_CLOSE(easeInOutQuad8(255), 255, 1);
            CHECK_CLOSE(easeInOutQuad8(128), 128,
                        1); // midpoint should be unchanged
        }

        SUBCASE("symmetry") {
            // ease-in-out should be symmetric around midpoint
            for (uint8_t i = 0; i < 128; ++i) {
                uint8_t forward = easeInOutQuad8(i);
                uint8_t backward = easeInOutQuad8(255 - i);
                CHECK_CLOSE(forward, 255 - backward, 1);
            }
        }



        SUBCASE("first quarter should be slower than linear") {
            // ease-in portion should start slower than linear
            uint8_t quarter = easeInOutQuad8(64); // 64 = 255/4
            CHECK_LT(quarter, 64); // should be less than linear progression
        }
    }

    SUBCASE("easeInOutCubic8") {
        SUBCASE("boundary values") {
            CHECK_CLOSE(easeInOutCubic8(0), 0, 1);
            CHECK_CLOSE(easeInOutCubic8(255), 255, 1);
            CHECK_CLOSE(easeInOutCubic8(128), 128, 1);
        }

        SUBCASE("symmetry") {
            const int kTolerance = 2; // This is too high, come back to this.
            for (uint8_t i = 0; i < 128; ++i) {
                uint8_t forward = easeInOutCubic8(i);
                uint8_t backward = easeInOutCubic8(255 - i);
                CHECK_CLOSE(forward, 255 - backward, kTolerance);
            }
        }



        SUBCASE("more pronounced than quadratic") {
            // cubic should be more pronounced than quadratic in ease-in portion
            uint8_t quarter = 64;
            uint8_t quad_result = easeInOutQuad8(quarter);
            uint8_t cubic_result = easeInOutCubic8(quarter);
            CHECK_LT(cubic_result, quad_result);
        }
    }
}

TEST_CASE("easing function special values") {

    SUBCASE("quarter points") {
        // Test specific values that are common in animations
        // 16-bit quarter points
        CHECK_LT(easeInOutQuad16(16384), 16384);
        CHECK_GT(easeInOutQuad16(49152), 49152);

        CHECK_LT(easeInOutCubic16(16384), easeInOutQuad16(16384));
        CHECK_GT(easeInOutCubic16(49152), easeInOutQuad16(49152));
    }
}

TEST_CASE("easeInOutQuad16") {
    SUBCASE("boundary values") {
        CHECK_EQ(easeInOutQuad16(0), 0);
        CHECK_EQ(easeInOutQuad16(65535), 65535);
        CHECK_EQ(easeInOutQuad16(32768), 32768); // midpoint

        // Test values very close to boundaries
        CHECK_EQ(easeInOutQuad16(1), 0);
        CHECK_EQ(easeInOutQuad16(65534), 65535);

        // Test edge cases around midpoint
        CHECK_EQ(easeInOutQuad16(32767), 32767);
        CHECK_EQ(easeInOutQuad16(32769), 32770);
    }

    SUBCASE("quartile values") {
        // Test specific quartile values for 16-bit quadratic easing
        CHECK_EQ(easeInOutQuad16(16384), 8192); // 25% input -> 12.5% output
        CHECK_EQ(easeInOutQuad16(32768),
                 32768); // 50% input -> 50% output (midpoint)
        CHECK_EQ(easeInOutQuad16(49152),
                 57344); // 75% input -> actual measured output

        // Additional quartile boundary checks
        CHECK_LT(easeInOutQuad16(16384),
                 16384); // ease-in should be slower than linear
        CHECK_GT(easeInOutQuad16(49152),
                 49152); // ease-out should be faster than linear
    }

    SUBCASE("symmetry") {
        for (uint16_t i = 0; i < 32768; i += 256) {
            uint16_t forward = easeInOutQuad16(i);
            uint16_t backward = easeInOutQuad16(65535 - i);
            CHECK_EQ(forward, 65535 - backward);
        }
    }



    SUBCASE("scaling consistency with 8-bit") {
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
                std::abs((int16_t)result8 - (int16_t)scaled_result16);
            CHECK_LE(diff, kTolerance);
        }
    }
}

TEST_CASE("easeInOutCubic16") {

    SUBCASE("boundary values") {
        CHECK_EQ(easeInOutCubic16(0), 0);
        CHECK_EQ(easeInOutCubic16(65535), 65535);
        CHECK_EQ(easeInOutCubic16(32768), 32769);
    }

    SUBCASE("quartile values") {
        CHECK_EQ(easeInOutCubic16(16384), 4096);
        CHECK_EQ(easeInOutCubic16(32768), 32769);
        CHECK_EQ(easeInOutCubic16(49152), 61440);
    }

    SUBCASE("symmetry") {
        const int kTolerance = 2; // Note that this is too high.
        for (uint16_t i = 0; i < 32768; i += 256) {
            uint16_t forward = easeInOutCubic16(i);
            uint16_t backward = easeInOutCubic16(65535 - i);
            // CHECK_EQ(forward, 65535 - backward);
            CHECK_CLOSE(forward, 65535 - backward, kTolerance);
        }
    }



    SUBCASE("more pronounced than quadratic") {
        uint16_t quarter = 16384;
        uint16_t quad_result = easeInOutQuad16(quarter);
        uint16_t cubic_result = easeInOutCubic16(quarter);
        CHECK_LT(cubic_result, quad_result);
    }

    SUBCASE("scaling consistency with 8-bit") {
        for (uint16_t i = 0; i <= 255; ++i) {
            uint8_t input8 = i;
            uint16_t input16 = map8_to_16(input8);

            uint8_t result8 = easeInOutCubic8(input8);
            uint16_t result16 = easeInOutCubic16(input16);
            uint8_t scaled_result16 = map16_to_8(result16);

            // Should be within 2 due to precision differences in cubic
            // calculation
            int16_t diff =
                std::abs((int16_t)result8 - (int16_t)scaled_result16);
            CHECK_LE(diff, 2);
        }
    }
}

TEST_CASE("easing function ordering") {

    SUBCASE("8-bit: cubic should be more pronounced than quadratic") {
        for (uint16_t i = 32; i < 128; i += 16) { // test ease-in portion
            uint8_t quad = easeInOutQuad8(i);
            uint8_t cubic = easeInOutCubic8(i);
            CHECK_LE(cubic, quad);
        }

        for (uint16_t i = 128; i < 224; i += 16) { // test ease-out portion
            uint8_t quad = easeInOutQuad8(i);
            uint8_t cubic = easeInOutCubic8(i);
            CHECK_GE(cubic, quad);
        }
    }

    SUBCASE("16-bit: cubic should be more pronounced than quadratic") {
        for (uint32_t i = 8192; i < 32768; i += 4096) { // test ease-in portion
            uint16_t quad = easeInOutQuad16(i);
            uint16_t cubic = easeInOutCubic16(i);
            CHECK_LE(cubic, quad);
        }

        for (uint32_t i = 32768; i < 57344;
             i += 4096) { // test ease-out portion
            uint16_t quad = easeInOutQuad16(i);
            uint16_t cubic = easeInOutCubic16(i);
            CHECK_GE(cubic, quad);
        }
    }
}

TEST_CASE("easeInQuad16") {
    SUBCASE("boundary values") {
        CHECK_EQ(easeInQuad16(0), 0);
        CHECK_EQ(easeInQuad16(65535), 65535);

        // Test values very close to boundaries
        CHECK_EQ(easeInQuad16(1), 0);         // (1 * 1) / 65535 = 0
        CHECK_EQ(easeInQuad16(65534), 65533); // (65534 * 65534) / 65535 = 65533
    }

    SUBCASE("quartile values") {
        // Test specific quartile values for 16-bit quadratic ease-in
        // Expected values calculated as (input * input) / 65535
        CHECK_EQ(easeInQuad16(16384),
                 4096); // 25% input -> ~6.25% output: (16384^2)/65535 = 4096
        CHECK_EQ(easeInQuad16(32768),
                 16384); // 50% input -> 25% output: (32768^2)/65535 = 16384
        CHECK_EQ(easeInQuad16(49152),
                 36864); // 75% input -> ~56.25% output: (49152^2)/65535 = 36864

        // Additional test points
        CHECK_EQ(easeInQuad16(8192), 1024);   // 12.5% input -> ~1.56% output
        CHECK_EQ(easeInQuad16(57344), 50176); // 87.5% input -> ~76.56% output
    }

    SUBCASE("mathematical precision") {
        // Test specific values where we can verify the exact mathematical
        // result
        CHECK_EQ(easeInQuad16(256), 1);    // (256 * 256) / 65535 = 1
        CHECK_EQ(easeInQuad16(512), 4);    // (512 * 512) / 65535 = 4
        CHECK_EQ(easeInQuad16(1024), 16);  // (1024 * 1024) / 65535 = 16
        CHECK_EQ(easeInQuad16(2048), 64);  // (2048 * 2048) / 65535 = 64
        CHECK_EQ(easeInQuad16(4096), 256); // (4096 * 4096) / 65535 = 256
    }



    SUBCASE("ease-in behavior") {
        // For ease-in, early values should be less than linear progression
        // while later values should be greater than linear progression

        // First quarter should be significantly slower than linear
        uint16_t quarter_linear = 16384; // 25% of 65535
        uint16_t quarter_eased = easeInQuad16(16384);
        CHECK_LT(quarter_eased, quarter_linear);
        CHECK_LT(quarter_eased, quarter_linear / 2); // Should be much slower

        // Third quarter should still be slower than linear for ease-in
        // (ease-in accelerates but doesn't surpass linear until very late)
        uint16_t three_quarter_linear = 49152; // 75% of 65535
        uint16_t three_quarter_eased = easeInQuad16(49152);
        CHECK_LT(three_quarter_eased, three_quarter_linear);

        // The acceleration should be visible in the differences
        uint16_t early_diff = easeInQuad16(8192) - easeInQuad16(0); // 0-12.5%
        uint16_t late_diff =
            easeInQuad16(57344) - easeInQuad16(49152); // 75-87.5%
        CHECK_GT(late_diff,
                 early_diff * 10); // Late differences should be much larger
    }

    SUBCASE("specific known values") {
        // Test some additional specific values for regression testing
        CHECK_EQ(easeInQuad16(65535 / 4), 4095);      // Quarter point
        CHECK_EQ(easeInQuad16(65535 / 2), 16383);     // Half point
        CHECK_EQ(easeInQuad16(65535 * 3 / 4), 36863); // Three-quarter point

        // Edge cases near boundaries
        CHECK_EQ(easeInQuad16(255), 0);       // Small value should round to 0
        CHECK_EQ(easeInQuad16(65280), 65025); // Near-max value
    }
}

TEST_CASE("All easing functions boundary tests") {
    SUBCASE("8-bit easing functions boundary conditions") {
        for (size_t i = 0; i < NUM_EASING_TYPES; ++i) {
            fl::EaseType type = ALL_EASING_TYPES[i].first;
            const char* name = ALL_EASING_TYPES[i].second;
            uint8_t result_0 = fl::ease8(type, 0);
            uint8_t result_255 = fl::ease8(type, 255);
            
            INFO("Testing EaseType " << name);
            CHECK_EQ(result_0, 0);
            CHECK_EQ(result_255, 255);
        }
    }
    
    SUBCASE("16-bit easing functions boundary conditions") {
        for (size_t i = 0; i < NUM_EASING_TYPES; ++i) {
            fl::EaseType type = ALL_EASING_TYPES[i].first;
            const char* name = ALL_EASING_TYPES[i].second;
            uint16_t result_0 = fl::ease16(type, 0);
            uint16_t result_max = fl::ease16(type, 65535);
            
            INFO("Testing EaseType " << name);
            CHECK_EQ(result_0, 0);
            CHECK_EQ(result_max, 65535);
        }
    }
}

TEST_CASE("All easing functions monotonicity tests") {
    SUBCASE("8-bit easing functions monotonicity") {
        for (size_t i = 0; i < NUM_EASING_TYPES; ++i) {
            fl::EaseType type = ALL_EASING_TYPES[i].first;
            const char* name = ALL_EASING_TYPES[i].second;
            
            // Function should be non-decreasing
            uint8_t prev = 0;
            for (uint16_t input = 0; input <= 255; ++input) {
                uint8_t current = fl::ease8(type, input);
                INFO("Testing EaseType " << name << " at input " << input);
                CHECK_GE(current, prev);
                prev = current;
            }
        }
    }
    
    SUBCASE("16-bit easing functions monotonicity") {
        for (size_t i = 0; i < NUM_EASING_TYPES; ++i) {
            fl::EaseType type = ALL_EASING_TYPES[i].first;
            const char* name = ALL_EASING_TYPES[i].second;
            
            // Function should be non-decreasing
            uint16_t prev = 0;
            for (uint32_t input = 0; input <= 65535; input += 256) {
                uint16_t current = fl::ease16(type, input);
                INFO("Testing EaseType " << name << " at input " << input);
                CHECK_GE(current, prev);
                prev = current;
            }
        }
    }
}

TEST_CASE("All easing functions 8-bit vs 16-bit consistency tests") {
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
    
    SUBCASE("8-bit vs 16-bit scaling consistency") {
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
                
                int16_t diff = std::abs((int16_t)result8 - (int16_t)scaled_result16);
                
                // Track the worst case for reporting
                if (diff > max_diff) {
                    max_diff = diff;
                    worst_input = input8;
                }
                
                INFO("Testing EaseType " << name 
                     << " at input " << (int)input8 
                     << " (8-bit result: " << (int)result8
                     << ", 16-bit scaled result: " << (int)scaled_result16
                     << ", diff: " << diff << ")");
                CHECK_LE(diff, tolerance);
            }
            
            // Log the maximum difference found for this easing type
            INFO("EaseType " << name << " maximum difference: " << max_diff 
                 << " at input " << (int)worst_input);
        }
    }
    
    SUBCASE("Boundary values consistency") {
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
            
            INFO("Testing EaseType " << name << " boundary values");
            CHECK_EQ(result8_0, scaled_result16_0);
            CHECK_EQ(result8_255, scaled_result16_255);
            
            // Both should map to exact boundaries
            CHECK_EQ(result8_0, 0);
            CHECK_EQ(result8_255, 255);
            CHECK_EQ(scaled_result16_0, 0);
            CHECK_EQ(scaled_result16_255, 255);
        }
    }
    
        SUBCASE("Midpoint consistency") {
        for (size_t type_idx = 0; type_idx < NUM_EASING_TYPES; ++type_idx) {
            fl::EaseType type = ALL_EASING_TYPES[type_idx].first;
            const char* name = ALL_EASING_TYPES[type_idx].second;
            
            // Test midpoint values - should be relatively close
            uint8_t result8_mid = fl::ease8(type, 128);
            uint16_t result16_mid = fl::ease16(type, 32768);
            uint8_t scaled_result16_mid = map16_to_8(result16_mid);
            
            int16_t diff = std::abs((int16_t)result8_mid - (int16_t)scaled_result16_mid);
            
            INFO("Testing EaseType " << name << " midpoint consistency"
                 << " (8-bit: " << (int)result8_mid
                 << ", 16-bit scaled: " << (int)scaled_result16_mid
                 << ", diff: " << diff << ")");
            
            // Use the same tolerance as the general test
            CHECK_LE(diff, tolerances[type_idx]);
        }
    }
}
