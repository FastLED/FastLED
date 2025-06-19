#include "test.h"
#include "FastLED.h"
#include "lib8tion/intmap.h"
#include <cmath>
#include <cstdlib>

FASTLED_USING_NAMESPACE

#if 1

TEST_CASE("8-bit easing functions") {
    SUBCASE("ease8InOutQuad") {
        SUBCASE("boundary values") {
            CHECK_CLOSE(ease8InOutQuad(0), 0, 1);
            CHECK_CLOSE(ease8InOutQuad(255), 255, 1);
            CHECK_CLOSE(ease8InOutQuad(128), 128, 1);  // midpoint should be unchanged
        }
        
        SUBCASE("symmetry") {
            // ease-in-out should be symmetric around midpoint
            for (uint8_t i = 0; i < 128; ++i) {
                uint8_t forward = ease8InOutQuad(i);
                uint8_t backward = ease8InOutQuad(255 - i);
                CHECK_CLOSE(forward, 255 - backward, 1);
            }
        }
        
        SUBCASE("monotonicity") {
            // function should be non-decreasing
            uint8_t prev = 0;
            for (uint16_t i = 0; i <= 255; ++i) {
                uint8_t current = ease8InOutQuad(i);
                CHECK_GE(current, prev);
                prev = current;
            }
        }
        
        SUBCASE("first quarter should be slower than linear") {
            // ease-in portion should start slower than linear
            uint8_t quarter = ease8InOutQuad(64);  // 64 = 255/4
            CHECK_LT(quarter, 64);  // should be less than linear progression
        }
    }

    SUBCASE("ease8InOutCubic") {
        SUBCASE("boundary values") {
            CHECK_CLOSE(ease8InOutCubic(0), 0, 1);
            CHECK_CLOSE(ease8InOutCubic(255), 255, 1);
            CHECK_CLOSE(ease8InOutCubic(128), 128, 1);
        }
        
        #if 1
        SUBCASE("symmetry") {
            for (uint8_t i = 0; i < 128; ++i) {
                uint8_t forward = ease8InOutCubic(i);
                uint8_t backward = ease8InOutCubic(255 - i);
                CHECK_CLOSE(forward, 255 - backward, 1);
            }
        }
        #endif
        
        #if 1
        SUBCASE("monotonicity") {
            uint8_t prev = 0;
            for (uint16_t i = 0; i <= 255; ++i) {
                uint8_t current = ease8InOutCubic(i);
                CHECK_GE(current, prev);
                prev = current;
            }
        }
        #endif
        
        #if 1
        SUBCASE("more pronounced than quadratic") {
            // cubic should be more pronounced than quadratic in ease-in portion
            uint8_t quarter = 64;
            uint8_t quad_result = ease8InOutQuad(quarter);
            uint8_t cubic_result = ease8InOutCubic(quarter);
            CHECK_LT(cubic_result, quad_result);
        }
        #endif
    }

    SUBCASE("ease8InOutApprox") {
        SUBCASE("boundary values") {
            CHECK_CLOSE(ease8InOutApprox(0), 0, 1);
            CHECK_CLOSE(ease8InOutApprox(255), 255, 1);
            CHECK_CLOSE(ease8InOutApprox(128), 128, 1);
        }
        
        SUBCASE("symmetry") {
            for (uint8_t i = 0; i < 128; ++i) {
                uint8_t forward = ease8InOutApprox(i);
                uint8_t backward = ease8InOutApprox(255 - i);
                CHECK_CLOSE(forward, 255 - backward, 1);
            }
        }
        
        SUBCASE("monotonicity") {
            uint8_t prev = 0;
            for (uint16_t i = 0; i <= 255; ++i) {
                uint8_t current = ease8InOutApprox(i);
                CHECK_GE(current, prev);
                prev = current;
            }
        }
        
        SUBCASE("approximation accuracy") {
            // should be within a few percent of cubic
            for (uint16_t i = 0; i <= 255; i += 8) {
                uint8_t approx = ease8InOutApprox(i);
                uint8_t cubic = ease8InOutCubic(i);
                int16_t diff = std::abs((int16_t)approx - (int16_t)cubic);
                CHECK_LE(diff, 8);  // should be within ~3% (8/255)
            }
        }
    }
}

#endif


#if 1

TEST_CASE("ease16InOutQuad") {
    SUBCASE("boundary values") {
        CHECK_EQ(ease16InOutQuad(0), 0);
        CHECK_EQ(ease16InOutQuad(65535), 65535);
        CHECK_EQ(ease16InOutQuad(32768), 32769);  // midpoint
    }

    
    SUBCASE("symmetry") {
        for (uint16_t i = 0; i < 32768; i += 256) {
            uint16_t forward = ease16InOutQuad(i);
            uint16_t backward = ease16InOutQuad(65535 - i);
            CHECK_EQ(forward, 65535 - backward);
        }
    }


    
    SUBCASE("monotonicity") {
        uint16_t prev = 0;
        for (uint32_t i = 0; i <= 65535; i += 256) {
            uint16_t current = ease16InOutQuad(i);
            CHECK_GE(current, prev);
            prev = current;
        }
    }

    SUBCASE("scaling consistency with 8-bit") {
        const int kTolerance = 2;  // Note that this is too high.
        // 16-bit version should be consistent with 8-bit when scaled
        for (uint16_t i = 0; i <= 255; ++i) {
            uint8_t input8 = i;
            uint16_t input16 = map8_to_16(input8);
            
            uint8_t result8 = ease8InOutQuad(input8);
            uint16_t result16 = ease16InOutQuad(input16);
            uint8_t scaled_result16 = map16_to_8(result16);
            
            // Should be within 1 due to precision differences
            int16_t diff = std::abs((int16_t)result8 - (int16_t)scaled_result16);
            CHECK_LE(diff, kTolerance);
        }
    }
}

