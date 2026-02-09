#include "test.h"
#include "fl/stl/limits.h"
#include "test.h"

using namespace fl;

// Test primary template (unspecialized type)
FL_TEST_CASE("fl::numeric_limits - primary template") {
    FL_SUBCASE("struct without specialization") {
        struct CustomType {};

        FL_CHECK_EQ(numeric_limits<CustomType>::is_specialized, false);
        FL_CHECK_EQ(numeric_limits<CustomType>::is_signed, false);
        FL_CHECK_EQ(numeric_limits<CustomType>::is_integer, false);
        FL_CHECK_EQ(numeric_limits<CustomType>::is_exact, false);
        FL_CHECK_EQ(numeric_limits<CustomType>::has_infinity, false);
        FL_CHECK_EQ(numeric_limits<CustomType>::has_quiet_NaN, false);
        FL_CHECK_EQ(numeric_limits<CustomType>::has_signaling_NaN, false);
        FL_CHECK_EQ(numeric_limits<CustomType>::digits, 0);
        FL_CHECK_EQ(numeric_limits<CustomType>::digits10, 0);
    }
}

// Test bool specialization
FL_TEST_CASE("fl::numeric_limits<bool>") {
    FL_SUBCASE("type properties") {
        FL_CHECK_EQ(numeric_limits<bool>::is_specialized, true);
        FL_CHECK_EQ(numeric_limits<bool>::is_signed, false);
        FL_CHECK_EQ(numeric_limits<bool>::is_integer, true);
        FL_CHECK_EQ(numeric_limits<bool>::is_exact, true);
        FL_CHECK_EQ(numeric_limits<bool>::has_infinity, false);
        FL_CHECK_EQ(numeric_limits<bool>::has_quiet_NaN, false);
        FL_CHECK_EQ(numeric_limits<bool>::has_signaling_NaN, false);
    }

    FL_SUBCASE("digits") {
        FL_CHECK_EQ(numeric_limits<bool>::digits, 1);
        FL_CHECK_EQ(numeric_limits<bool>::digits10, 0);
    }

    FL_SUBCASE("min/max values") {
        FL_CHECK_EQ(numeric_limits<bool>::min(), false);
        FL_CHECK_EQ(numeric_limits<bool>::max(), true);
        FL_CHECK_EQ(numeric_limits<bool>::lowest(), false);
    }

    FL_SUBCASE("constexpr evaluation") {
        static_assert(numeric_limits<bool>::min() == false, "min() must be constexpr");
        static_assert(numeric_limits<bool>::max() == true, "max() must be constexpr");
        static_assert(numeric_limits<bool>::is_specialized, "is_specialized must be constexpr");
    }
}

// Test char specialization
FL_TEST_CASE("fl::numeric_limits<char>") {
    FL_SUBCASE("type properties") {
        FL_CHECK_EQ(numeric_limits<char>::is_specialized, true);
        FL_CHECK_EQ(numeric_limits<char>::is_integer, true);
        FL_CHECK_EQ(numeric_limits<char>::is_exact, true);
        FL_CHECK_EQ(numeric_limits<char>::has_infinity, false);
        FL_CHECK_EQ(numeric_limits<char>::has_quiet_NaN, false);
        FL_CHECK_EQ(numeric_limits<char>::has_signaling_NaN, false);

        // char signedness is platform-dependent
        bool expected_signed = (char(-1) < char(0));
        FL_CHECK_EQ(numeric_limits<char>::is_signed, expected_signed);
    }

    FL_SUBCASE("digits") {
        // char is 8 bits
        int expected_digits = 8 - (numeric_limits<char>::is_signed ? 1 : 0);
        FL_CHECK_EQ(numeric_limits<char>::digits, expected_digits);
        FL_CHECK_EQ(numeric_limits<char>::digits10, 2);
    }

    FL_SUBCASE("min/max values") {
        char min_val = numeric_limits<char>::min();
        char max_val = numeric_limits<char>::max();

        // Verify min < max (or min == 0 for unsigned)
        bool valid_range = min_val < max_val || (min_val == 0 && !numeric_limits<char>::is_signed);
        FL_CHECK(valid_range);

        // Verify lowest() == min() for integers
        FL_CHECK_EQ(numeric_limits<char>::lowest(), min_val);

        // Verify epsilon and round_error are zero for integers
        FL_CHECK_EQ(numeric_limits<char>::epsilon(), 0);
        FL_CHECK_EQ(numeric_limits<char>::round_error(), 0);
    }
}

