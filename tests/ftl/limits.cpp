#include "test.h"
#include "fl/stl/limits.h"
#include "doctest.h"

using namespace fl;

// Test primary template (unspecialized type)
TEST_CASE("fl::numeric_limits - primary template") {
    SUBCASE("struct without specialization") {
        struct CustomType {};

        CHECK_EQ(numeric_limits<CustomType>::is_specialized, false);
        CHECK_EQ(numeric_limits<CustomType>::is_signed, false);
        CHECK_EQ(numeric_limits<CustomType>::is_integer, false);
        CHECK_EQ(numeric_limits<CustomType>::is_exact, false);
        CHECK_EQ(numeric_limits<CustomType>::has_infinity, false);
        CHECK_EQ(numeric_limits<CustomType>::has_quiet_NaN, false);
        CHECK_EQ(numeric_limits<CustomType>::has_signaling_NaN, false);
        CHECK_EQ(numeric_limits<CustomType>::digits, 0);
        CHECK_EQ(numeric_limits<CustomType>::digits10, 0);
    }
}

// Test bool specialization
TEST_CASE("fl::numeric_limits<bool>") {
    SUBCASE("type properties") {
        CHECK_EQ(numeric_limits<bool>::is_specialized, true);
        CHECK_EQ(numeric_limits<bool>::is_signed, false);
        CHECK_EQ(numeric_limits<bool>::is_integer, true);
        CHECK_EQ(numeric_limits<bool>::is_exact, true);
        CHECK_EQ(numeric_limits<bool>::has_infinity, false);
        CHECK_EQ(numeric_limits<bool>::has_quiet_NaN, false);
        CHECK_EQ(numeric_limits<bool>::has_signaling_NaN, false);
    }

    SUBCASE("digits") {
        CHECK_EQ(numeric_limits<bool>::digits, 1);
        CHECK_EQ(numeric_limits<bool>::digits10, 0);
    }

    SUBCASE("min/max values") {
        CHECK_EQ(numeric_limits<bool>::min(), false);
        CHECK_EQ(numeric_limits<bool>::max(), true);
        CHECK_EQ(numeric_limits<bool>::lowest(), false);
    }

    SUBCASE("constexpr evaluation") {
        static_assert(numeric_limits<bool>::min() == false, "min() must be constexpr");
        static_assert(numeric_limits<bool>::max() == true, "max() must be constexpr");
        static_assert(numeric_limits<bool>::is_specialized, "is_specialized must be constexpr");
    }
}

// Test char specialization
TEST_CASE("fl::numeric_limits<char>") {
    SUBCASE("type properties") {
        CHECK_EQ(numeric_limits<char>::is_specialized, true);
        CHECK_EQ(numeric_limits<char>::is_integer, true);
        CHECK_EQ(numeric_limits<char>::is_exact, true);
        CHECK_EQ(numeric_limits<char>::has_infinity, false);
        CHECK_EQ(numeric_limits<char>::has_quiet_NaN, false);
        CHECK_EQ(numeric_limits<char>::has_signaling_NaN, false);

        // char signedness is platform-dependent
        bool expected_signed = (char(-1) < char(0));
        CHECK_EQ(numeric_limits<char>::is_signed, expected_signed);
    }

    SUBCASE("digits") {
        // char is 8 bits
        int expected_digits = 8 - (numeric_limits<char>::is_signed ? 1 : 0);
        CHECK_EQ(numeric_limits<char>::digits, expected_digits);
        CHECK_EQ(numeric_limits<char>::digits10, 2);
    }

    SUBCASE("min/max values") {
        char min_val = numeric_limits<char>::min();
        char max_val = numeric_limits<char>::max();

        // Verify min < max (or min == 0 for unsigned)
        bool valid_range = min_val < max_val || (min_val == 0 && !numeric_limits<char>::is_signed);
        CHECK(valid_range);

        // Verify lowest() == min() for integers
        CHECK_EQ(numeric_limits<char>::lowest(), min_val);

        // Verify epsilon and round_error are zero for integers
        CHECK_EQ(numeric_limits<char>::epsilon(), 0);
        CHECK_EQ(numeric_limits<char>::round_error(), 0);
    }
}

