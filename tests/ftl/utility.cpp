#include "fl/stl/utility.h"
#include "fl/stl/limits.h"
#include "doctest.h"

using namespace fl;

TEST_CASE("fl::less<T>") {
    SUBCASE("int comparisons") {
        less<int> cmp;
        CHECK(cmp(1, 2));
        CHECK(!cmp(2, 1));
        CHECK(!cmp(5, 5));
        CHECK(cmp(-10, 0));
        CHECK(cmp(-5, -3));
    }

    SUBCASE("unsigned comparisons") {
        less<unsigned int> cmp;
        CHECK(cmp(0u, 1u));
        CHECK(cmp(100u, 200u));
        CHECK(!cmp(200u, 100u));
        CHECK(!cmp(50u, 50u));
    }

    SUBCASE("float comparisons") {
        less<float> cmp;
        CHECK(cmp(1.0f, 2.0f));
        CHECK(cmp(-1.0f, 0.0f));
        CHECK(!cmp(2.0f, 1.0f));
        CHECK(!cmp(3.14f, 3.14f));
        CHECK(cmp(0.0f, 0.1f));
    }

    SUBCASE("double comparisons") {
        less<double> cmp;
        CHECK(cmp(1.0, 2.0));
        CHECK(cmp(-1.0, 0.0));
        CHECK(!cmp(2.0, 1.0));
        CHECK(!cmp(3.14159, 3.14159));
    }

    SUBCASE("char comparisons") {
        less<char> cmp;
        CHECK(cmp('a', 'b'));
        CHECK(cmp('A', 'Z'));
        CHECK(!cmp('z', 'a'));
        CHECK(!cmp('m', 'm'));
    }

    SUBCASE("const types") {
        less<const int> cmp;
        const int a = 5;
        const int b = 10;
        CHECK(cmp(a, b));
        CHECK(!cmp(b, a));
        CHECK(!cmp(a, a));
    }

    SUBCASE("constexpr evaluation") {
        // Test that less can be used in constexpr contexts
        constexpr less<int> cmp;
        static_assert(cmp(1, 2), "1 < 2 should be true");
        static_assert(!cmp(2, 1), "2 < 1 should be false");
        static_assert(!cmp(5, 5), "5 < 5 should be false");
    }
}

TEST_CASE("fl::less<void> - transparent comparator") {
    less<void> cmp;

    SUBCASE("same types") {
        CHECK(cmp(1, 2));
        CHECK(!cmp(2, 1));
        CHECK(!cmp(5, 5));
    }

    SUBCASE("different integer types") {
        CHECK(cmp(short(10), int(20)));
        CHECK(cmp(int(5), long(10)));
        CHECK(cmp(char(10), int(20)));
        CHECK(!cmp(long(100), int(50)));
    }

    SUBCASE("signed and unsigned") {
        // Note: signed/unsigned comparisons have platform-dependent behavior
        // These tests verify that less<void> compiles and works with mixed types
        CHECK(cmp(short(10), int(20)));
        CHECK(cmp(char(5), int(10)));
    }

    SUBCASE("integer and floating point") {
        CHECK(cmp(1, 1.5));
        CHECK(cmp(5, 10.0));
        CHECK(!cmp(10, 5.0));
        CHECK(cmp(int(3), float(3.14)));
    }

    SUBCASE("float and double") {
        CHECK(cmp(float(1.0), double(2.0)));
        CHECK(cmp(double(1.5), float(2.5)));
        CHECK(!cmp(float(5.0), double(3.0)));
    }

    SUBCASE("forward semantics") {
        // Test that less<void> properly forwards arguments
        struct MoveOnly {
            int value;
            MoveOnly(int v) : value(v) {}
            MoveOnly(const MoveOnly&) = delete;
            MoveOnly(MoveOnly&& other) : value(other.value) {}
            bool operator<(const MoveOnly& other) const {
                return value < other.value;
            }
        };

        // This should compile because less<void> uses perfect forwarding
        CHECK(cmp(MoveOnly(1), MoveOnly(2)));
    }

    SUBCASE("constexpr with void") {
        constexpr less<void> cmp_void;
        static_assert(cmp_void(1, 2), "1 < 2 should be true");
        static_assert(!cmp_void(2, 1), "2 < 1 should be false");
        static_assert(cmp_void(1.0f, 2.0), "1.0f < 2.0 should be true");
    }
}