// Test signed char specialization
FL_TEST_CASE("fl::numeric_limits<signed char>") {
    FL_SUBCASE("type properties") {
        FL_CHECK_EQ(numeric_limits<signed char>::is_specialized, true);
        FL_CHECK_EQ(numeric_limits<signed char>::is_signed, true);
        FL_CHECK_EQ(numeric_limits<signed char>::is_integer, true);
        FL_CHECK_EQ(numeric_limits<signed char>::is_exact, true);
    }

    FL_SUBCASE("digits") {
        FL_CHECK_EQ(numeric_limits<signed char>::digits, 7);
        FL_CHECK_EQ(numeric_limits<signed char>::digits10, 2);
    }

    FL_SUBCASE("min/max values") {
        FL_CHECK_EQ(numeric_limits<signed char>::min(), -128);
        FL_CHECK_EQ(numeric_limits<signed char>::max(), 127);
        FL_CHECK_EQ(numeric_limits<signed char>::lowest(), -128);
    }

    FL_SUBCASE("constexpr evaluation") {
        static_assert(numeric_limits<signed char>::min() == -128, "min() must be constexpr");
        static_assert(numeric_limits<signed char>::max() == 127, "max() must be constexpr");
    }
}

// Test unsigned char specialization
FL_TEST_CASE("fl::numeric_limits<unsigned char>") {
    FL_SUBCASE("type properties") {
        FL_CHECK_EQ(numeric_limits<unsigned char>::is_specialized, true);
        FL_CHECK_EQ(numeric_limits<unsigned char>::is_signed, false);
        FL_CHECK_EQ(numeric_limits<unsigned char>::is_integer, true);
        FL_CHECK_EQ(numeric_limits<unsigned char>::is_exact, true);
    }

    FL_SUBCASE("digits") {
        FL_CHECK_EQ(numeric_limits<unsigned char>::digits, 8);
        FL_CHECK_EQ(numeric_limits<unsigned char>::digits10, 2);
    }

    FL_SUBCASE("min/max values") {
        FL_CHECK_EQ(numeric_limits<unsigned char>::min(), 0);
        FL_CHECK_EQ(numeric_limits<unsigned char>::max(), 255);
        FL_CHECK_EQ(numeric_limits<unsigned char>::lowest(), 0);
    }

    FL_SUBCASE("constexpr evaluation") {
        static_assert(numeric_limits<unsigned char>::min() == 0, "min() must be constexpr");
        static_assert(numeric_limits<unsigned char>::max() == 255, "max() must be constexpr");
    }
}

// Test short specialization
FL_TEST_CASE("fl::numeric_limits<short>") {
    FL_SUBCASE("type properties") {
        FL_CHECK_EQ(numeric_limits<short>::is_specialized, true);
        FL_CHECK_EQ(numeric_limits<short>::is_signed, true);
        FL_CHECK_EQ(numeric_limits<short>::is_integer, true);
        FL_CHECK_EQ(numeric_limits<short>::is_exact, true);
    }

    FL_SUBCASE("digits") {
        FL_CHECK_EQ(numeric_limits<short>::digits, 15);
        FL_CHECK_EQ(numeric_limits<short>::digits10, 4);
    }

    FL_SUBCASE("min/max values") {
        FL_CHECK_EQ(numeric_limits<short>::min(), -32768);
        FL_CHECK_EQ(numeric_limits<short>::max(), 32767);
        FL_CHECK_EQ(numeric_limits<short>::lowest(), -32768);
    }

    FL_SUBCASE("constexpr evaluation") {
        static_assert(numeric_limits<short>::min() == -32768, "min() must be constexpr");
        static_assert(numeric_limits<short>::max() == 32767, "max() must be constexpr");
    }
}