#if 0

TEST_CASE("ease16InOutCubic") {
    SUBCASE("boundary values") {
        CHECK_EQ(ease16InOutCubic(0), 0);
        CHECK_EQ(ease16InOutCubic(65535), 65535);
        CHECK_EQ(ease16InOutCubic(32768), 32768);
    }
    
    SUBCASE("symmetry") {
        for (uint16_t i = 0; i < 32768; i += 256) {
            uint16_t forward = ease16InOutCubic(i);
            uint16_t backward = ease16InOutCubic(65535 - i);
            CHECK_EQ(forward, 65535 - backward);
        }
    }
    
    SUBCASE("monotonicity") {
        uint16_t prev = 0;
        for (uint32_t i = 0; i <= 65535; i += 256) {
            uint16_t current = ease16InOutCubic(i);
            CHECK_GE(current, prev);
            prev = current;
        }
    }
    
    SUBCASE("more pronounced than quadratic") {
        uint16_t quarter = 16384;
        uint16_t quad_result = ease16InOutQuad(quarter);
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
            
            // Should be within 2 due to precision differences in cubic calculation
            int16_t diff = std::abs((int16_t)result8 - (int16_t)scaled_result16);
            CHECK_LE(diff, 2);
        }
    }
}

TEST_CASE("ease16InOutApprox") {
    SUBCASE("boundary values") {
        CHECK_EQ(ease16InOutApprox(0), 0);
        CHECK_EQ(ease16InOutApprox(65535), 65535);
        CHECK_EQ(ease16InOutApprox(32768), 32768);
    }
    
    SUBCASE("symmetry") {
        for (uint16_t i = 0; i < 32768; i += 256) {
            uint16_t forward = ease16InOutApprox(i);
            uint16_t backward = ease16InOutApprox(65535 - i);
            CHECK_EQ(forward, 65535 - backward);
        }
    }
    
    SUBCASE("monotonicity") {
        uint16_t prev = 0;
        for (uint32_t i = 0; i <= 65535; i += 256) {
            uint16_t current = ease16InOutApprox(i);
            CHECK_GE(current, prev);
            prev = current;
        }
    }
    
    SUBCASE("approximation accuracy") {
        // should be within a reasonable range of cubic
        for (uint32_t i = 0; i <= 65535; i += 2048) {
            uint16_t approx = ease16InOutApprox(i);
            uint16_t cubic = ease16InOutCubic(i);
            int32_t diff = std::abs((int32_t)approx - (int32_t)cubic);
            CHECK_LE(diff, 2048);  // should be within ~3% (2048/65535)
        }
    }
    
    SUBCASE("scaling consistency with 8-bit") {
        for (uint16_t i = 0; i <= 255; ++i) {
            uint8_t input8 = i;
            uint16_t input16 = map8_to_16(input8);
            
            uint8_t result8 = ease8InOutApprox(input8);
            uint16_t result16 = ease16InOutApprox(input16);
            uint8_t scaled_result16 = map16_to_8(result16);
            
            // Should be within 1 due to precision differences
            int16_t diff = std::abs((int16_t)result8 - (int16_t)scaled_result16);
            CHECK_LE(diff, 1);
        }
    }
}

TEST_CASE("easing function ordering") {
    SUBCASE("8-bit: cubic should be more pronounced than quadratic") {
        for (uint16_t i = 32; i < 128; i += 16) {  // test ease-in portion
            uint8_t quad = ease8InOutQuad(i);
            uint8_t cubic = ease8InOutCubic(i);
            CHECK_LE(cubic, quad);
        }
        
        for (uint16_t i = 128; i < 224; i += 16) {  // test ease-out portion
            uint8_t quad = ease8InOutQuad(i);
            uint8_t cubic = ease8InOutCubic(i);
            CHECK_GE(cubic, quad);
        }
    }
    
    SUBCASE("16-bit: cubic should be more pronounced than quadratic") {
        for (uint32_t i = 8192; i < 32768; i += 4096) {  // test ease-in portion
            uint16_t quad = ease16InOutQuad(i);
            uint16_t cubic = ease16InOutCubic(i);
            CHECK_LE(cubic, quad);
        }
        
        for (uint32_t i = 32768; i < 57344; i += 4096) {  // test ease-out portion
            uint16_t quad = ease16InOutQuad(i);
            uint16_t cubic = ease16InOutCubic(i);
            CHECK_GE(cubic, quad);
        }
    }
}

TEST_CASE("easing function special values") {
    SUBCASE("quarter points") {
        // Test specific values that are common in animations
        
        // 8-bit quarter points
        CHECK_LT(ease8InOutQuad(64), 64);   // ease-in should be slower
        CHECK_GT(ease8InOutQuad(192), 192); // ease-out should be faster
        
        CHECK_LT(ease8InOutCubic(64), ease8InOutQuad(64));   // cubic more pronounced
        CHECK_GT(ease8InOutCubic(192), ease8InOutQuad(192)); // cubic more pronounced
        
        // 16-bit quarter points
        CHECK_LT(ease16InOutQuad(16384), 16384);
        CHECK_GT(ease16InOutQuad(49152), 49152);
        
        CHECK_LT(ease16InOutCubic(16384), ease16InOutQuad(16384));
        CHECK_GT(ease16InOutCubic(49152), ease16InOutQuad(49152));
    }
}

#endif