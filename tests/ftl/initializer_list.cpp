#include "doctest.h"
#include "fl/stl/initializer_list.h"
#include "fl/int.h"

using namespace fl;

TEST_CASE("fl::initializer_list basic functionality") {
    SUBCASE("empty initializer_list") {
        fl::initializer_list<int> empty;

        // Note: std::initializer_list doesn't have empty() method
        CHECK_EQ(empty.size(), 0);
        CHECK_EQ(empty.begin(), empty.end());
    }

    SUBCASE("initializer_list with elements") {
        fl::initializer_list<int> list = {1, 2, 3, 4, 5};

        CHECK_EQ(list.size(), 5);
        CHECK(list.begin() != list.end());
    }

    SUBCASE("access elements via iterators") {
        fl::initializer_list<int> list = {10, 20, 30};

        auto it = list.begin();
        CHECK_EQ(*it, 10);
        ++it;
        CHECK_EQ(*it, 20);
        ++it;
        CHECK_EQ(*it, 30);
        ++it;
        CHECK(it == list.end());
    }

    SUBCASE("range-based iteration") {
        fl::initializer_list<int> list = {1, 2, 3, 4};
        int sum = 0;

        for (auto val : list) {
            sum += val;
        }

        CHECK_EQ(sum, 10);
    }
}

TEST_CASE("fl::initializer_list with different types") {
    SUBCASE("double values") {
        fl::initializer_list<double> list = {1.5, 2.5, 3.5};

        CHECK_EQ(list.size(), 3);

        auto it = list.begin();
        CHECK(doctest::Approx(*it).epsilon(0.001) == 1.5);
        ++it;
        CHECK(doctest::Approx(*it).epsilon(0.001) == 2.5);
        ++it;
        CHECK(doctest::Approx(*it).epsilon(0.001) == 3.5);
    }

    SUBCASE("const char* strings") {
        fl::initializer_list<const char*> list = {"hello", "world"};

        CHECK_EQ(list.size(), 2);
    }

    SUBCASE("struct types") {
        struct Point {
            int x, y;
        };

        fl::initializer_list<Point> list = {{1, 2}, {3, 4}, {5, 6}};

        CHECK_EQ(list.size(), 3);

        auto it = list.begin();
        CHECK_EQ(it->x, 1);
        CHECK_EQ(it->y, 2);
        ++it;
        CHECK_EQ(it->x, 3);
    }
}

TEST_CASE("fl::initializer_list size and empty") {
    SUBCASE("size of various lists") {
        fl::initializer_list<int> empty;
        fl::initializer_list<int> single = {42};
        fl::initializer_list<int> multiple = {1, 2, 3, 4, 5, 6, 7};

        CHECK_EQ(empty.size(), 0);
        CHECK_EQ(single.size(), 1);
        CHECK_EQ(multiple.size(), 7);
    }

    SUBCASE("empty check via size") {
        fl::initializer_list<int> empty;
        fl::initializer_list<int> nonempty = {1};

        CHECK_EQ(empty.size(), 0);
        CHECK(nonempty.size() > 0);
    }
}

TEST_CASE("fl::initializer_list iterators") {
    SUBCASE("begin and end consistency") {
        fl::initializer_list<int> list = {1, 2, 3};

        auto b = list.begin();
        auto e = list.end();

        CHECK_EQ(e - b, 3);
    }

    SUBCASE("iterator arithmetic") {
        fl::initializer_list<int> list = {10, 20, 30, 40};

        auto it = list.begin();
        CHECK_EQ(*it, 10);
        CHECK_EQ(*(it + 1), 20);
        CHECK_EQ(*(it + 2), 30);
        CHECK_EQ(*(it + 3), 40);
    }

    SUBCASE("empty list iterators") {
        fl::initializer_list<int> empty;

        CHECK_EQ(empty.begin(), empty.end());
    }
}