// Test unsigned short specialization
FL_TEST_CASE("fl::numeric_limits<unsigned short>") {
    FL_SUBCASE("type properties") {
        FL_CHECK_EQ(numeric_limits<unsigned short>::is_specialized, true);
        FL_CHECK_EQ(numeric_limits<unsigned short>::is_signed, false);
        FL_CHECK_EQ(numeric_limits<unsigned short>::is_integer, true);
        FL_CHECK_EQ(numeric_limits<unsigned short>::is_exact, true);
    }

    FL_SUBCASE("digits") {
        FL_CHECK_EQ(numeric_limits<unsigned short>::digits, 16);
        FL_CHECK_EQ(numeric_limits<unsigned short>::digits10, 4);
    }

    FL_SUBCASE("min/max values") {
        FL_CHECK_EQ(numeric_limits<unsigned short>::min(), 0);
        FL_CHECK_EQ(numeric_limits<unsigned short>::max(), 65535);
        FL_CHECK_EQ(numeric_limits<unsigned short>::lowest(), 0);
    }

    FL_SUBCASE("constexpr evaluation") {
        static_assert(numeric_limits<unsigned short>::min() == 0, "min() must be constexpr");
        static_assert(numeric_limits<unsigned short>::max() == 65535, "max() must be constexpr");
    }
}

// Test int specialization (platform-dependent: 16 or 32 bits)
FL_TEST_CASE("fl::numeric_limits<int>") {
    FL_SUBCASE("type properties") {
        FL_CHECK_EQ(numeric_limits<int>::is_specialized, true);
        FL_CHECK_EQ(numeric_limits<int>::is_signed, true);
        FL_CHECK_EQ(numeric_limits<int>::is_integer, true);
        FL_CHECK_EQ(numeric_limits<int>::is_exact, true);
    }

    FL_SUBCASE("digits - platform-dependent") {
        // int can be 16-bit (AVR) or 32-bit (most platforms)
        int expected_digits = (sizeof(int) * 8) - 1;
        FL_CHECK_EQ(numeric_limits<int>::digits, expected_digits);

        int expected_digits10 = (sizeof(int) == 2) ? 4 : 9;
        FL_CHECK_EQ(numeric_limits<int>::digits10, expected_digits10);
    }

    FL_SUBCASE("min/max values - platform-dependent") {
        int min_val = numeric_limits<int>::min();
        int max_val = numeric_limits<int>::max();

        // Verify min is negative and max is positive
        FL_CHECK(min_val < 0);
        FL_CHECK(max_val > 0);

        // Verify min/max are consistent with size
        if (sizeof(int) == 2) {
            FL_CHECK_EQ(min_val, -32768);
            FL_CHECK_EQ(max_val, 32767);
        } else if (sizeof(int) == 4) {
            FL_CHECK_EQ(min_val, -2147483647 - 1);
            FL_CHECK_EQ(max_val, 2147483647);
        }

        FL_CHECK_EQ(numeric_limits<int>::lowest(), min_val);
    }

    FL_SUBCASE("constexpr evaluation") {
        constexpr int min_val = numeric_limits<int>::min();
        constexpr int max_val = numeric_limits<int>::max();
        FL_CHECK(min_val < 0);
        FL_CHECK(max_val > 0);
    }
}

// Test unsigned int specialization (platform-dependent: 16 or 32 bits)
FL_TEST_CASE("fl::numeric_limits<unsigned int>") {
    FL_SUBCASE("type properties") {
        FL_CHECK_EQ(numeric_limits<unsigned int>::is_specialized, true);
        FL_CHECK_EQ(numeric_limits<unsigned int>::is_signed, false);
        FL_CHECK_EQ(numeric_limits<unsigned int>::is_integer, true);
        FL_CHECK_EQ(numeric_limits<unsigned int>::is_exact, true);
    }

    FL_SUBCASE("digits - platform-dependent") {
        int expected_digits = sizeof(unsigned int) * 8;
        FL_CHECK_EQ(numeric_limits<unsigned int>::digits, expected_digits);

        int expected_digits10 = (sizeof(unsigned int) == 2) ? 4 : 9;
        FL_CHECK_EQ(numeric_limits<unsigned int>::digits10, expected_digits10);
    }

    FL_SUBCASE("min/max values - platform-dependent") {
        FL_CHECK_EQ(numeric_limits<unsigned int>::min(), 0u);
        FL_CHECK_EQ(numeric_limits<unsigned int>::lowest(), 0u);

        unsigned int max_val = numeric_limits<unsigned int>::max();
        if (sizeof(unsigned int) == 2) {
            FL_CHECK_EQ(max_val, 65535u);
        } else if (sizeof(unsigned int) == 4) {
            FL_CHECK_EQ(max_val, 4294967295u);
        }
    }
}