TEST_CASE("fl::DefaultLess - backward compatibility") {
    SUBCASE("alias works correctly") {
        // DefaultLess should be an alias for less
        DefaultLess<int> cmp;
        CHECK(cmp(1, 2));
        CHECK(!cmp(2, 1));
        CHECK(!cmp(5, 5));
    }

    SUBCASE("same behavior as less<T>") {
        less<int> less_cmp;
        DefaultLess<int> default_cmp;

        CHECK(less_cmp(1, 2) == default_cmp(1, 2));
        CHECK(less_cmp(5, 3) == default_cmp(5, 3));
        CHECK(less_cmp(10, 10) == default_cmp(10, 10));
    }

    SUBCASE("constexpr compatibility") {
        constexpr DefaultLess<int> cmp;
        static_assert(cmp(1, 2), "DefaultLess should work in constexpr context");
    }
}

TEST_CASE("fl::less - edge cases") {
    SUBCASE("zero comparisons") {
        less<int> cmp;
        CHECK(cmp(-1, 0));
        CHECK(cmp(0, 1));
        CHECK(!cmp(0, 0));
        CHECK(!cmp(0, -1));
    }

    SUBCASE("boundary values") {
        less<int> cmp;
        CHECK(cmp(fl::numeric_limits<int>::min(), 0));
        CHECK(cmp(0, fl::numeric_limits<int>::max()));
        CHECK(cmp(fl::numeric_limits<int>::min(), fl::numeric_limits<int>::max()));
        CHECK(!cmp(fl::numeric_limits<int>::max(), fl::numeric_limits<int>::min()));
    }

    SUBCASE("floating point special values") {
        less<float> cmp;

        // Normal values
        CHECK(cmp(1.0f, 2.0f));

        // Very small values
        CHECK(cmp(0.0f, 0.001f));
        CHECK(cmp(-0.001f, 0.0f));

        // Negative zero (treated as equal to positive zero)
        float neg_zero = -0.0f;
        float pos_zero = 0.0f;
        CHECK(!cmp(neg_zero, pos_zero));
        CHECK(!cmp(pos_zero, neg_zero));
    }

    SUBCASE("pointer comparisons") {
        less<int*> cmp;
        int arr[5] = {1, 2, 3, 4, 5};

        // Pointers in array are ordered
        CHECK(cmp(&arr[0], &arr[1]));
        CHECK(cmp(&arr[0], &arr[4]));
        CHECK(!cmp(&arr[3], &arr[1]));
        CHECK(!cmp(&arr[2], &arr[2]));
    }
}

TEST_CASE("fl::less - use with standard algorithms pattern") {
    SUBCASE("manual sorting check") {
        // Demonstrate less can be used like std::less in sorting contexts
        int arr[] = {5, 2, 8, 1, 9};
        less<int> cmp;

        // Manually verify sorting logic using less
        CHECK(cmp(arr[1], arr[0]));  // 2 < 5
        CHECK(cmp(arr[3], arr[1]));  // 1 < 2
        CHECK(!cmp(arr[4], arr[2])); // 9 < 8 is false
    }

    SUBCASE("transparent comparison in generic context") {
        less<void> cmp;

        // Can compare different types without explicit template instantiation
        CHECK(cmp(1, 2));
        CHECK(cmp(1.5f, 2.5));
        CHECK(cmp('a', 'z'));

        // Verify it works with different type combinations
        CHECK(cmp(short(10), long(20)));
        CHECK(cmp(float(1.5), double(2.5)));
    }
}
