// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "FastLED.h"
#include "lib8tion/scale8.h"
#include "lib8tion/intmap.h"
#include "fl/math_macros.h"
#include <math.h>

#include "fl/namespace.h"
FASTLED_USING_NAMESPACE


TEST_CASE("scale16") {
    CHECK_EQ(scale16(0, 0), 0);
    CHECK_EQ(scale16(0, 1), 0);
    CHECK_EQ(scale16(1, 0), 0);
    CHECK_EQ(scale16(0xffff, 0xffff), 0xffff);
    CHECK_EQ(scale16(0xffff, 0xffff >> 1), 0xffff >> 1);
    CHECK_EQ(scale16(0xffff >> 1, 0xffff >> 1), 0xffff >> 2);

    for (int i = 0; i < 16; ++i) {
        for (int j = 0; j < 16; ++j) {
            int total_bitshift = i + j;
            if (total_bitshift > 15) {
                break;
            }
            // print out info if this test fails to capture the i,j values that are failing
            INFO("i: " << i << " j: " << j << " total_bitshift: " << total_bitshift);
            CHECK_EQ(scale16(0xffff >> i, 0xffff >> j), 0xffff >> total_bitshift);
        }
    }
}


TEST_CASE("scale16by8") {
    CHECK_EQ(scale16by8(0, 0), 0);
    CHECK_EQ(scale16by8(0, 1), 0);
    CHECK_EQ(scale16by8(1, 0), 0);
    CHECK_EQ(scale16by8(map8_to_16(1), 1), 2);
    CHECK_EQ(scale16by8(0xffff, 0xff), 0xffff);
    CHECK_EQ(scale16by8(0xffff, 0xff >> 1), 0xffff >> 1);
    CHECK_EQ(scale16by8(0xffff >> 1, 0xff >> 1), 0xffff >> 2);

    for (int i = 0; i < 16; ++i) {
        for (int j = 0; j < 8; ++j) {
            int total_bitshift = i + j;
            if (total_bitshift > 7) {
                break;
            }
            // print out info if this test fails to capture the i,j values that are failing
            INFO("i: " << i << " j: " << j << " total_bitshift: " << total_bitshift);
            CHECK_EQ(scale16by8(0xffff >> i, 0xff >> j), 0xffff >> total_bitshift);
        }
    }
}

TEST_CASE("bit equivalence") {
    // tests that 8bit and 16bit are equivalent
    uint8_t r = 0xff;
    uint8_t r_scale = 0xff / 2;
    uint8_t brightness = 0xff / 2;
    uint16_t r_scale16 = map8_to_16(r_scale);
    uint16_t brightness16 = map8_to_16(brightness);
    uint16_t r16 = scale16by8(scale16(r_scale16, brightness16), r);
    uint8_t r8 = scale8(scale8(r_scale, brightness), r);
    CHECK_EQ(r16 >> 8, r8);
}

TEST_CASE("sqrt16") {
    float f = sqrt(.5) * 0xff;
    uint8_t result = sqrt16(map8_to_16(0xff / 2));
    CHECK_EQ(int(f), result);
    CHECK_EQ(sqrt8(0xff / 2), result);
}