// Test long specialization (platform-dependent: 32 or 64 bits)
FL_TEST_CASE("fl::numeric_limits<long>") {
    FL_SUBCASE("type properties") {
        FL_CHECK_EQ(numeric_limits<long>::is_specialized, true);
        FL_CHECK_EQ(numeric_limits<long>::is_signed, true);
        FL_CHECK_EQ(numeric_limits<long>::is_integer, true);
        FL_CHECK_EQ(numeric_limits<long>::is_exact, true);
    }

    FL_SUBCASE("digits - platform-dependent") {
        int expected_digits = (sizeof(long) * 8) - 1;
        FL_CHECK_EQ(numeric_limits<long>::digits, expected_digits);
    }

    FL_SUBCASE("min/max values - platform-dependent") {
        long min_val = numeric_limits<long>::min();
        long max_val = numeric_limits<long>::max();

        FL_CHECK(min_val < 0);
        FL_CHECK(max_val > 0);
        FL_CHECK_EQ(numeric_limits<long>::lowest(), min_val);
    }
}

// Test unsigned long specialization (platform-dependent: 32 or 64 bits)
FL_TEST_CASE("fl::numeric_limits<unsigned long>") {
    FL_SUBCASE("type properties") {
        FL_CHECK_EQ(numeric_limits<unsigned long>::is_specialized, true);
        FL_CHECK_EQ(numeric_limits<unsigned long>::is_signed, false);
        FL_CHECK_EQ(numeric_limits<unsigned long>::is_integer, true);
        FL_CHECK_EQ(numeric_limits<unsigned long>::is_exact, true);
    }

    FL_SUBCASE("digits - platform-dependent") {
        int expected_digits = sizeof(unsigned long) * 8;
        FL_CHECK_EQ(numeric_limits<unsigned long>::digits, expected_digits);
    }

    FL_SUBCASE("min/max values") {
        FL_CHECK_EQ(numeric_limits<unsigned long>::min(), 0ul);
        FL_CHECK_EQ(numeric_limits<unsigned long>::lowest(), 0ul);
        FL_CHECK(numeric_limits<unsigned long>::max() > 0ul);
    }
}

// Test long long specialization (always 64 bits)
FL_TEST_CASE("fl::numeric_limits<long long>") {
    FL_SUBCASE("type properties") {
        FL_CHECK_EQ(numeric_limits<long long>::is_specialized, true);
        FL_CHECK_EQ(numeric_limits<long long>::is_signed, true);
        FL_CHECK_EQ(numeric_limits<long long>::is_integer, true);
        FL_CHECK_EQ(numeric_limits<long long>::is_exact, true);
    }

    FL_SUBCASE("digits") {
        FL_CHECK_EQ(numeric_limits<long long>::digits, 63);
        FL_CHECK_EQ(numeric_limits<long long>::digits10, 19);
    }

    FL_SUBCASE("min/max values") {
        FL_CHECK_EQ(numeric_limits<long long>::min(), -9223372036854775807LL - 1);
        FL_CHECK_EQ(numeric_limits<long long>::max(), 9223372036854775807LL);
        FL_CHECK_EQ(numeric_limits<long long>::lowest(), -9223372036854775807LL - 1);
    }

    FL_SUBCASE("constexpr evaluation") {
        static_assert(numeric_limits<long long>::digits == 63, "digits must be constexpr");
        static_assert(numeric_limits<long long>::max() == 9223372036854775807LL, "max() must be constexpr");
    }
}