// Test signed char specialization
TEST_CASE("fl::numeric_limits<signed char>") {
    SUBCASE("type properties") {
        CHECK_EQ(numeric_limits<signed char>::is_specialized, true);
        CHECK_EQ(numeric_limits<signed char>::is_signed, true);
        CHECK_EQ(numeric_limits<signed char>::is_integer, true);
        CHECK_EQ(numeric_limits<signed char>::is_exact, true);
    }

    SUBCASE("digits") {
        CHECK_EQ(numeric_limits<signed char>::digits, 7);
        CHECK_EQ(numeric_limits<signed char>::digits10, 2);
    }

    SUBCASE("min/max values") {
        CHECK_EQ(numeric_limits<signed char>::min(), -128);
        CHECK_EQ(numeric_limits<signed char>::max(), 127);
        CHECK_EQ(numeric_limits<signed char>::lowest(), -128);
    }

    SUBCASE("constexpr evaluation") {
        static_assert(numeric_limits<signed char>::min() == -128, "min() must be constexpr");
        static_assert(numeric_limits<signed char>::max() == 127, "max() must be constexpr");
    }
}

// Test unsigned char specialization
TEST_CASE("fl::numeric_limits<unsigned char>") {
    SUBCASE("type properties") {
        CHECK_EQ(numeric_limits<unsigned char>::is_specialized, true);
        CHECK_EQ(numeric_limits<unsigned char>::is_signed, false);
        CHECK_EQ(numeric_limits<unsigned char>::is_integer, true);
        CHECK_EQ(numeric_limits<unsigned char>::is_exact, true);
    }

    SUBCASE("digits") {
        CHECK_EQ(numeric_limits<unsigned char>::digits, 8);
        CHECK_EQ(numeric_limits<unsigned char>::digits10, 2);
    }

    SUBCASE("min/max values") {
        CHECK_EQ(numeric_limits<unsigned char>::min(), 0);
        CHECK_EQ(numeric_limits<unsigned char>::max(), 255);
        CHECK_EQ(numeric_limits<unsigned char>::lowest(), 0);
    }

    SUBCASE("constexpr evaluation") {
        static_assert(numeric_limits<unsigned char>::min() == 0, "min() must be constexpr");
        static_assert(numeric_limits<unsigned char>::max() == 255, "max() must be constexpr");
    }
}

// Test short specialization
TEST_CASE("fl::numeric_limits<short>") {
    SUBCASE("type properties") {
        CHECK_EQ(numeric_limits<short>::is_specialized, true);
        CHECK_EQ(numeric_limits<short>::is_signed, true);
        CHECK_EQ(numeric_limits<short>::is_integer, true);
        CHECK_EQ(numeric_limits<short>::is_exact, true);
    }

    SUBCASE("digits") {
        CHECK_EQ(numeric_limits<short>::digits, 15);
        CHECK_EQ(numeric_limits<short>::digits10, 4);
    }

    SUBCASE("min/max values") {
        CHECK_EQ(numeric_limits<short>::min(), -32768);
        CHECK_EQ(numeric_limits<short>::max(), 32767);
        CHECK_EQ(numeric_limits<short>::lowest(), -32768);
    }

    SUBCASE("constexpr evaluation") {
        static_assert(numeric_limits<short>::min() == -32768, "min() must be constexpr");
        static_assert(numeric_limits<short>::max() == 32767, "max() must be constexpr");
    }
}

