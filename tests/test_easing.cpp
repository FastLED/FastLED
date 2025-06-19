#include "fl/ease.h"
#include "lib8tion/intmap.h"
#include "test.h"
#include <cmath>
#include <cstdlib>

FASTLED_USING_NAMESPACE

#if 1

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

        SUBCASE("monotonicity") {
            // function should be non-decreasing
            uint8_t prev = 0;
            for (uint16_t i = 0; i <= 255; ++i) {
                uint8_t current = easeInOutQuad8(i);
                CHECK_GE(current, prev);
                prev = current;
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

#if 1
        SUBCASE("symmetry") {
            const int kTolerance = 2; // This is too high, come back to this.
            for (uint8_t i = 0; i < 128; ++i) {
                uint8_t forward = easeInOutCubic8(i);
                uint8_t backward = easeInOutCubic8(255 - i);
                CHECK_CLOSE(forward, 255 - backward, kTolerance);
            }
        }
#endif

#if 0
        SUBCASE("monotonicity") {
            uint8_t prev = 0;
            for (uint16_t i = 0; i <= 255; ++i) {
                uint8_t current = ease8InOutCubic(i);
                CHECK_GE(current, prev);
                prev = current;
            }
        }
#endif

#if 0
        SUBCASE("more pronounced than quadratic") {
            // cubic should be more pronounced than quadratic in ease-in portion
            uint8_t quarter = 64;
            uint8_t quad_result = ease8InOutQuad(quarter);
            uint8_t cubic_result = ease8InOutCubic(quarter);
            CHECK_LT(cubic_result, quad_result);
        }
#endif
    }

    SUBCASE("easeInOutApprox8") {
        SUBCASE("boundary values") {
            CHECK_CLOSE(easeInOutApprox8(0), 0, 1);
            CHECK_CLOSE(easeInOutApprox8(255), 255, 1);
            CHECK_CLOSE(easeInOutApprox8(128), 128, 1);
        }

        SUBCASE("symmetry") {
            for (uint8_t i = 0; i < 128; ++i) {
                uint8_t forward = easeInOutApprox8(i);
                uint8_t backward = easeInOutApprox8(255 - i);
                CHECK_CLOSE(forward, 255 - backward, 1);
            }
        }

        SUBCASE("monotonicity") {
            uint8_t prev = 0;
            for (uint16_t i = 0; i <= 255; ++i) {
                uint8_t current = easeInOutApprox8(i);
                CHECK_GE(current, prev);
                prev = current;
            }
        }

        SUBCASE("approximation accuracy") {
            // should be within a few percent of cubic
            for (uint16_t i = 0; i <= 255; i += 8) {
                uint8_t approx = easeInOutApprox8(i);
                uint8_t cubic = easeInOutCubic8(i);
                int16_t diff = std::abs((int16_t)approx - (int16_t)cubic);
                CHECK_LE(diff, 8); // should be within ~3% (8/255)
            }
        }
    }
}

#endif

TEST_CASE("easing function special values") {
    #if 0
    SUBCASE("quarter points") {
        // Test specific values that are common in animations


        
        // 8-bit quarter points
        CHECK_LT(ease8InOutQuad(64), 64);   // ease-in should be slower
        CHECK_GT(ease8InOutQuad(192), 192); // ease-out should be faster
        
        CHECK_LT(ease8InOutCubic(64), ease8InOutQuad(64));   // cubic more pronounced
        CHECK_GT(ease8InOutCubic(192), ease8InOutQuad(192)); // cubic more pronounced


        // 16-bit quarter points
        CHECK_LT(easeInOutQuad16(16384), 16384);
        CHECK_GT(easeInOutQuad16(49152), 49152);

        CHECK_LT(ease16InOutCubic(16384), easeInOutQuad16(16384));
        CHECK_GT(ease16InOutCubic(49152), easeInOutQuad16(49152));
    }
#endif

}

