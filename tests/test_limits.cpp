
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "fl/limits.h"
using namespace fl;
TEST_CASE("numeric_limits basic properties") {
    SUBCASE("int is specialized") {
        CHECK(fl::numeric_limits<int>::is_specialized);
        CHECK(fl::numeric_limits<int>::is_signed);
        CHECK(fl::numeric_limits<int>::is_integer);
        CHECK(fl::numeric_limits<int>::is_exact);
        CHECK_FALSE(fl::numeric_limits<int>::has_infinity);
    }

    SUBCASE("unsigned int is specialized") {
        CHECK(fl::numeric_limits<unsigned int>::is_specialized);
        CHECK_FALSE(fl::numeric_limits<unsigned int>::is_signed);
        CHECK(fl::numeric_limits<unsigned int>::is_integer);
        CHECK(fl::numeric_limits<unsigned int>::is_exact);
    }

    SUBCASE("float is specialized") {
        CHECK(fl::numeric_limits<float>::is_specialized);
        CHECK(fl::numeric_limits<float>::is_signed);
        CHECK_FALSE(fl::numeric_limits<float>::is_integer);
        CHECK_FALSE(fl::numeric_limits<float>::is_exact);
        CHECK(fl::numeric_limits<float>::has_infinity);
        CHECK(fl::numeric_limits<float>::has_quiet_NaN);
    }

    SUBCASE("double is specialized") {
        CHECK(fl::numeric_limits<double>::is_specialized);
        CHECK(fl::numeric_limits<double>::is_signed);
        CHECK_FALSE(fl::numeric_limits<double>::is_integer);
        CHECK(fl::numeric_limits<double>::has_infinity);
    }
}

TEST_CASE("numeric_limits integer values") {
    SUBCASE("int8_t (i8) limits") {
        CHECK(fl::numeric_limits<i8>::min() == -128);
        CHECK(fl::numeric_limits<i8>::max() == 127);
        CHECK(fl::numeric_limits<i8>::lowest() == -128);
    }

    SUBCASE("uint8_t (u8) limits") {
        CHECK(fl::numeric_limits<u8>::min() == 0);
        CHECK(fl::numeric_limits<u8>::max() == 255);
        CHECK(fl::numeric_limits<u8>::lowest() == 0);
    }

    SUBCASE("int16_t (i16) limits") {
        CHECK(fl::numeric_limits<i16>::min() == -32768);
        CHECK(fl::numeric_limits<i16>::max() == 32767);
    }

    SUBCASE("uint16_t (u16) limits") {
        CHECK(fl::numeric_limits<u16>::min() == 0);
        CHECK(fl::numeric_limits<u16>::max() == 65535);
    }

    SUBCASE("int32_t (i32) limits") {
        CHECK(fl::numeric_limits<i32>::min() == -2147483647 - 1);
        CHECK(fl::numeric_limits<i32>::max() == 2147483647);
    }

    SUBCASE("uint32_t (u32) limits") {
        CHECK(fl::numeric_limits<u32>::min() == 0);
        CHECK(fl::numeric_limits<u32>::max() == 4294967295U);
    }

    SUBCASE("int64_t (i64) limits") {
        CHECK(fl::numeric_limits<i64>::min() == -9223372036854775807LL - 1);
        CHECK(fl::numeric_limits<i64>::max() == 9223372036854775807LL);
    }

    SUBCASE("uint64_t (u64) limits") {
        CHECK(fl::numeric_limits<u64>::min() == 0);
        CHECK(fl::numeric_limits<u64>::max() == 18446744073709551615ULL);
    }
}

TEST_CASE("numeric_limits floating point values") {
    SUBCASE("float epsilon") {
        float eps = fl::numeric_limits<float>::epsilon();
        CHECK(eps > 0.0f);
        CHECK(eps < 0.001f);
        // Verify epsilon is approximately FLT_EPSILON (1.19209290e-07F)
        CHECK(eps > 1.0e-8f);
        CHECK(eps < 1.0e-6f);
    }

    SUBCASE("double epsilon") {
        double eps = fl::numeric_limits<double>::epsilon();
        CHECK(eps > 0.0);
        CHECK(eps < 0.0001);
        // Verify epsilon is approximately DBL_EPSILON (2.2204460492503131e-16)
        CHECK(eps > 1.0e-17);
        CHECK(eps < 1.0e-15);
    }

    SUBCASE("float min/max") {
        float fmin = fl::numeric_limits<float>::min();
        float fmax = fl::numeric_limits<float>::max();
        CHECK(fmin > 0.0f);
        CHECK(fmin < 1.0e-30f);
        CHECK(fmax > 1.0e+30f);
        CHECK(fl::numeric_limits<float>::lowest() < 0.0f);
    }

    SUBCASE("double min/max") {
        double dmin = fl::numeric_limits<double>::min();
        double dmax = fl::numeric_limits<double>::max();
        CHECK(dmin > 0.0);
        CHECK(dmin < 1.0e-300);
        CHECK(dmax > 1.0e+300);
        CHECK(fl::numeric_limits<double>::lowest() < 0.0);
    }
}