// Test unsigned short specialization
TEST_CASE("fl::numeric_limits<unsigned short>") {
    SUBCASE("type properties") {
        CHECK_EQ(numeric_limits<unsigned short>::is_specialized, true);
        CHECK_EQ(numeric_limits<unsigned short>::is_signed, false);
        CHECK_EQ(numeric_limits<unsigned short>::is_integer, true);
        CHECK_EQ(numeric_limits<unsigned short>::is_exact, true);
    }

    SUBCASE("digits") {
        CHECK_EQ(numeric_limits<unsigned short>::digits, 16);
        CHECK_EQ(numeric_limits<unsigned short>::digits10, 4);
    }

    SUBCASE("min/max values") {
        CHECK_EQ(numeric_limits<unsigned short>::min(), 0);
        CHECK_EQ(numeric_limits<unsigned short>::max(), 65535);
        CHECK_EQ(numeric_limits<unsigned short>::lowest(), 0);
    }

    SUBCASE("constexpr evaluation") {
        static_assert(numeric_limits<unsigned short>::min() == 0, "min() must be constexpr");
        static_assert(numeric_limits<unsigned short>::max() == 65535, "max() must be constexpr");
    }
}

// Test int specialization (platform-dependent: 16 or 32 bits)
TEST_CASE("fl::numeric_limits<int>") {
    SUBCASE("type properties") {
        CHECK_EQ(numeric_limits<int>::is_specialized, true);
        CHECK_EQ(numeric_limits<int>::is_signed, true);
        CHECK_EQ(numeric_limits<int>::is_integer, true);
        CHECK_EQ(numeric_limits<int>::is_exact, true);
    }

    SUBCASE("digits - platform-dependent") {
        // int can be 16-bit (AVR) or 32-bit (most platforms)
        int expected_digits = (sizeof(int) * 8) - 1;
        CHECK_EQ(numeric_limits<int>::digits, expected_digits);

        int expected_digits10 = (sizeof(int) == 2) ? 4 : 9;
        CHECK_EQ(numeric_limits<int>::digits10, expected_digits10);
    }

    SUBCASE("min/max values - platform-dependent") {
        int min_val = numeric_limits<int>::min();
        int max_val = numeric_limits<int>::max();

        // Verify min is negative and max is positive
        CHECK(min_val < 0);
        CHECK(max_val > 0);

        // Verify min/max are consistent with size
        if (sizeof(int) == 2) {
            CHECK_EQ(min_val, -32768);
            CHECK_EQ(max_val, 32767);
        } else if (sizeof(int) == 4) {
            CHECK_EQ(min_val, -2147483647 - 1);
            CHECK_EQ(max_val, 2147483647);
        }

        CHECK_EQ(numeric_limits<int>::lowest(), min_val);
    }

    SUBCASE("constexpr evaluation") {
        constexpr int min_val = numeric_limits<int>::min();
        constexpr int max_val = numeric_limits<int>::max();
        CHECK(min_val < 0);
        CHECK(max_val > 0);
    }
}

// Test unsigned int specialization (platform-dependent: 16 or 32 bits)
TEST_CASE("fl::numeric_limits<unsigned int>") {
    SUBCASE("type properties") {
        CHECK_EQ(numeric_limits<unsigned int>::is_specialized, true);
        CHECK_EQ(numeric_limits<unsigned int>::is_signed, false);
        CHECK_EQ(numeric_limits<unsigned int>::is_integer, true);
        CHECK_EQ(numeric_limits<unsigned int>::is_exact, true);
    }

    SUBCASE("digits - platform-dependent") {
        int expected_digits = sizeof(unsigned int) * 8;
        CHECK_EQ(numeric_limits<unsigned int>::digits, expected_digits);

        int expected_digits10 = (sizeof(unsigned int) == 2) ? 4 : 9;
        CHECK_EQ(numeric_limits<unsigned int>::digits10, expected_digits10);
    }

    SUBCASE("min/max values - platform-dependent") {
        CHECK_EQ(numeric_limits<unsigned int>::min(), 0u);
        CHECK_EQ(numeric_limits<unsigned int>::lowest(), 0u);

        unsigned int max_val = numeric_limits<unsigned int>::max();
        if (sizeof(unsigned int) == 2) {
            CHECK_EQ(max_val, 65535u);
        } else if (sizeof(unsigned int) == 4) {
            CHECK_EQ(max_val, 4294967295u);
        }
    }
}