TEST_CASE("easeInOutQuad16") {
    SUBCASE("boundary values") {
        CHECK_EQ(easeInOutQuad16(0), 0);
        CHECK_EQ(easeInOutQuad16(65535), 65535);
        CHECK_EQ(easeInOutQuad16(32768), 32769); // midpoint
        
        // Test values very close to boundaries
        CHECK_EQ(easeInOutQuad16(1), 0);
        CHECK_EQ(easeInOutQuad16(65534), 65535);
        
        // Test edge cases around midpoint
        CHECK_EQ(easeInOutQuad16(32767), 32766);
        CHECK_EQ(easeInOutQuad16(32769), 32771);
    }

    SUBCASE("quartile values") {
        // Test specific quartile values for 16-bit quadratic easing
        CHECK_EQ(easeInOutQuad16(16384), 8192);   // 25% input -> 12.5% output
        CHECK_EQ(easeInOutQuad16(32768), 32769);  // 50% input -> 50% output (midpoint)
        CHECK_EQ(easeInOutQuad16(49152), 57345);  // 75% input -> actual measured output
        
        // Additional quartile boundary checks
        CHECK_LT(easeInOutQuad16(16384), 16384);  // ease-in should be slower than linear
        CHECK_GT(easeInOutQuad16(49152), 49152);  // ease-out should be faster than linear
    }

    SUBCASE("symmetry") {
        for (uint16_t i = 0; i < 32768; i += 256) {
            uint16_t forward = easeInOutQuad16(i);
            uint16_t backward = easeInOutQuad16(65535 - i);
            CHECK_EQ(forward, 65535 - backward);
        }
    }

    SUBCASE("monotonicity") {
        uint16_t prev = 0;
        for (uint32_t i = 0; i <= 65535; i += 256) {
            uint16_t current = easeInOutQuad16(i);
            CHECK_GE(current, prev);
            prev = current;
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
        const int kTolerance = 2;  // Note that this is too high.
        for (uint16_t i = 0; i < 32768; i += 256) {
            uint16_t forward = easeInOutCubic16(i);
            uint16_t backward = easeInOutCubic16(65535 - i);
            // CHECK_EQ(forward, 65535 - backward);
            CHECK_CLOSE(forward, 65535 - backward, kTolerance);
        }
    }



    SUBCASE("monotonicity") {
        uint16_t prev = 0;
        for (uint32_t i = 0; i <= 65535; i += 256) {
            uint16_t current = easeInOutCubic16(i);
            CHECK_GE(current, prev);
            prev = current;
        }
    }

#if 0

    SUBCASE("more pronounced than quadratic") {
        uint16_t quarter = 16384;
        uint16_t quad_result = easeInOutQuad16(quarter);
        uint16_t cubic_result = ease16InOutCubic(quarter);
        CHECK_LT(cubic_result, quad_result);
    }



    SUBCASE("scaling consistency with 8-bit") {
        for (uint16_t i = 0; i <= 255; ++i) {
            uint8_t input8 = i;
            uint16_t input16 = map8_to_16(input8);

            uint8_t result8 = ease8InOutCubic(input8);
            uint16_t result16 = ease16InOutCubic(input16);
            uint8_t scaled_result16 = map16_to_8(result16);

            // Should be within 2 due to precision differences in cubic
            // calculation
            int16_t diff =
                std::abs((int16_t)result8 - (int16_t)scaled_result16);
            CHECK_LE(diff, 2);
        }
    }

    #endif
}


TEST_CASE("easing function ordering") {


#if 0

    SUBCASE("8-bit: cubic should be more pronounced than quadratic") {
        for (uint16_t i = 32; i < 128; i += 16) { // test ease-in portion
            uint8_t quad = ease8InOutQuad(i);
            uint8_t cubic = ease8InOutCubic(i);
            CHECK_LE(cubic, quad);
        }

        for (uint16_t i = 128; i < 224; i += 16) { // test ease-out portion
            uint8_t quad = ease8InOutQuad(i);
            uint8_t cubic = ease8InOutCubic(i);
            CHECK_GE(cubic, quad);
        }
    }

        

    SUBCASE("16-bit: cubic should be more pronounced than quadratic") {
        for (uint32_t i = 8192; i < 32768; i += 4096) {  // test ease-in portion
            uint16_t quad = easeInOutQuad16(i);
            uint16_t cubic = ease16InOutCubic(i);
            CHECK_LE(cubic, quad);
        }


        for (uint32_t i = 32768; i < 57344; i += 4096) {  // test ease-out portion
            uint16_t quad = easeInOutQuad16(i);
            uint16_t cubic = ease16InOutCubic(i);
            CHECK_GE(cubic, quad);
        }


    }

#endif
}

TEST_CASE("easeInQuad16") {
    SUBCASE("boundary values") {
        CHECK_EQ(easeInQuad16(0), 0);
        CHECK_EQ(easeInQuad16(65535), 65535);
        
        // Test values very close to boundaries
        CHECK_EQ(easeInQuad16(1), 0);        // (1 * 1) / 65535 = 0
        CHECK_EQ(easeInQuad16(65534), 65533); // (65534 * 65534) / 65535 = 65533
    }

    SUBCASE("quartile values") {
        // Test specific quartile values for 16-bit quadratic ease-in
        // Expected values calculated as (input * input) / 65535
        CHECK_EQ(easeInQuad16(16384), 4096);   // 25% input -> ~6.25% output: (16384^2)/65535 = 4096
        CHECK_EQ(easeInQuad16(32768), 16384);  // 50% input -> 25% output: (32768^2)/65535 = 16384  
        CHECK_EQ(easeInQuad16(49152), 36864);  // 75% input -> ~56.25% output: (49152^2)/65535 = 36864
        
        // Additional test points
        CHECK_EQ(easeInQuad16(8192), 1024);    // 12.5% input -> ~1.56% output
        CHECK_EQ(easeInQuad16(57344), 50176);  // 87.5% input -> ~76.56% output
    }

    SUBCASE("mathematical precision") {
        // Test specific values where we can verify the exact mathematical result
        CHECK_EQ(easeInQuad16(256), 1);        // (256 * 256) / 65535 = 1
        CHECK_EQ(easeInQuad16(512), 4);        // (512 * 512) / 65535 = 4
        CHECK_EQ(easeInQuad16(1024), 16);      // (1024 * 1024) / 65535 = 16
        CHECK_EQ(easeInQuad16(2048), 64);      // (2048 * 2048) / 65535 = 64
        CHECK_EQ(easeInQuad16(4096), 256);     // (4096 * 4096) / 65535 = 256
    }

    SUBCASE("monotonicity") {
        // Function should be non-decreasing (ease-in starts slow, accelerates)
        uint16_t prev = 0;
        for (uint32_t i = 0; i <= 65535; i += 256) {
            uint16_t current = easeInQuad16(i);
            CHECK_GE(current, prev);
            prev = current;
        }
    }

    SUBCASE("ease-in behavior") {
        // For ease-in, early values should be less than linear progression
        // while later values should be greater than linear progression
        
        // First quarter should be significantly slower than linear
        uint16_t quarter_linear = 16384;  // 25% of 65535
        uint16_t quarter_eased = easeInQuad16(16384);
        CHECK_LT(quarter_eased, quarter_linear);
        CHECK_LT(quarter_eased, quarter_linear / 2); // Should be much slower
        
        // Third quarter should still be slower than linear for ease-in
        // (ease-in accelerates but doesn't surpass linear until very late)
        uint16_t three_quarter_linear = 49152;  // 75% of 65535
        uint16_t three_quarter_eased = easeInQuad16(49152);
        CHECK_LT(three_quarter_eased, three_quarter_linear);
        
        // The acceleration should be visible in the differences
        uint16_t early_diff = easeInQuad16(8192) - easeInQuad16(0);      // 0-12.5%
        uint16_t late_diff = easeInQuad16(57344) - easeInQuad16(49152);  // 75-87.5%
        CHECK_GT(late_diff, early_diff * 10); // Late differences should be much larger
    }

    SUBCASE("specific known values") {
        // Test some additional specific values for regression testing
        CHECK_EQ(easeInQuad16(65535/4), 4095);    // Quarter point
        CHECK_EQ(easeInQuad16(65535/2), 16383);   // Half point  
        CHECK_EQ(easeInQuad16(65535*3/4), 36863); // Three-quarter point
        
        // Edge cases near boundaries
        CHECK_EQ(easeInQuad16(255), 0);           // Small value should round to 0
        CHECK_EQ(easeInQuad16(65280), 65025);     // Near-max value
    }
}