TEST_CASE("numeric_limits special values") {
    SUBCASE("float infinity") {
        float inf = fl::numeric_limits<float>::infinity();
        CHECK(inf > fl::numeric_limits<float>::max());
        CHECK(inf > 0.0f);
        // Check infinity arithmetic
        CHECK((inf + 1.0f) == inf);
        CHECK((inf * 2.0f) == inf);
    }

    SUBCASE("double infinity") {
        double inf = fl::numeric_limits<double>::infinity();
        CHECK(inf > fl::numeric_limits<double>::max());
        CHECK((inf + 1.0) == inf);
    }

    SUBCASE("float NaN") {
        float nan = fl::numeric_limits<float>::quiet_NaN();
        // NaN is not equal to itself
        CHECK(nan != nan);
    }

    SUBCASE("double NaN") {
        double nan = fl::numeric_limits<double>::quiet_NaN();
        // NaN is not equal to itself
        CHECK(nan != nan);
    }
}

TEST_CASE("numeric_limits constexpr compatibility") {
    SUBCASE("compile-time constants") {
        // These should all be compile-time constants
        constexpr int int_max = fl::numeric_limits<int>::max();
        constexpr int int_min = fl::numeric_limits<int>::min();
        constexpr float float_eps = fl::numeric_limits<float>::epsilon();
        constexpr double double_max = fl::numeric_limits<double>::max();
        constexpr bool int_signed = fl::numeric_limits<int>::is_signed;
        constexpr bool uint_signed = fl::numeric_limits<unsigned int>::is_signed;

        // Verify values (these checks happen at compile time)
        // int can be 16-bit (AVR) or 32-bit (most platforms)
        if (sizeof(int) == 4) {
            CHECK(int_max == 2147483647);
            CHECK(int_min == -2147483647 - 1);
        } else {
            CHECK(int_max == 32767);
            CHECK(int_min == -32768);
        }
        CHECK(int_signed == true);
        CHECK(uint_signed == false);

        // Use the constexpr values to avoid unused variable warnings
        (void)float_eps;
        (void)double_max;
    }
}

TEST_CASE("numeric_limits digits and precision") {
    SUBCASE("integer digits") {
        CHECK(fl::numeric_limits<i8>::digits == 7);   // 7 value bits (1 sign bit)
        CHECK(fl::numeric_limits<u8>::digits == 8);   // 8 value bits
        CHECK(fl::numeric_limits<i16>::digits == 15); // 15 value bits
        CHECK(fl::numeric_limits<u16>::digits == 16); // 16 value bits
        CHECK(fl::numeric_limits<i32>::digits == 31); // 31 value bits
        CHECK(fl::numeric_limits<u32>::digits == 32); // 32 value bits
        CHECK(fl::numeric_limits<i64>::digits == 63); // 63 value bits
        CHECK(fl::numeric_limits<u64>::digits == 64); // 64 value bits

        // int/unsigned int can vary by platform (16-bit on AVR, 32-bit elsewhere)
        if (sizeof(int) == 4) {
            CHECK(fl::numeric_limits<int>::digits == 31);
            CHECK(fl::numeric_limits<unsigned int>::digits == 32);
        } else {
            CHECK(fl::numeric_limits<int>::digits == 15);
            CHECK(fl::numeric_limits<unsigned int>::digits == 16);
        }
    }

    SUBCASE("floating point digits") {
        CHECK(fl::numeric_limits<float>::digits == 24);   // FLT_MANT_DIG
        CHECK(fl::numeric_limits<double>::digits == 53);  // DBL_MANT_DIG
        CHECK(fl::numeric_limits<float>::digits10 == 6);  // FLT_DIG
        CHECK(fl::numeric_limits<double>::digits10 == 15); // DBL_DIG
    }
}

TEST_CASE("numeric_limits bool") {
    CHECK(fl::numeric_limits<bool>::is_specialized);
    CHECK_FALSE(fl::numeric_limits<bool>::is_signed);
    CHECK(fl::numeric_limits<bool>::is_integer);
    CHECK(fl::numeric_limits<bool>::min() == false);
    CHECK(fl::numeric_limits<bool>::max() == true);
    CHECK(fl::numeric_limits<bool>::digits == 1);
}
