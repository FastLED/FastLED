#include "doctest.h"
#include "fl/stl/limits.h"

using namespace fl;

TEST_CASE("fl::numeric_limits<int>") {
    SUBCASE("min returns minimum int value") {
        constexpr int min_val = numeric_limits<int>::min();
        // Should be -2^31 for 32-bit int
        CHECK_EQ(min_val, -2147483647 - 1);

        // Compile-time check
        static_assert(numeric_limits<int>::min() < 0, "int min should be negative");
    }

    SUBCASE("max returns maximum int value") {
        constexpr int max_val = numeric_limits<int>::max();
        // Should be 2^31 - 1 for 32-bit int
        CHECK_EQ(max_val, 2147483647);

        // Compile-time check
        static_assert(numeric_limits<int>::max() > 0, "int max should be positive");
    }

    SUBCASE("min is less than max") {
        static_assert(numeric_limits<int>::min() < numeric_limits<int>::max(),
                      "min should be less than max");
    }
}

TEST_CASE("fl::numeric_limits<unsigned int>") {
    SUBCASE("min returns 0") {
        constexpr unsigned int min_val = numeric_limits<unsigned int>::min();
        CHECK_EQ(min_val, 0u);

        static_assert(numeric_limits<unsigned int>::min() == 0,
                      "unsigned int min should be 0");
    }

    SUBCASE("max returns maximum unsigned int value") {
        constexpr unsigned int max_val = numeric_limits<unsigned int>::max();
        // Should be 2^32 - 1 for 32-bit unsigned int
        CHECK_EQ(max_val, 4294967295u);

        static_assert(numeric_limits<unsigned int>::max() > 0,
                      "unsigned int max should be positive");
    }
}

TEST_CASE("fl::numeric_limits<long>") {
    SUBCASE("min returns minimum long value") {
        constexpr long min_val = numeric_limits<long>::min();
        CHECK(min_val < 0);

        static_assert(numeric_limits<long>::min() < 0, "long min should be negative");
    }

    SUBCASE("max returns maximum long value") {
        constexpr long max_val = numeric_limits<long>::max();
        CHECK(max_val > 0);

        static_assert(numeric_limits<long>::max() > 0, "long max should be positive");
    }

    SUBCASE("consistent with sizeof") {
        // On systems where long is 4 bytes
        if (sizeof(long) == 4) {
            CHECK_EQ(numeric_limits<long>::max(), 2147483647L);
            CHECK_EQ(numeric_limits<long>::min(), -2147483647L - 1);
        }
        // On systems where long is 8 bytes
        else if (sizeof(long) == 8) {
            CHECK_EQ(numeric_limits<long>::max(), 9223372036854775807L);
            CHECK_EQ(numeric_limits<long>::min(), -9223372036854775807L - 1);
        }
    }
}

TEST_CASE("fl::numeric_limits<unsigned long>") {
    SUBCASE("min returns 0") {
        constexpr unsigned long min_val = numeric_limits<unsigned long>::min();
        CHECK_EQ(min_val, 0ul);

        static_assert(numeric_limits<unsigned long>::min() == 0,
                      "unsigned long min should be 0");
    }

    SUBCASE("max returns maximum unsigned long value") {
        constexpr unsigned long max_val = numeric_limits<unsigned long>::max();
        CHECK(max_val > 0);

        static_assert(numeric_limits<unsigned long>::max() > 0,
                      "unsigned long max should be positive");
    }

    SUBCASE("consistent with sizeof") {
        if (sizeof(unsigned long) == 4) {
            CHECK_EQ(numeric_limits<unsigned long>::max(), 4294967295ul);
        }
        else if (sizeof(unsigned long) == 8) {
            CHECK_EQ(numeric_limits<unsigned long>::max(), 18446744073709551615ul);
        }
    }
}

TEST_CASE("fl::numeric_limits<long long>") {
    SUBCASE("min returns minimum long long value") {
        constexpr long long min_val = numeric_limits<long long>::min();
        // Should be -2^63
        CHECK_EQ(min_val, -9223372036854775807LL - 1);

        static_assert(numeric_limits<long long>::min() < 0,
                      "long long min should be negative");
    }

    SUBCASE("max returns maximum long long value") {
        constexpr long long max_val = numeric_limits<long long>::max();
        // Should be 2^63 - 1
        CHECK_EQ(max_val, 9223372036854775807LL);

        static_assert(numeric_limits<long long>::max() > 0,
                      "long long max should be positive");
    }
}