// Test unsigned long long specialization (always 64 bits)
FL_TEST_CASE("fl::numeric_limits<unsigned long long>") {
    FL_SUBCASE("type properties") {
        FL_CHECK_EQ(numeric_limits<unsigned long long>::is_specialized, true);
        FL_CHECK_EQ(numeric_limits<unsigned long long>::is_signed, false);
        FL_CHECK_EQ(numeric_limits<unsigned long long>::is_integer, true);
        FL_CHECK_EQ(numeric_limits<unsigned long long>::is_exact, true);
    }

    FL_SUBCASE("digits") {
        FL_CHECK_EQ(numeric_limits<unsigned long long>::digits, 64);
        FL_CHECK_EQ(numeric_limits<unsigned long long>::digits10, 19);
    }

    FL_SUBCASE("min/max values") {
        FL_CHECK_EQ(numeric_limits<unsigned long long>::min(), 0ull);
        FL_CHECK_EQ(numeric_limits<unsigned long long>::max(), 18446744073709551615ull);
        FL_CHECK_EQ(numeric_limits<unsigned long long>::lowest(), 0ull);
    }

    FL_SUBCASE("constexpr evaluation") {
        static_assert(numeric_limits<unsigned long long>::digits == 64, "digits must be constexpr");
        static_assert(numeric_limits<unsigned long long>::max() == 18446744073709551615ull, "max() must be constexpr");
    }
}

// Test float specialization
FL_TEST_CASE("fl::numeric_limits<float>") {
    FL_SUBCASE("type properties") {
        FL_CHECK_EQ(numeric_limits<float>::is_specialized, true);
        FL_CHECK_EQ(numeric_limits<float>::is_signed, true);
        FL_CHECK_EQ(numeric_limits<float>::is_integer, false);
        FL_CHECK_EQ(numeric_limits<float>::is_exact, false);
        FL_CHECK_EQ(numeric_limits<float>::has_infinity, true);
        FL_CHECK_EQ(numeric_limits<float>::has_quiet_NaN, true);
        FL_CHECK_EQ(numeric_limits<float>::has_signaling_NaN, true);
    }

    FL_SUBCASE("digits") {
        FL_CHECK_EQ(numeric_limits<float>::digits, 24);
        FL_CHECK_EQ(numeric_limits<float>::digits10, 6);
        FL_CHECK_EQ(numeric_limits<float>::max_digits10, 9);
    }

    FL_SUBCASE("exponents") {
        FL_CHECK_EQ(numeric_limits<float>::max_exponent, 128);
        FL_CHECK_EQ(numeric_limits<float>::max_exponent10, 38);
        FL_CHECK_EQ(numeric_limits<float>::min_exponent, -125);
        FL_CHECK_EQ(numeric_limits<float>::min_exponent10, -37);
    }

    FL_SUBCASE("min/max/lowest values") {
        float min_val = numeric_limits<float>::min();
        float max_val = numeric_limits<float>::max();
        float lowest_val = numeric_limits<float>::lowest();

        // min() returns smallest positive normalized value
        FL_CHECK(min_val > 0.0f);
        FL_CHECK(min_val < 1.0f);

        // max() returns largest finite value
        FL_CHECK(max_val > 1.0e30f);

        // lowest() returns most negative finite value
        FL_CHECK(lowest_val < 0.0f);
        FL_CHECK(lowest_val < -1.0e30f);
        FL_CHECK_EQ(lowest_val, -max_val);
    }

    FL_SUBCASE("epsilon and round_error") {
        float eps = numeric_limits<float>::epsilon();
        float round_err = numeric_limits<float>::round_error();

        FL_CHECK(eps > 0.0f);
        FL_CHECK(eps < 1.0f);
        FL_CHECK_CLOSE(round_err, 0.5f, 0.0f);

        // Epsilon test: 1 + epsilon != 1
        FL_CHECK(1.0f + eps != 1.0f);
        FL_CHECK(1.0f + eps/2.0f == 1.0f);  // Half epsilon should round to 1
    }

    FL_SUBCASE("special values") {
        float inf = numeric_limits<float>::infinity();
        float qnan = numeric_limits<float>::quiet_NaN();
        float snan = numeric_limits<float>::signaling_NaN();
        float denorm = numeric_limits<float>::denorm_min();

        // Test infinity
        FL_CHECK(inf > numeric_limits<float>::max());
        FL_CHECK(inf == inf);  // Infinity equals itself

        // Test NaN (NaN != NaN by IEEE 754)
        FL_CHECK(qnan != qnan);
        FL_CHECK(snan != snan);

        // Test denorm_min (smallest subnormal positive value)
        FL_CHECK(denorm > 0.0f);
        FL_CHECK(denorm < numeric_limits<float>::min());
    }

    FL_SUBCASE("constexpr evaluation") {
        constexpr float min_val = numeric_limits<float>::min();
        constexpr float max_val = numeric_limits<float>::max();
        constexpr float eps = numeric_limits<float>::epsilon();

        static_assert(min_val > 0.0f, "min() must be constexpr");
        static_assert(max_val > 0.0f, "max() must be constexpr");
        static_assert(eps > 0.0f, "epsilon() must be constexpr");
    }
}

