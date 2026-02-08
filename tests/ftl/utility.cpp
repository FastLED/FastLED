#include "fl/stl/utility.h"
#include "fl/stl/limits.h"
#include "test.h"

using namespace fl;

FL_TEST_CASE("fl::less<T>") {
    FL_SUBCASE("int comparisons") {
        less<int> cmp;
        FL_CHECK(cmp(1, 2));
        FL_CHECK(!cmp(2, 1));
        FL_CHECK(!cmp(5, 5));
        FL_CHECK(cmp(-10, 0));
        FL_CHECK(cmp(-5, -3));
    }

    FL_SUBCASE("unsigned comparisons") {
        less<unsigned int> cmp;
        FL_CHECK(cmp(0u, 1u));
        FL_CHECK(cmp(100u, 200u));
        FL_CHECK(!cmp(200u, 100u));
        FL_CHECK(!cmp(50u, 50u));
    }

    FL_SUBCASE("float comparisons") {
        less<float> cmp;
        FL_CHECK(cmp(1.0f, 2.0f));
        FL_CHECK(cmp(-1.0f, 0.0f));
        FL_CHECK(!cmp(2.0f, 1.0f));
        FL_CHECK(!cmp(3.14f, 3.14f));
        FL_CHECK(cmp(0.0f, 0.1f));
    }

    FL_SUBCASE("double comparisons") {
        less<double> cmp;
        FL_CHECK(cmp(1.0, 2.0));
        FL_CHECK(cmp(-1.0, 0.0));
        FL_CHECK(!cmp(2.0, 1.0));
        FL_CHECK(!cmp(3.14159, 3.14159));
    }

    FL_SUBCASE("char comparisons") {
        less<char> cmp;
        FL_CHECK(cmp('a', 'b'));
        FL_CHECK(cmp('A', 'Z'));
        FL_CHECK(!cmp('z', 'a'));
        FL_CHECK(!cmp('m', 'm'));
    }

    FL_SUBCASE("const types") {
        less<const int> cmp;
        const int a = 5;
        const int b = 10;
        FL_CHECK(cmp(a, b));
        FL_CHECK(!cmp(b, a));
        FL_CHECK(!cmp(a, a));
    }

    FL_SUBCASE("constexpr evaluation") {
        // Test that less can be used in constexpr contexts
        constexpr less<int> cmp;
        static_assert(cmp(1, 2), "1 < 2 should be true");
        static_assert(!cmp(2, 1), "2 < 1 should be false");
        static_assert(!cmp(5, 5), "5 < 5 should be false");
    }
}

FL_TEST_CASE("fl::less<void> - transparent comparator") {
    less<void> cmp;

    FL_SUBCASE("same types") {
        FL_CHECK(cmp(1, 2));
        FL_CHECK(!cmp(2, 1));
        FL_CHECK(!cmp(5, 5));
    }

    FL_SUBCASE("different integer types") {
        FL_CHECK(cmp(short(10), int(20)));
        FL_CHECK(cmp(int(5), long(10)));
        FL_CHECK(cmp(char(10), int(20)));
        FL_CHECK(!cmp(long(100), int(50)));
    }

    FL_SUBCASE("signed and unsigned") {
        // Note: signed/unsigned comparisons have platform-dependent behavior
        // These tests verify that less<void> compiles and works with mixed types
        FL_CHECK(cmp(short(10), int(20)));
        FL_CHECK(cmp(char(5), int(10)));
    }

    FL_SUBCASE("integer and floating point") {
        FL_CHECK(cmp(1, 1.5));
        FL_CHECK(cmp(5, 10.0));
        FL_CHECK(!cmp(10, 5.0));
        FL_CHECK(cmp(int(3), float(3.14)));
    }

    FL_SUBCASE("float and double") {
        FL_CHECK(cmp(float(1.0), double(2.0)));
        FL_CHECK(cmp(double(1.5), float(2.5)));
        FL_CHECK(!cmp(float(5.0), double(3.0)));
    }

    FL_SUBCASE("forward semantics") {
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
        FL_CHECK(cmp(MoveOnly(1), MoveOnly(2)));
    }

    FL_SUBCASE("constexpr with void") {
        constexpr less<void> cmp_void;
        static_assert(cmp_void(1, 2), "1 < 2 should be true");
        static_assert(!cmp_void(2, 1), "2 < 1 should be false");
        static_assert(cmp_void(1.0f, 2.0), "1.0f < 2.0 should be true");
    }
}