// Test long specialization (platform-dependent: 32 or 64 bits)
TEST_CASE("fl::numeric_limits<long>") {
    SUBCASE("type properties") {
        CHECK_EQ(numeric_limits<long>::is_specialized, true);
        CHECK_EQ(numeric_limits<long>::is_signed, true);
        CHECK_EQ(numeric_limits<long>::is_integer, true);
        CHECK_EQ(numeric_limits<long>::is_exact, true);
    }

    SUBCASE("digits - platform-dependent") {
        int expected_digits = (sizeof(long) * 8) - 1;
        CHECK_EQ(numeric_limits<long>::digits, expected_digits);
    }

    SUBCASE("min/max values - platform-dependent") {
        long min_val = numeric_limits<long>::min();
        long max_val = numeric_limits<long>::max();

        CHECK(min_val < 0);
        CHECK(max_val > 0);
        CHECK_EQ(numeric_limits<long>::lowest(), min_val);
    }
}

// Test unsigned long specialization (platform-dependent: 32 or 64 bits)
TEST_CASE("fl::numeric_limits<unsigned long>") {
    SUBCASE("type properties") {
        CHECK_EQ(numeric_limits<unsigned long>::is_specialized, true);
        CHECK_EQ(numeric_limits<unsigned long>::is_signed, false);
        CHECK_EQ(numeric_limits<unsigned long>::is_integer, true);
        CHECK_EQ(numeric_limits<unsigned long>::is_exact, true);
    }

    SUBCASE("digits - platform-dependent") {
        int expected_digits = sizeof(unsigned long) * 8;
        CHECK_EQ(numeric_limits<unsigned long>::digits, expected_digits);
    }

    SUBCASE("min/max values") {
        CHECK_EQ(numeric_limits<unsigned long>::min(), 0ul);
        CHECK_EQ(numeric_limits<unsigned long>::lowest(), 0ul);
        CHECK(numeric_limits<unsigned long>::max() > 0ul);
    }
}

// Test long long specialization (always 64 bits)
TEST_CASE("fl::numeric_limits<long long>") {
    SUBCASE("type properties") {
        CHECK_EQ(numeric_limits<long long>::is_specialized, true);
        CHECK_EQ(numeric_limits<long long>::is_signed, true);
        CHECK_EQ(numeric_limits<long long>::is_integer, true);
        CHECK_EQ(numeric_limits<long long>::is_exact, true);
    }

    SUBCASE("digits") {
        CHECK_EQ(numeric_limits<long long>::digits, 63);
        CHECK_EQ(numeric_limits<long long>::digits10, 19);
    }

    SUBCASE("min/max values") {
        CHECK_EQ(numeric_limits<long long>::min(), -9223372036854775807LL - 1);
        CHECK_EQ(numeric_limits<long long>::max(), 9223372036854775807LL);
        CHECK_EQ(numeric_limits<long long>::lowest(), -9223372036854775807LL - 1);
    }

    SUBCASE("constexpr evaluation") {
        static_assert(numeric_limits<long long>::digits == 63, "digits must be constexpr");
        static_assert(numeric_limits<long long>::max() == 9223372036854775807LL, "max() must be constexpr");
    }
}