// Test double specialization
FL_TEST_CASE("fl::numeric_limits<double>") {
    FL_SUBCASE("type properties") {
        FL_CHECK_EQ(numeric_limits<double>::is_specialized, true);
        FL_CHECK_EQ(numeric_limits<double>::is_signed, true);
        FL_CHECK_EQ(numeric_limits<double>::is_integer, false);
        FL_CHECK_EQ(numeric_limits<double>::is_exact, false);
        FL_CHECK_EQ(numeric_limits<double>::has_infinity, true);
        FL_CHECK_EQ(numeric_limits<double>::has_quiet_NaN, true);
        FL_CHECK_EQ(numeric_limits<double>::has_signaling_NaN, true);
    }

    FL_SUBCASE("digits") {
        FL_CHECK_EQ(numeric_limits<double>::digits, 53);
        FL_CHECK_EQ(numeric_limits<double>::digits10, 15);
        FL_CHECK_EQ(numeric_limits<double>::max_digits10, 17);
    }

    FL_SUBCASE("exponents") {
        FL_CHECK_EQ(numeric_limits<double>::max_exponent, 1024);
        FL_CHECK_EQ(numeric_limits<double>::max_exponent10, 308);
        FL_CHECK_EQ(numeric_limits<double>::min_exponent, -1021);
        FL_CHECK_EQ(numeric_limits<double>::min_exponent10, -307);
    }

    FL_SUBCASE("min/max/lowest values") {
        double min_val = numeric_limits<double>::min();
        double max_val = numeric_limits<double>::max();
        double lowest_val = numeric_limits<double>::lowest();

        // min() returns smallest positive normalized value
        FL_CHECK(min_val > 0.0);
        FL_CHECK(min_val < 1.0);

        // max() returns largest finite value
        FL_CHECK(max_val > 1.0e100);

        // lowest() returns most negative finite value
        FL_CHECK(lowest_val < 0.0);
        FL_CHECK(lowest_val < -1.0e100);
        FL_CHECK_EQ(lowest_val, -max_val);
    }

    FL_SUBCASE("epsilon and round_error") {
        double eps = numeric_limits<double>::epsilon();
        double round_err = numeric_limits<double>::round_error();

        FL_CHECK(eps > 0.0);
        FL_CHECK(eps < 1.0);
        FL_CHECK(eps < numeric_limits<float>::epsilon());  // double epsilon is smaller
        FL_CHECK_CLOSE(round_err, 0.5, 0.0);

        // Epsilon test: 1 + epsilon != 1
        FL_CHECK(1.0 + eps != 1.0);
        FL_CHECK(1.0 + eps/2.0 == 1.0);  // Half epsilon should round to 1
    }

    FL_SUBCASE("special values") {
        double inf = numeric_limits<double>::infinity();
        double qnan = numeric_limits<double>::quiet_NaN();
        double snan = numeric_limits<double>::signaling_NaN();
        double denorm = numeric_limits<double>::denorm_min();

        // Test infinity
        FL_CHECK(inf > numeric_limits<double>::max());
        FL_CHECK(inf == inf);  // Infinity equals itself

        // Test NaN (NaN != NaN by IEEE 754)
        FL_CHECK(qnan != qnan);
        FL_CHECK(snan != snan);

        // Test denorm_min (smallest subnormal positive value)
        FL_CHECK(denorm > 0.0);
        FL_CHECK(denorm < numeric_limits<double>::min());
    }

    FL_SUBCASE("constexpr evaluation") {
        constexpr double min_val = numeric_limits<double>::min();
        constexpr double max_val = numeric_limits<double>::max();
        constexpr double eps = numeric_limits<double>::epsilon();

        static_assert(min_val > 0.0, "min() must be constexpr");
        static_assert(max_val > 0.0, "max() must be constexpr");
        static_assert(eps > 0.0, "epsilon() must be constexpr");
    }
}