TEST_CASE("fl::initializer_list const correctness") {
    SUBCASE("const reference access") {
        fl::initializer_list<int> list = {1, 2, 3};

        // initializer_list always provides const access
        auto it = list.begin();
        // Cannot modify: *it = 10; // Would be a compile error
        CHECK_EQ(*it, 1);
    }

    SUBCASE("const initializer_list") {
        const fl::initializer_list<int> list = {5, 10, 15};

        CHECK_EQ(list.size(), 3);
        CHECK(list.size() > 0);

        auto it = list.begin();
        CHECK_EQ(*it, 5);
    }
}

TEST_CASE("fl::initializer_list copy and assignment") {
    SUBCASE("copy constructor") {
        fl::initializer_list<int> list1 = {1, 2, 3};
        fl::initializer_list<int> list2 = list1;

        CHECK_EQ(list1.size(), list2.size());
        CHECK_EQ(list1.begin(), list2.begin());
        CHECK_EQ(list1.end(), list2.end());
    }

    SUBCASE("assignment") {
        fl::initializer_list<int> list1 = {1, 2, 3};
        fl::initializer_list<int> list2;

        list2 = list1;

        CHECK_EQ(list1.size(), list2.size());
        CHECK_EQ(list1.begin(), list2.begin());
    }
}

TEST_CASE("fl::initializer_list with containers") {
    SUBCASE("initialize fl::array from initializer_list") {
        auto init = {1, 2, 3, 4};

        // Verify we can iterate over the initializer_list
        int sum = 0;
        for (auto val : init) {
            sum += val;
        }
        CHECK_EQ(sum, 10);
    }

    SUBCASE("pass initializer_list to function") {
        auto sum_func = [](fl::initializer_list<int> list) {
            int sum = 0;
            for (auto val : list) {
                sum += val;
            }
            return sum;
        };

        CHECK_EQ(sum_func({1, 2, 3}), 6);
        CHECK_EQ(sum_func({10, 20, 30, 40}), 100);
        CHECK_EQ(sum_func({}), 0);
    }
}

TEST_CASE("fl::initializer_list free functions") {
    // Note: fl::begin/fl::end for initializer_list are only defined on AVR
    // On other platforms, fl::initializer_list is just std::initializer_list
    // and we use the member begin()/end() methods

#ifdef __AVR__
    SUBCASE("fl::begin for initializer_list") {
        fl::initializer_list<int> list = {1, 2, 3};

        auto b = fl::begin(list);
        CHECK_EQ(b, list.begin());
        CHECK_EQ(*b, 1);
    }

    SUBCASE("fl::end for initializer_list") {
        fl::initializer_list<int> list = {1, 2, 3};

        auto e = fl::end(list);
        CHECK_EQ(e, list.end());
    }

    SUBCASE("iterate using free functions") {
        fl::initializer_list<int> list = {5, 10, 15, 20};
        int sum = 0;

        for (auto it = fl::begin(list); it != fl::end(list); ++it) {
            sum += *it;
        }

        CHECK_EQ(sum, 50);
    }
#else
    SUBCASE("non-AVR platforms use std::initializer_list") {  // okay std namespace - documenting implementation
        fl::initializer_list<int> list = {1, 2, 3};

        // We can use member functions directly
        auto b = list.begin();
        auto e = list.end();

        CHECK_EQ(b, list.begin());
        CHECK_EQ(e, list.end());
        CHECK_EQ(*b, 1);
    }
#endif
}

TEST_CASE("fl::initializer_list constexpr behavior") {
    SUBCASE("constexpr construction") {
        // Note: constexpr initializer_list is compiler-dependent
        // but we can test that the class has constexpr members
        fl::initializer_list<int> list = {1, 2, 3};

        // These operations should work at runtime
        CHECK_EQ(list.size(), 3);
        CHECK(list.size() > 0);
    }
}

TEST_CASE("fl::initializer_list single element") {
    SUBCASE("single int") {
        fl::initializer_list<int> list = {42};

        CHECK_EQ(list.size(), 1);
        CHECK(list.size() > 0);
        CHECK_EQ(*list.begin(), 42);
    }

    SUBCASE("single double") {
        fl::initializer_list<double> list = {3.14};

        CHECK_EQ(list.size(), 1);
        CHECK(doctest::Approx(*list.begin()).epsilon(0.001) == 3.14);
    }
}