FL_TEST_CASE("fl::DefaultLess - backward compatibility") {
    FL_SUBCASE("alias works correctly") {
        // DefaultLess should be an alias for less
        DefaultLess<int> cmp;
        FL_CHECK(cmp(1, 2));
        FL_CHECK(!cmp(2, 1));
        FL_CHECK(!cmp(5, 5));
    }

    FL_SUBCASE("same behavior as less<T>") {
        less<int> less_cmp;
        DefaultLess<int> default_cmp;

        FL_CHECK(less_cmp(1, 2) == default_cmp(1, 2));
        FL_CHECK(less_cmp(5, 3) == default_cmp(5, 3));
        FL_CHECK(less_cmp(10, 10) == default_cmp(10, 10));
    }

    FL_SUBCASE("constexpr compatibility") {
        constexpr DefaultLess<int> cmp;
        static_assert(cmp(1, 2), "DefaultLess should work in constexpr context");
    }
}

FL_TEST_CASE("fl::less - edge cases") {
    FL_SUBCASE("zero comparisons") {
        less<int> cmp;
        FL_CHECK(cmp(-1, 0));
        FL_CHECK(cmp(0, 1));
        FL_CHECK(!cmp(0, 0));
        FL_CHECK(!cmp(0, -1));
    }

    FL_SUBCASE("boundary values") {
        less<int> cmp;
        FL_CHECK(cmp(fl::numeric_limits<int>::min(), 0));
        FL_CHECK(cmp(0, fl::numeric_limits<int>::max()));
        FL_CHECK(cmp(fl::numeric_limits<int>::min(), fl::numeric_limits<int>::max()));
        FL_CHECK(!cmp(fl::numeric_limits<int>::max(), fl::numeric_limits<int>::min()));
    }

    FL_SUBCASE("floating point special values") {
        less<float> cmp;

        // Normal values
        FL_CHECK(cmp(1.0f, 2.0f));

        // Very small values
        FL_CHECK(cmp(0.0f, 0.001f));
        FL_CHECK(cmp(-0.001f, 0.0f));

        // Negative zero (treated as equal to positive zero)
        float neg_zero = -0.0f;
        float pos_zero = 0.0f;
        FL_CHECK(!cmp(neg_zero, pos_zero));
        FL_CHECK(!cmp(pos_zero, neg_zero));
    }

    FL_SUBCASE("pointer comparisons") {
        less<int*> cmp;
        int arr[5] = {1, 2, 3, 4, 5};

        // Pointers in array are ordered
        FL_CHECK(cmp(&arr[0], &arr[1]));
        FL_CHECK(cmp(&arr[0], &arr[4]));
        FL_CHECK(!cmp(&arr[3], &arr[1]));
        FL_CHECK(!cmp(&arr[2], &arr[2]));
    }
}

FL_TEST_CASE("fl::less - use with standard algorithms pattern") {
    FL_SUBCASE("manual sorting check") {
        // Demonstrate less can be used like std::less in sorting contexts
        int arr[] = {5, 2, 8, 1, 9};
        less<int> cmp;

        // Manually verify sorting logic using less
        FL_CHECK(cmp(arr[1], arr[0]));  // 2 < 5
        FL_CHECK(cmp(arr[3], arr[1]));  // 1 < 2
        FL_CHECK(!cmp(arr[4], arr[2])); // 9 < 8 is false
    }

    FL_SUBCASE("transparent comparison in generic context") {
        less<void> cmp;

        // Can compare different types without explicit template instantiation
        FL_CHECK(cmp(1, 2));
        FL_CHECK(cmp(1.5f, 2.5));
        FL_CHECK(cmp('a', 'z'));

        // Verify it works with different type combinations
        FL_CHECK(cmp(short(10), long(20)));
        FL_CHECK(cmp(float(1.5), double(2.5)));
    }
}