// Test unsigned long long specialization (always 64 bits)
TEST_CASE("fl::numeric_limits<unsigned long long>") {
    SUBCASE("type properties") {
        CHECK_EQ(numeric_limits<unsigned long long>::is_specialized, true);
        CHECK_EQ(numeric_limits<unsigned long long>::is_signed, false);
        CHECK_EQ(numeric_limits<unsigned long long>::is_integer, true);
        CHECK_EQ(numeric_limits<unsigned long long>::is_exact, true);
    }

    SUBCASE("digits") {
        CHECK_EQ(numeric_limits<unsigned long long>::digits, 64);
        CHECK_EQ(numeric_limits<unsigned long long>::digits10, 19);
    }

    SUBCASE("min/max values") {
        CHECK_EQ(numeric_limits<unsigned long long>::min(), 0ull);
        CHECK_EQ(numeric_limits<unsigned long long>::max(), 18446744073709551615ull);
        CHECK_EQ(numeric_limits<unsigned long long>::lowest(), 0ull);
    }

    SUBCASE("constexpr evaluation") {
        static_assert(numeric_limits<unsigned long long>::digits == 64, "digits must be constexpr");
        static_assert(numeric_limits<unsigned long long>::max() == 18446744073709551615ull, "max() must be constexpr");
    }
}

// Test float specialization
TEST_CASE("fl::numeric_limits<float>") {
    SUBCASE("type properties") {
        CHECK_EQ(numeric_limits<float>::is_specialized, true);
        CHECK_EQ(numeric_limits<float>::is_signed, true);
        CHECK_EQ(numeric_limits<float>::is_integer, false);
        CHECK_EQ(numeric_limits<float>::is_exact, false);
        CHECK_EQ(numeric_limits<float>::has_infinity, true);
        CHECK_EQ(numeric_limits<float>::has_quiet_NaN, true);
        CHECK_EQ(numeric_limits<float>::has_signaling_NaN, true);
    }

    SUBCASE("digits") {
        CHECK_EQ(numeric_limits<float>::digits, 24);
        CHECK_EQ(numeric_limits<float>::digits10, 6);
        CHECK_EQ(numeric_limits<float>::max_digits10, 9);
    }

    SUBCASE("exponents") {
        CHECK_EQ(numeric_limits<float>::max_exponent, 128);
        CHECK_EQ(numeric_limits<float>::max_exponent10, 38);
        CHECK_EQ(numeric_limits<float>::min_exponent, -125);
        CHECK_EQ(numeric_limits<float>::min_exponent10, -37);
    }

    SUBCASE("min/max/lowest values") {
        float min_val = numeric_limits<float>::min();
        float max_val = numeric_limits<float>::max();
        float lowest_val = numeric_limits<float>::lowest();

        // min() returns smallest positive normalized value
        CHECK(min_val > 0.0f);
        CHECK(min_val < 1.0f);

        // max() returns largest finite value
        CHECK(max_val > 1.0e30f);

        // lowest() returns most negative finite value
        CHECK(lowest_val < 0.0f);
        CHECK(lowest_val < -1.0e30f);
        CHECK_EQ(lowest_val, -max_val);
    }

    SUBCASE("epsilon and round_error") {
        float eps = numeric_limits<float>::epsilon();
        float round_err = numeric_limits<float>::round_error();

        CHECK(eps > 0.0f);
        CHECK(eps < 1.0f);
        CHECK_CLOSE(round_err, 0.5f, 0.0f);

        // Epsilon test: 1 + epsilon != 1
        CHECK(1.0f + eps != 1.0f);
        CHECK(1.0f + eps/2.0f == 1.0f);  // Half epsilon should round to 1
    }

    SUBCASE("special values") {
        float inf = numeric_limits<float>::infinity();
        float qnan = numeric_limits<float>::quiet_NaN();
        float snan = numeric_limits<float>::signaling_NaN();
        float denorm = numeric_limits<float>::denorm_min();

        // Test infinity
        CHECK(inf > numeric_limits<float>::max());
        CHECK(inf == inf);  // Infinity equals itself

        // Test NaN (NaN != NaN by IEEE 754)
        CHECK(qnan != qnan);
        CHECK(snan != snan);

        // Test denorm_min (smallest subnormal positive value)
        CHECK(denorm > 0.0f);
        CHECK(denorm < numeric_limits<float>::min());
    }

    SUBCASE("constexpr evaluation") {
        constexpr float min_val = numeric_limits<float>::min();
        constexpr float max_val = numeric_limits<float>::max();
        constexpr float eps = numeric_limits<float>::epsilon();

        static_assert(min_val > 0.0f, "min() must be constexpr");
        static_assert(max_val > 0.0f, "max() must be constexpr");
        static_assert(eps > 0.0f, "epsilon() must be constexpr");
    }
}

