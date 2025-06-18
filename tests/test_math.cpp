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
}

TEST_CASE("common_type_impl behavior") {
    SUBCASE("same types return same type") {
        static_assert(fl::is_same<fl::common_type_t<int, int>, int>::value, "int + int should return int");
        static_assert(fl::is_same<fl::common_type_t<short, short>, short>::value, "short + short should return short");
        static_assert(fl::is_same<fl::common_type_t<long, long>, long>::value, "long + long should return long");
        static_assert(fl::is_same<fl::common_type_t<float, float>, float>::value, "float + float should return float");
    }

    SUBCASE("different size promotions with generic types") {
        // Smaller to larger promotions
        static_assert(fl::is_same<fl::common_type_t<short, int>, int>::value, "short + int should return int");
        static_assert(fl::is_same<fl::common_type_t<int, short>, int>::value, "int + short should return int");
        static_assert(fl::is_same<fl::common_type_t<int, long>, long>::value, "int + long should return long");
        static_assert(fl::is_same<fl::common_type_t<long, int>, long>::value, "long + int should return long");
        static_assert(fl::is_same<fl::common_type_t<long, long long>, long long>::value, "long + long long should return long long");
    }

    SUBCASE("mixed signedness same size with generic types") {
        // These should use the partial specialization to choose signed version
        static_assert(fl::is_same<fl::common_type_t<short, unsigned short>, short>::value, "short + unsigned short should return short");
        static_assert(fl::is_same<fl::common_type_t<unsigned short, short>, short>::value, "unsigned short + short should return short");
        static_assert(fl::is_same<fl::common_type_t<int, unsigned int>, int>::value, "int + unsigned int should return int");
        static_assert(fl::is_same<fl::common_type_t<unsigned int, int>, int>::value, "unsigned int + int should return int");
        static_assert(fl::is_same<fl::common_type_t<long, unsigned long>, long>::value, "long + unsigned long should return long");
    }

    SUBCASE("float/double promotions with generic types") {
        // Float always wins over integers
        static_assert(fl::is_same<fl::common_type_t<int, float>, float>::value, "int + float should return float");
        static_assert(fl::is_same<fl::common_type_t<float, int>, float>::value, "float + int should return float");
        static_assert(fl::is_same<fl::common_type_t<short, float>, float>::value, "short + float should return float");
        static_assert(fl::is_same<fl::common_type_t<long, float>, float>::value, "long + float should return float");
        
        // Double wins over float
        static_assert(fl::is_same<fl::common_type_t<float, double>, double>::value, "float + double should return double");
        static_assert(fl::is_same<fl::common_type_t<double, float>, double>::value, "double + float should return double");
        static_assert(fl::is_same<fl::common_type_t<int, double>, double>::value, "int + double should return double");
    }

    SUBCASE("sized types with generic types mixed") {
        // Verify sized types work with generic types
        static_assert(fl::is_same<fl::common_type_t<int8_t, int>, int>::value, "int8_t + int should return int");
        static_assert(fl::is_same<fl::common_type_t<int, int8_t>, int>::value, "int + int8_t should return int");
        static_assert(fl::is_same<fl::common_type_t<uint16_t, int>, int>::value, "uint16_t + int should return int");
        static_assert(fl::is_same<fl::common_type_t<short, int32_t>, int32_t>::value, "short + int32_t should return int32_t");
    }

    SUBCASE("cross signedness different sizes with generic types") {
        // Larger type wins when different sizes
        static_assert(fl::is_same<fl::common_type_t<char, unsigned int>, unsigned int>::value, "char + unsigned int should return unsigned int");
        static_assert(fl::is_same<fl::common_type_t<unsigned char, int>, int>::value, "unsigned char + int should return int");
        static_assert(fl::is_same<fl::common_type_t<short, unsigned long>, unsigned long>::value, "short + unsigned long should return unsigned long");
    }

    SUBCASE("explicit sized type combinations") {
        // Test the explicit sized type specializations we added
        static_assert(fl::is_same<fl::common_type_t<int8_t, int16_t>, int16_t>::value, "int8_t + int16_t should return int16_t");
        static_assert(fl::is_same<fl::common_type_t<uint8_t, uint32_t>, uint32_t>::value, "uint8_t + uint32_t should return uint32_t");
        static_assert(fl::is_same<fl::common_type_t<int16_t, uint32_t>, uint32_t>::value, "int16_t + uint32_t should return uint32_t");
        static_assert(fl::is_same<fl::common_type_t<uint16_t, int32_t>, int32_t>::value, "uint16_t + int32_t should return int32_t");
    }

    SUBCASE("mixed signedness same size with sized types") {
        // Test partial specialization with sized types
        static_assert(fl::is_same<fl::common_type_t<int16_t, uint16_t>, int16_t>::value, "int16_t + uint16_t should return int16_t");
        static_assert(fl::is_same<fl::common_type_t<uint16_t, int16_t>, int16_t>::value, "uint16_t + int16_t should return int16_t");
        static_assert(fl::is_same<fl::common_type_t<int32_t, uint32_t>, int32_t>::value, "int32_t + uint32_t should return int32_t");
        static_assert(fl::is_same<fl::common_type_t<int64_t, uint64_t>, int64_t>::value, "int64_t + uint64_t should return int64_t");
    }

    SUBCASE("runtime value verification") {
        // Test that the actual values work correctly, not just types
        short a = 100;
        int b = 200;
        auto result = fl::fl_min(a, b);
        static_assert(fl::is_same<decltype(result), int>::value, "short + int min should return int");
        CHECK_EQ(result, 100);

        unsigned int c = 300;
        int d = 400;
        auto result2 = fl::fl_max(c, d);
        static_assert(fl::is_same<decltype(result2), int>::value, "unsigned int + int max should return int");
        CHECK_EQ(result2, 400);

        float e = 1.5f;
        long f = 2;
        auto result3 = fl::fl_min(e, f);
        static_assert(fl::is_same<decltype(result3), float>::value, "float + long min should return float");
        CHECK_EQ(result3, 1.5f);
    }
}