// Test comparison between float and double precision
FL_TEST_CASE("fl::numeric_limits - float vs double precision") {
    FL_SUBCASE("epsilon comparison") {
        // double should have higher precision (smaller epsilon)
        FL_CHECK(numeric_limits<double>::epsilon() < numeric_limits<float>::epsilon());
    }

    FL_SUBCASE("digits comparison") {
        // double should have more mantissa bits
        FL_CHECK(numeric_limits<double>::digits > numeric_limits<float>::digits);
        FL_CHECK(numeric_limits<double>::digits10 > numeric_limits<float>::digits10);
    }

    FL_SUBCASE("range comparison") {
        // double should have larger exponent range
        FL_CHECK(numeric_limits<double>::max_exponent > numeric_limits<float>::max_exponent);
        FL_CHECK(numeric_limits<double>::min_exponent < numeric_limits<float>::min_exponent);
    }
}

// Test that digits are computed correctly (tests helper functions indirectly)
FL_TEST_CASE("fl::numeric_limits - digits computation") {
    FL_SUBCASE("unsigned types have full bit width") {
        // Unsigned types use all bits for value
        FL_CHECK_EQ(numeric_limits<unsigned char>::digits, 8);
        FL_CHECK_EQ(numeric_limits<unsigned short>::digits, 16);
        FL_CHECK_EQ(numeric_limits<unsigned int>::digits, sizeof(unsigned int) * 8);
        FL_CHECK_EQ(numeric_limits<unsigned long>::digits, sizeof(unsigned long) * 8);
        FL_CHECK_EQ(numeric_limits<unsigned long long>::digits, 64);
    }

    FL_SUBCASE("signed types lose one bit for sign") {
        // Signed types use n-1 bits for value (1 bit for sign)
        FL_CHECK_EQ(numeric_limits<signed char>::digits, 7);
        FL_CHECK_EQ(numeric_limits<short>::digits, 15);
        FL_CHECK_EQ(numeric_limits<int>::digits, sizeof(int) * 8 - 1);
        FL_CHECK_EQ(numeric_limits<long>::digits, sizeof(long) * 8 - 1);
        FL_CHECK_EQ(numeric_limits<long long>::digits, 63);
    }
}

// Test that limits work correctly in generic contexts
FL_TEST_CASE("fl::numeric_limits - generic context usage") {
    FL_SUBCASE("int range checking") {
        int value = 50;
        int min_allowed = 0;
        int max_allowed = 100;

        int type_min = numeric_limits<int>::min();
        int type_max = numeric_limits<int>::max();

        // Ensure requested range is within type's range
        FL_CHECK(min_allowed >= type_min);
        FL_CHECK(max_allowed <= type_max);

        // Test in range
        FL_CHECK(value >= min_allowed);
        FL_CHECK(value <= max_allowed);

        // Test out of range
        int value2 = 150;
        bool in_range = (value2 >= min_allowed && value2 <= max_allowed);
        FL_CHECK_FALSE(in_range);
    }

    FL_SUBCASE("float range checking") {
        float value = 0.5f;
        float min_allowed = 0.0f;
        float max_allowed = 1.0f;

        // For floating-point, min() returns smallest positive normalized value
        // Use lowest() for most negative value
        float type_lowest = numeric_limits<float>::lowest();
        float type_max = numeric_limits<float>::max();

        // Ensure requested range is within type's range
        FL_CHECK(min_allowed >= type_lowest);
        FL_CHECK(max_allowed <= type_max);

        // Test in range
        FL_CHECK(value >= min_allowed);
        FL_CHECK(value <= max_allowed);

        // Test out of range
        float value2 = 1.5f;
        bool in_range = (value2 >= min_allowed && value2 <= max_allowed);
        FL_CHECK_FALSE(in_range);
    }
}