// Test double specialization
TEST_CASE("fl::numeric_limits<double>") {
    SUBCASE("type properties") {
        CHECK_EQ(numeric_limits<double>::is_specialized, true);
        CHECK_EQ(numeric_limits<double>::is_signed, true);
        CHECK_EQ(numeric_limits<double>::is_integer, false);
        CHECK_EQ(numeric_limits<double>::is_exact, false);
        CHECK_EQ(numeric_limits<double>::has_infinity, true);
        CHECK_EQ(numeric_limits<double>::has_quiet_NaN, true);
        CHECK_EQ(numeric_limits<double>::has_signaling_NaN, true);
    }

    SUBCASE("digits") {
        CHECK_EQ(numeric_limits<double>::digits, 53);
        CHECK_EQ(numeric_limits<double>::digits10, 15);
        CHECK_EQ(numeric_limits<double>::max_digits10, 17);
    }

    SUBCASE("exponents") {
        CHECK_EQ(numeric_limits<double>::max_exponent, 1024);
        CHECK_EQ(numeric_limits<double>::max_exponent10, 308);
        CHECK_EQ(numeric_limits<double>::min_exponent, -1021);
        CHECK_EQ(numeric_limits<double>::min_exponent10, -307);
    }

    SUBCASE("min/max/lowest values") {
        double min_val = numeric_limits<double>::min();
        double max_val = numeric_limits<double>::max();
        double lowest_val = numeric_limits<double>::lowest();

        // min() returns smallest positive normalized value
        CHECK(min_val > 0.0);
        CHECK(min_val < 1.0);

        // max() returns largest finite value
        CHECK(max_val > 1.0e100);

        // lowest() returns most negative finite value
        CHECK(lowest_val < 0.0);
        CHECK(lowest_val < -1.0e100);
        CHECK_EQ(lowest_val, -max_val);
    }

    SUBCASE("epsilon and round_error") {
        double eps = numeric_limits<double>::epsilon();
        double round_err = numeric_limits<double>::round_error();

        CHECK(eps > 0.0);
        CHECK(eps < 1.0);
        CHECK(eps < numeric_limits<float>::epsilon());  // double epsilon is smaller
        CHECK_CLOSE(round_err, 0.5, 0.0);

        // Epsilon test: 1 + epsilon != 1
        CHECK(1.0 + eps != 1.0);
        CHECK(1.0 + eps/2.0 == 1.0);  // Half epsilon should round to 1
    }

    SUBCASE("special values") {
        double inf = numeric_limits<double>::infinity();
        double qnan = numeric_limits<double>::quiet_NaN();
        double snan = numeric_limits<double>::signaling_NaN();
        double denorm = numeric_limits<double>::denorm_min();

        // Test infinity
        CHECK(inf > numeric_limits<double>::max());
        CHECK(inf == inf);  // Infinity equals itself

        // Test NaN (NaN != NaN by IEEE 754)
        CHECK(qnan != qnan);
        CHECK(snan != snan);

        // Test denorm_min (smallest subnormal positive value)
        CHECK(denorm > 0.0);
        CHECK(denorm < numeric_limits<double>::min());
    }

    SUBCASE("constexpr evaluation") {
        constexpr double min_val = numeric_limits<double>::min();
        constexpr double max_val = numeric_limits<double>::max();
        constexpr double eps = numeric_limits<double>::epsilon();

        static_assert(min_val > 0.0, "min() must be constexpr");
        static_assert(max_val > 0.0, "max() must be constexpr");
        static_assert(eps > 0.0, "epsilon() must be constexpr");
    }
}