TEST_CASE("fl_min and fl_max type promotion") {
    SUBCASE("int8_t and int16_t should promote to int16_t") {
        int8_t a = 10;
        int16_t b = 20;
        
        auto min_result = fl::fl_min(a, b);
        auto max_result = fl::fl_max(a, b);
        
        // Check that return type is int16_t
        static_assert(fl::is_same<decltype(min_result), int16_t>::value, "fl_min should return int16_t");
        static_assert(fl::is_same<decltype(max_result), int16_t>::value, "fl_max should return int16_t");
        
        // Check values
        CHECK_EQ(min_result, 10);
        CHECK_EQ(max_result, 20);
    }

    SUBCASE("uint8_t and int16_t should promote to int16_t") {
        uint8_t a = 100;
        int16_t b = 200;
        
        auto min_result = fl::fl_min(a, b);
        auto max_result = fl::fl_max(a, b);
        
        // Check that return type is int16_t
        static_assert(fl::is_same<decltype(min_result), int16_t>::value, "fl_min should return int16_t");
        static_assert(fl::is_same<decltype(max_result), int16_t>::value, "fl_max should return int16_t");
        
        // Check values
        CHECK_EQ(min_result, 100);
        CHECK_EQ(max_result, 200);
    }

    SUBCASE("int and float should promote to float") {
        int a = 30;
        float b = 25.5f;
        
        auto min_result = fl::fl_min(a, b);
        auto max_result = fl::fl_max(a, b);
        
        // Check that return type is float
        static_assert(fl::is_same<decltype(min_result), float>::value, "fl_min should return float");
        static_assert(fl::is_same<decltype(max_result), float>::value, "fl_max should return float");
        
        // Check values
        CHECK_EQ(min_result, 25.5f);
        CHECK_EQ(max_result, 30.0f);
    }

    SUBCASE("float and double should promote to double") {
        float a = 1.5f;
        double b = 2.7;
        
        auto min_result = fl::fl_min(a, b);
        auto max_result = fl::fl_max(a, b);
        
        // Check that return type is double
        static_assert(fl::is_same<decltype(min_result), double>::value, "fl_min should return double");
        static_assert(fl::is_same<decltype(max_result), double>::value, "fl_max should return double");
        
        // Check values
        CHECK_EQ(min_result, 1.5);
        CHECK_EQ(max_result, 2.7);
    }

    SUBCASE("same types should return same type") {
        int a = 5;
        int b = 10;
        
        auto min_result = fl::fl_min(a, b);
        auto max_result = fl::fl_max(a, b);
        
        // Check that return type is int
        static_assert(fl::is_same<decltype(min_result), int>::value, "fl_min should return int");
        static_assert(fl::is_same<decltype(max_result), int>::value, "fl_max should return int");
        
        // Check values
        CHECK_EQ(min_result, 5);
        CHECK_EQ(max_result, 10);
    }

    SUBCASE("signed and unsigned promotion with larger types") {
        int16_t a = 50;  // Use int16_t and uint16_t instead of int8_t/uint8_t
        uint16_t b = 100;
        
        auto min_result = fl::fl_min(a, b);
        auto max_result = fl::fl_max(a, b);
        
        // int16_t and uint16_t should return signed version (int16_t) when same size but different signedness
        static_assert(fl::is_same<decltype(min_result), int16_t>::value, "fl_min should return int16_t");
        static_assert(fl::is_same<decltype(max_result), int16_t>::value, "fl_max should return int16_t");
        
        // Basic functionality check: min should be less than max
        CHECK_EQ(min_result, 50);
        CHECK_EQ(max_result, 100);
        CHECK_LT(min_result, max_result);
    }

    SUBCASE("int32_t and uint32_t should return signed version") {
        int32_t a = 1000000;
        uint32_t b = 2000000;
        
        auto min_result = fl::fl_min(a, b);
        auto max_result = fl::fl_max(a, b);
        
        // int32_t and uint32_t should return signed version (int32_t) when same size but different signedness
        static_assert(fl::is_same<decltype(min_result), int32_t>::value, "fl_min should return int32_t");
        static_assert(fl::is_same<decltype(max_result), int32_t>::value, "fl_max should return int32_t");
        
        // Check values
        CHECK_EQ(min_result, 1000000);
        CHECK_EQ(max_result, 2000000);
    }

    SUBCASE("edge case: floating point vs large integer") {
        long long a = 1000000LL;
        float b = 999.9f;
        
        auto min_result = fl::fl_min(a, b);
        auto max_result = fl::fl_max(a, b);
        
        // Should promote to float since it has higher rank in our system
        static_assert(fl::is_same<decltype(min_result), float>::value, "fl_min should return float");
        static_assert(fl::is_same<decltype(max_result), float>::value, "fl_max should return float");
        
        // Check values (allowing for floating point precision)
        CHECK_LT(min_result, max_result);
    }

    SUBCASE("runtime value verification from type traits") {
        // Test that the actual values work correctly, not just types
        short a = 100;
        int b = 200;
        auto result = fl::fl_min(a, b);
        static_assert(fl::is_same<decltype(result), int>::value,
                      "short + int min should return int");
        CHECK_EQ(result, 100);

        unsigned int c = 300;
        int d = 400;
        auto result2 = fl::fl_max(c, d);
        static_assert(fl::is_same<decltype(result2), int>::value,
                      "unsigned int + int max should return int");
        CHECK_EQ(result2, 400);

        float e = 1.5f;
        long f = 2;
        auto result3 = fl::fl_min(e, f);
        static_assert(fl::is_same<decltype(result3), float>::value,
                      "float + long min should return float");
        CHECK_EQ(result3, 1.5f);
    }

    SUBCASE("runtime value correctness with helper templates") {
        // Test that our helper logic produces correct runtime values, not just
        // types

        // Test size-based promotion
        int8_t small = 100;
        int32_t large = 200;
        auto size_result = fl::fl_max(small, large);
        static_assert(fl::is_same<decltype(size_result), int32_t>::value,
                      "size promotion should work");
        CHECK_EQ(size_result, 200);

        // Test rank-based promotion (int vs long same size case)
        int rank_low = 300;
        long rank_high = 400;
        auto rank_result = fl::fl_max(rank_low, rank_high);
        static_assert(fl::is_same<decltype(rank_result), long>::value,
                      "rank promotion should work");
        CHECK_EQ(rank_result, 400);

        // Test signedness-based promotion
        int16_t signed_val = 500;
        uint16_t unsigned_val = 600;
        auto sign_result = fl::fl_max(signed_val, unsigned_val);
        static_assert(fl::is_same<decltype(sign_result), int16_t>::value,
                      "signedness promotion should work");
        CHECK_EQ(sign_result, 600); // Values should still work correctly

        // Test floating point promotion
        int int_val = 700;
        float float_val = 750.5f;
        auto float_result = fl::fl_max(int_val, float_val);
        static_assert(fl::is_same<decltype(float_result), float>::value,
                      "float promotion should work");
        CHECK_EQ(float_result, 750.5f);
    }
}
