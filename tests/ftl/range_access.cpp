#include "fl/stl/range_access.h"
#include "fl/stl/array.h"
#include "test.h"

using namespace fl;

FL_TEST_CASE("fl::begin and fl::end for C arrays") {
    FL_SUBCASE("int array") {
        int arr[5] = {1, 2, 3, 4, 5};

        int* b = fl::begin(arr);
        int* e = fl::end(arr);

        FL_CHECK_EQ(b, arr);
        FL_CHECK_EQ(e, arr + 5);
        FL_CHECK_EQ(e - b, 5);
        FL_CHECK_EQ(*b, 1);
        FL_CHECK_EQ(*(e - 1), 5);
    }

    FL_SUBCASE("const int array") {
        const int arr[3] = {10, 20, 30};

        const int* b = fl::begin(arr);
        const int* e = fl::end(arr);

        FL_CHECK_EQ(b, arr);
        FL_CHECK_EQ(e, arr + 3);
        FL_CHECK_EQ(e - b, 3);
    }

    FL_SUBCASE("double array") {
        double arr[4] = {1.1, 2.2, 3.3, 4.4};

        double* b = fl::begin(arr);
        double* e = fl::end(arr);

        FL_CHECK_EQ(e - b, 4);
        FL_CHECK(doctest::Approx(*b).epsilon(0.001) == 1.1);
    }

    FL_SUBCASE("single element array") {
        int arr[1] = {42};

        int* b = fl::begin(arr);
        int* e = fl::end(arr);

        FL_CHECK_EQ(e - b, 1);
        FL_CHECK_EQ(*b, 42);
    }

    FL_SUBCASE("iterate with begin/end") {
        int arr[5] = {1, 2, 3, 4, 5};
        int sum = 0;

        for (auto it = fl::begin(arr); it != fl::end(arr); ++it) {
            sum += *it;
        }

        FL_CHECK_EQ(sum, 15);
    }
}

FL_TEST_CASE("fl::begin and fl::end for containers") {
    FL_SUBCASE("fl::array") {
        fl::array<int, 4> arr = {{10, 20, 30, 40}};

        auto b = fl::begin(arr);
        auto e = fl::end(arr);

        FL_CHECK_EQ(b, arr.begin());
        FL_CHECK_EQ(e, arr.end());
        FL_CHECK_EQ(e - b, 4);
        FL_CHECK_EQ(*b, 10);
    }

    FL_SUBCASE("const fl::array") {
        const fl::array<int, 3> arr = {{5, 15, 25}};

        auto b = fl::begin(arr);
        auto e = fl::end(arr);

        FL_CHECK_EQ(b, arr.begin());
        FL_CHECK_EQ(e, arr.end());
        FL_CHECK_EQ(e - b, 3);
    }

    FL_SUBCASE("iterate with range-based constructs") {
        fl::array<int, 5> arr = {{1, 2, 3, 4, 5}};
        int sum = 0;

        auto b = fl::begin(arr);
        auto e = fl::end(arr);

        for (auto it = b; it != e; ++it) {
            sum += *it;
        }

        FL_CHECK_EQ(sum, 15);
    }
}

FL_TEST_CASE("fl::begin and fl::end constexpr") {
    FL_SUBCASE("constexpr evaluation with static array") {
        static constexpr int arr[3] = {1, 2, 3};
        constexpr const int* b = fl::begin(arr);
        constexpr const int* e = fl::end(arr);

        static_assert(e - b == 3, "size should be 3");
        FL_CHECK_EQ(e - b, 3);
    }

    FL_SUBCASE("runtime with fl::array") {
        // fl::array constructor is not constexpr due to initializer_list,
        // so we test it at runtime
        const fl::array<int, 4> arr = {{10, 20, 30, 40}};

        auto b = fl::begin(arr);
        auto e = fl::end(arr);

        FL_CHECK_EQ(e - b, 4);
    }
}

FL_TEST_CASE("fl::begin and fl::end with different types") {
    FL_SUBCASE("char array") {
        char str[6] = "hello";

        char* b = fl::begin(str);
        char* e = fl::end(str);

        FL_CHECK_EQ(e - b, 6);  // includes null terminator
        FL_CHECK_EQ(*b, 'h');
    }

    FL_SUBCASE("float array") {
        float arr[3] = {1.5f, 2.5f, 3.5f};

        float* b = fl::begin(arr);
        float* e = fl::end(arr);

        FL_CHECK_EQ(e - b, 3);
        FL_CHECK(doctest::Approx(*b).epsilon(0.001) == 1.5f);
    }

    FL_SUBCASE("struct array") {
        struct Point {
            int x, y;
        };

        Point arr[2] = {{1, 2}, {3, 4}};

        Point* b = fl::begin(arr);
        Point* e = fl::end(arr);

        FL_CHECK_EQ(e - b, 2);
        FL_CHECK_EQ(b->x, 1);
        FL_CHECK_EQ(b->y, 2);
    }
}

FL_TEST_CASE("fl::begin and fl::end for empty-like cases") {
    FL_SUBCASE("single element array") {
        int arr[1] = {99};

        FL_CHECK_EQ(fl::end(arr) - fl::begin(arr), 1);
        FL_CHECK_EQ(*fl::begin(arr), 99);
    }

    FL_SUBCASE("empty fl::array") {
        fl::array<int, 0> arr;

        auto b = fl::begin(arr);
        auto e = fl::end(arr);

        FL_CHECK_EQ(b, e);  // empty container
    }
}

FL_TEST_CASE("fl::begin and fl::end modify through iterator") {
    FL_SUBCASE("modify array elements") {
        int arr[4] = {1, 2, 3, 4};

        for (auto it = fl::begin(arr); it != fl::end(arr); ++it) {
            *it *= 2;
        }

        FL_CHECK_EQ(arr[0], 2);
        FL_CHECK_EQ(arr[1], 4);
        FL_CHECK_EQ(arr[2], 6);
        FL_CHECK_EQ(arr[3], 8);
    }

    FL_SUBCASE("modify container elements") {
        fl::array<int, 3> arr = {{10, 20, 30}};

        for (auto it = fl::begin(arr); it != fl::end(arr); ++it) {
            *it += 5;
        }

        FL_CHECK_EQ(arr[0], 15);
        FL_CHECK_EQ(arr[1], 25);
        FL_CHECK_EQ(arr[2], 35);
    }
}