// Test comparison between float and double precision
TEST_CASE("fl::numeric_limits - float vs double precision") {
    SUBCASE("epsilon comparison") {
        // double should have higher precision (smaller epsilon)
        CHECK(numeric_limits<double>::epsilon() < numeric_limits<float>::epsilon());
    }

    SUBCASE("digits comparison") {
        // double should have more mantissa bits
        CHECK(numeric_limits<double>::digits > numeric_limits<float>::digits);
        CHECK(numeric_limits<double>::digits10 > numeric_limits<float>::digits10);
    }

    SUBCASE("range comparison") {
        // double should have larger exponent range
        CHECK(numeric_limits<double>::max_exponent > numeric_limits<float>::max_exponent);
        CHECK(numeric_limits<double>::min_exponent < numeric_limits<float>::min_exponent);
    }
}

// Test that digits are computed correctly (tests helper functions indirectly)
TEST_CASE("fl::numeric_limits - digits computation") {
    SUBCASE("unsigned types have full bit width") {
        // Unsigned types use all bits for value
        CHECK_EQ(numeric_limits<unsigned char>::digits, 8);
        CHECK_EQ(numeric_limits<unsigned short>::digits, 16);
        CHECK_EQ(numeric_limits<unsigned int>::digits, sizeof(unsigned int) * 8);
        CHECK_EQ(numeric_limits<unsigned long>::digits, sizeof(unsigned long) * 8);
        CHECK_EQ(numeric_limits<unsigned long long>::digits, 64);
    }

    SUBCASE("signed types lose one bit for sign") {
        // Signed types use n-1 bits for value (1 bit for sign)
        CHECK_EQ(numeric_limits<signed char>::digits, 7);
        CHECK_EQ(numeric_limits<short>::digits, 15);
        CHECK_EQ(numeric_limits<int>::digits, sizeof(int) * 8 - 1);
        CHECK_EQ(numeric_limits<long>::digits, sizeof(long) * 8 - 1);
        CHECK_EQ(numeric_limits<long long>::digits, 63);
    }
}

// Test that limits work correctly in generic contexts
TEST_CASE("fl::numeric_limits - generic context usage") {
    SUBCASE("int range checking") {
        int value = 50;
        int min_allowed = 0;
        int max_allowed = 100;

        int type_min = numeric_limits<int>::min();
        int type_max = numeric_limits<int>::max();

        // Ensure requested range is within type's range
        CHECK(min_allowed >= type_min);
        CHECK(max_allowed <= type_max);

        // Test in range
        CHECK(value >= min_allowed);
        CHECK(value <= max_allowed);

        // Test out of range
        int value2 = 150;
        bool in_range = (value2 >= min_allowed && value2 <= max_allowed);
        CHECK_FALSE(in_range);
    }

    SUBCASE("float range checking") {
        float value = 0.5f;
        float min_allowed = 0.0f;
        float max_allowed = 1.0f;

        // For floating-point, min() returns smallest positive normalized value
        // Use lowest() for most negative value
        float type_lowest = numeric_limits<float>::lowest();
        float type_max = numeric_limits<float>::max();

        // Ensure requested range is within type's range
        CHECK(min_allowed >= type_lowest);
        CHECK(max_allowed <= type_max);

        // Test in range
        CHECK(value >= min_allowed);
        CHECK(value <= max_allowed);

        // Test out of range
        float value2 = 1.5f;
        bool in_range = (value2 >= min_allowed && value2 <= max_allowed);
        CHECK_FALSE(in_range);
    }
}
