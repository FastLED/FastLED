#include "fl/stl/range_access.h"
#include "fl/stl/array.h"
#include "doctest.h"

using namespace fl;

TEST_CASE("fl::begin and fl::end for C arrays") {
    SUBCASE("int array") {
        int arr[5] = {1, 2, 3, 4, 5};

        int* b = fl::begin(arr);
        int* e = fl::end(arr);

        CHECK_EQ(b, arr);
        CHECK_EQ(e, arr + 5);
        CHECK_EQ(e - b, 5);
        CHECK_EQ(*b, 1);
        CHECK_EQ(*(e - 1), 5);
    }

    SUBCASE("const int array") {
        const int arr[3] = {10, 20, 30};

        const int* b = fl::begin(arr);
        const int* e = fl::end(arr);

        CHECK_EQ(b, arr);
        CHECK_EQ(e, arr + 3);
        CHECK_EQ(e - b, 3);
    }

    SUBCASE("double array") {
        double arr[4] = {1.1, 2.2, 3.3, 4.4};

        double* b = fl::begin(arr);
        double* e = fl::end(arr);

        CHECK_EQ(e - b, 4);
        CHECK(doctest::Approx(*b).epsilon(0.001) == 1.1);
    }

    SUBCASE("single element array") {
        int arr[1] = {42};

        int* b = fl::begin(arr);
        int* e = fl::end(arr);

        CHECK_EQ(e - b, 1);
        CHECK_EQ(*b, 42);
    }

    SUBCASE("iterate with begin/end") {
        int arr[5] = {1, 2, 3, 4, 5};
        int sum = 0;

        for (auto it = fl::begin(arr); it != fl::end(arr); ++it) {
            sum += *it;
        }

        CHECK_EQ(sum, 15);
    }
}

TEST_CASE("fl::begin and fl::end for containers") {
    SUBCASE("fl::array") {
        fl::array<int, 4> arr = {{10, 20, 30, 40}};

        auto b = fl::begin(arr);
        auto e = fl::end(arr);

        CHECK_EQ(b, arr.begin());
        CHECK_EQ(e, arr.end());
        CHECK_EQ(e - b, 4);
        CHECK_EQ(*b, 10);
    }

    SUBCASE("const fl::array") {
        const fl::array<int, 3> arr = {{5, 15, 25}};

        auto b = fl::begin(arr);
        auto e = fl::end(arr);

        CHECK_EQ(b, arr.begin());
        CHECK_EQ(e, arr.end());
        CHECK_EQ(e - b, 3);
    }

    SUBCASE("iterate with range-based constructs") {
        fl::array<int, 5> arr = {{1, 2, 3, 4, 5}};
        int sum = 0;

        auto b = fl::begin(arr);
        auto e = fl::end(arr);

        for (auto it = b; it != e; ++it) {
            sum += *it;
        }

        CHECK_EQ(sum, 15);
    }
}

TEST_CASE("fl::begin and fl::end constexpr") {
    SUBCASE("constexpr evaluation with static array") {
        static constexpr int arr[3] = {1, 2, 3};
        constexpr const int* b = fl::begin(arr);
        constexpr const int* e = fl::end(arr);

        static_assert(e - b == 3, "size should be 3");
        CHECK_EQ(e - b, 3);
    }

    SUBCASE("runtime with fl::array") {
        // fl::array constructor is not constexpr due to initializer_list,
        // so we test it at runtime
        const fl::array<int, 4> arr = {{10, 20, 30, 40}};

        auto b = fl::begin(arr);
        auto e = fl::end(arr);

        CHECK_EQ(e - b, 4);
    }
}

TEST_CASE("fl::begin and fl::end with different types") {
    SUBCASE("char array") {
        char str[6] = "hello";

        char* b = fl::begin(str);
        char* e = fl::end(str);

        CHECK_EQ(e - b, 6);  // includes null terminator
        CHECK_EQ(*b, 'h');
    }

    SUBCASE("float array") {
        float arr[3] = {1.5f, 2.5f, 3.5f};

        float* b = fl::begin(arr);
        float* e = fl::end(arr);

        CHECK_EQ(e - b, 3);
        CHECK(doctest::Approx(*b).epsilon(0.001) == 1.5f);
    }

    SUBCASE("struct array") {
        struct Point {
            int x, y;
        };

        Point arr[2] = {{1, 2}, {3, 4}};

        Point* b = fl::begin(arr);
        Point* e = fl::end(arr);

        CHECK_EQ(e - b, 2);
        CHECK_EQ(b->x, 1);
        CHECK_EQ(b->y, 2);
    }
}

TEST_CASE("fl::begin and fl::end for empty-like cases") {
    SUBCASE("single element array") {
        int arr[1] = {99};

        CHECK_EQ(fl::end(arr) - fl::begin(arr), 1);
        CHECK_EQ(*fl::begin(arr), 99);
    }

    SUBCASE("empty fl::array") {
        fl::array<int, 0> arr;

        auto b = fl::begin(arr);
        auto e = fl::end(arr);

        CHECK_EQ(b, e);  // empty container
    }
}

TEST_CASE("fl::begin and fl::end modify through iterator") {
    SUBCASE("modify array elements") {
        int arr[4] = {1, 2, 3, 4};

        for (auto it = fl::begin(arr); it != fl::end(arr); ++it) {
            *it *= 2;
        }

        CHECK_EQ(arr[0], 2);
        CHECK_EQ(arr[1], 4);
        CHECK_EQ(arr[2], 6);
        CHECK_EQ(arr[3], 8);
    }

    SUBCASE("modify container elements") {
        fl::array<int, 3> arr = {{10, 20, 30}};

        for (auto it = fl::begin(arr); it != fl::end(arr); ++it) {
            *it += 5;
        }

        CHECK_EQ(arr[0], 15);
        CHECK_EQ(arr[1], 25);
        CHECK_EQ(arr[2], 35);
    }
}