TEST_CASE("fl::numeric_limits<unsigned long long>") {
    SUBCASE("min returns 0") {
        constexpr unsigned long long min_val = numeric_limits<unsigned long long>::min();
        CHECK_EQ(min_val, 0ull);

        static_assert(numeric_limits<unsigned long long>::min() == 0,
                      "unsigned long long min should be 0");
    }

    SUBCASE("max returns maximum unsigned long long value") {
        constexpr unsigned long long max_val = numeric_limits<unsigned long long>::max();
        // Should be 2^64 - 1
        CHECK_EQ(max_val, 18446744073709551615ull);

        static_assert(numeric_limits<unsigned long long>::max() > 0,
                      "unsigned long long max should be positive");
    }
}

TEST_CASE("fl::numeric_limits<float>") {
    SUBCASE("min returns minimum positive float value") {
        constexpr float min_val = numeric_limits<float>::min();
        CHECK(min_val > 0.0f);
        CHECK(min_val < 1.0f);

        // Should match __FLT_MIN__
        CHECK_EQ(min_val, __FLT_MIN__);

        static_assert(numeric_limits<float>::min() > 0,
                      "float min should be positive (smallest normalized value)");
    }

    SUBCASE("max returns maximum float value") {
        constexpr float max_val = numeric_limits<float>::max();
        CHECK(max_val > 1.0f);

        // Should match __FLT_MAX__
        CHECK_EQ(max_val, __FLT_MAX__);

        static_assert(numeric_limits<float>::max() > 0,
                      "float max should be positive");
    }
}

TEST_CASE("fl::numeric_limits<double>") {
    SUBCASE("min returns minimum positive double value") {
        constexpr double min_val = numeric_limits<double>::min();
        CHECK(min_val > 0.0);
        CHECK(min_val < 1.0);

        // Should match __DBL_MIN__
        CHECK_EQ(min_val, __DBL_MIN__);

        static_assert(numeric_limits<double>::min() > 0,
                      "double min should be positive (smallest normalized value)");
    }

    SUBCASE("max returns maximum double value") {
        constexpr double max_val = numeric_limits<double>::max();
        CHECK(max_val > 1.0);

        // Should match __DBL_MAX__
        CHECK_EQ(max_val, __DBL_MAX__);

        static_assert(numeric_limits<double>::max() > 0,
                      "double max should be positive");
    }

    SUBCASE("double range is larger than float range") {
        // Double should have larger range than float
        static_assert(numeric_limits<double>::max() > numeric_limits<float>::max(),
                      "double max should be larger than float max");
        static_assert(numeric_limits<double>::min() < numeric_limits<float>::min(),
                      "double min should be smaller than float min");
    }
}

TEST_CASE("fl::numeric_limits compile-time evaluation") {
    SUBCASE("all limits are constexpr") {
        // Test that all limits can be used in constexpr contexts
        constexpr int int_min = numeric_limits<int>::min();
        constexpr int int_max = numeric_limits<int>::max();
        constexpr unsigned int uint_max = numeric_limits<unsigned int>::max();
        constexpr long long_min = numeric_limits<long>::min();
        constexpr long long_max = numeric_limits<long>::max();
        constexpr long long ll_min = numeric_limits<long long>::min();
        constexpr long long ll_max = numeric_limits<long long>::max();
        constexpr float float_min = numeric_limits<float>::min();
        constexpr float float_max = numeric_limits<float>::max();
        constexpr double double_min = numeric_limits<double>::min();
        constexpr double double_max = numeric_limits<double>::max();

        // Use the values to prevent unused variable warnings
        CHECK(int_min < int_max);
        CHECK(uint_max > 0);
        CHECK(long_min < long_max);
        CHECK(ll_min < ll_max);
        CHECK(float_min < float_max);
        CHECK(double_min < double_max);
    }
}

TEST_CASE("fl::numeric_limits integral relationships") {
    SUBCASE("unsigned max is about twice signed max") {
        // For same-sized types, unsigned max ≈ 2 * signed max + 1
        static_assert(numeric_limits<unsigned int>::max() ==
                      static_cast<unsigned int>(numeric_limits<int>::max()) * 2u + 1u,
                      "unsigned max should be 2*signed_max + 1");
    }

    SUBCASE("signed min magnitude equals signed max + 1") {
        // For two's complement: |min| = max + 1
        long long int_min_abs = -static_cast<long long>(numeric_limits<int>::min());
        long long int_max = static_cast<long long>(numeric_limits<int>::max());
        CHECK_EQ(int_min_abs, int_max + 1);
    }
}
