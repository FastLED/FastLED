// g++ --std=c++11 test.cpp

#include "test.h"

#include "fl/algorithm.h"
#include "fl/dbg.h"
#include "fl/vector.h"
#include "fl/functional.h"
#include "fl/map.h"
#include "test.h"

using namespace fl;

TEST_CASE("reverse an int list") {
    fl::vector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    vec.push_back(3);
    vec.push_back(4);
    vec.push_back(5);

    fl::reverse(vec.begin(), vec.end());
    CHECK_EQ(vec[0], 5);
    CHECK_EQ(vec[1], 4);
    CHECK_EQ(vec[2], 3);
    CHECK_EQ(vec[3], 2);
    CHECK_EQ(vec[4], 1);
}

// This test should NOT compile - trying to sort a map should fail
// because it would break the tree's internal ordering invariant
#ifdef COMPILE_ERROR_TEST_MAP_SORT
TEST_CASE("fl::sort on fl::fl_map should fail to compile") {
    fl::fl_map<int, fl::string> map;
    map[3] = "three";
    map[1] = "one";
    map[2] = "two";

    // This CORRECTLY produces a compilation error!
    // ERROR: no match for 'operator+' on map iterators
    // 
    // fl::fl_map iterators are bidirectional (not random access)
    // fl::sort requires random access iterators for:
    //   - Iterator arithmetic: first + 1, last - first
    //   - Efficient pivot selection and partitioning
    //
    // This prevents users from accidentally corrupting the tree structure
    // by calling sort on a map, which would break ordering invariants.
    fl::sort(map.begin(), map.end());
}
#endif

TEST_CASE("fl::sort with default comparator") {
    SUBCASE("Sort integers") {
        fl::vector<int> vec;
        vec.push_back(5);
        vec.push_back(2);
        vec.push_back(8);
        vec.push_back(1);
        vec.push_back(9);
        vec.push_back(3);

        fl::sort(vec.begin(), vec.end());

        CHECK_EQ(vec[0], 1);
        CHECK_EQ(vec[1], 2);
        CHECK_EQ(vec[2], 3);
        CHECK_EQ(vec[3], 5);
        CHECK_EQ(vec[4], 8);
        CHECK_EQ(vec[5], 9);
    }

    SUBCASE("Sort empty container") {
        fl::vector<int> vec;
        fl::sort(vec.begin(), vec.end());
        CHECK_EQ(vec.size(), 0);
    }

    SUBCASE("Sort single element") {
        fl::vector<int> vec;
        vec.push_back(42);
        fl::sort(vec.begin(), vec.end());
        CHECK_EQ(vec.size(), 1);
        CHECK_EQ(vec[0], 42);
    }

    SUBCASE("Sort two elements") {
        fl::vector<int> vec;
        vec.push_back(3);
        vec.push_back(1);
        fl::sort(vec.begin(), vec.end());
        CHECK_EQ(vec[0], 1);
        CHECK_EQ(vec[1], 3);
    }

    SUBCASE("Sort already sorted array") {
        fl::vector<int> vec;
        vec.push_back(1);
        vec.push_back(2);
        vec.push_back(3);
        vec.push_back(4);
        vec.push_back(5);

        fl::sort(vec.begin(), vec.end());

        CHECK_EQ(vec[0], 1);
        CHECK_EQ(vec[1], 2);
        CHECK_EQ(vec[2], 3);
        CHECK_EQ(vec[3], 4);
        CHECK_EQ(vec[4], 5);
    }

    SUBCASE("Sort reverse sorted array") {
        fl::vector<int> vec;
        vec.push_back(5);
        vec.push_back(4);
        vec.push_back(3);
        vec.push_back(2);
        vec.push_back(1);

        fl::sort(vec.begin(), vec.end());

        CHECK_EQ(vec[0], 1);
        CHECK_EQ(vec[1], 2);
        CHECK_EQ(vec[2], 3);
        CHECK_EQ(vec[3], 4);
        CHECK_EQ(vec[4], 5);
    }

    SUBCASE("Sort with duplicates") {
        fl::vector<int> vec;
        vec.push_back(3);
        vec.push_back(1);
        vec.push_back(4);
        vec.push_back(1);
        vec.push_back(5);
        vec.push_back(3);
        vec.push_back(1);

        fl::sort(vec.begin(), vec.end());

        CHECK_EQ(vec[0], 1);
        CHECK_EQ(vec[1], 1);
        CHECK_EQ(vec[2], 1);
        CHECK_EQ(vec[3], 3);
        CHECK_EQ(vec[4], 3);
        CHECK_EQ(vec[5], 4);
        CHECK_EQ(vec[6], 5);
    }
}

TEST_CASE("fl::sort with custom comparator") {
    SUBCASE("Sort integers in descending order") {
        fl::vector<int> vec;
        vec.push_back(5);
        vec.push_back(2);
        vec.push_back(8);
        vec.push_back(1);
        vec.push_back(9);
        vec.push_back(3);

        fl::sort(vec.begin(), vec.end(), [](int a, int b) { return a > b; });

        CHECK_EQ(vec[0], 9);
        CHECK_EQ(vec[1], 8);
        CHECK_EQ(vec[2], 5);
        CHECK_EQ(vec[3], 3);
        CHECK_EQ(vec[4], 2);
        CHECK_EQ(vec[5], 1);
    }

    SUBCASE("Sort using fl::less") {
        fl::vector<int> vec;
        vec.push_back(5);
        vec.push_back(2);
        vec.push_back(8);
        vec.push_back(1);

        fl::sort(vec.begin(), vec.end(), fl::less<int>());

        CHECK_EQ(vec[0], 1);
        CHECK_EQ(vec[1], 2);
        CHECK_EQ(vec[2], 5);
        CHECK_EQ(vec[3], 8);
    }

    SUBCASE("Sort custom struct by member") {
        struct Point {
            int x, y;
            Point() : x(0), y(0) {}  // Add default constructor
            Point(int x, int y) : x(x), y(y) {}
        };

        fl::vector<Point> vec;
        vec.push_back(Point(3, 1));
        vec.push_back(Point(1, 3));
        vec.push_back(Point(2, 2));

        // Sort by x coordinate
        fl::sort(vec.begin(), vec.end(), [](const Point& a, const Point& b) {
            return a.x < b.x;
        });

        CHECK_EQ(vec[0].x, 1);
        CHECK_EQ(vec[1].x, 2);
        CHECK_EQ(vec[2].x, 3);
    }
}

TEST_CASE("fl::sort with different container types") {
    SUBCASE("Sort FixedVector") {
        fl::FixedVector<int, 6> vec;
        vec.push_back(5);
        vec.push_back(2);
        vec.push_back(8);
        vec.push_back(1);
        vec.push_back(9);
        vec.push_back(3);

        fl::sort(vec.begin(), vec.end());

        CHECK_EQ(vec[0], 1);
        CHECK_EQ(vec[1], 2);
        CHECK_EQ(vec[2], 3);
        CHECK_EQ(vec[3], 5);
        CHECK_EQ(vec[4], 8);
        CHECK_EQ(vec[5], 9);
    }

    SUBCASE("Sort HeapVector") {
        fl::HeapVector<int> vec;
        vec.push_back(5);
        vec.push_back(2);
        vec.push_back(8);
        vec.push_back(1);

        fl::sort(vec.begin(), vec.end());

        CHECK_EQ(vec[0], 1);
        CHECK_EQ(vec[1], 2);
        CHECK_EQ(vec[2], 5);
        CHECK_EQ(vec[3], 8);
    }
}

TEST_CASE("fl::sort stability and performance") {
    SUBCASE("Large array sort") {
        fl::vector<int> vec;
        // Create a larger array for testing
        for (int i = 100; i >= 1; --i) {
            vec.push_back(i);
        }

        fl::sort(vec.begin(), vec.end());

        // Verify all elements are in order
        for (size_t i = 0; i < vec.size(); ++i) {
            CHECK_EQ(vec[i], static_cast<int>(i + 1));
        }
    }

    SUBCASE("Partial sort") {
        fl::vector<int> vec;
        vec.push_back(5);
        vec.push_back(2);
        vec.push_back(8);
        vec.push_back(1);
        vec.push_back(9);
        vec.push_back(3);

        // Sort only first 3 elements
        fl::sort(vec.begin(), vec.begin() + 3);

        CHECK_EQ(vec[0], 2);
        CHECK_EQ(vec[1], 5);
        CHECK_EQ(vec[2], 8);
        // Rest should be unchanged
        CHECK_EQ(vec[3], 1);
        CHECK_EQ(vec[4], 9);
        CHECK_EQ(vec[5], 3);
    }
}

TEST_CASE("fl::sort edge cases") {
    SUBCASE("All elements equal") {
        fl::vector<int> vec;
        vec.push_back(5);
        vec.push_back(5);
        vec.push_back(5);
        vec.push_back(5);
        vec.push_back(5);

        fl::sort(vec.begin(), vec.end());

        for (size_t i = 0; i < vec.size(); ++i) {
            CHECK_EQ(vec[i], 5);
        }
    }

    SUBCASE("Mixed positive and negative") {
        fl::vector<int> vec;
        vec.push_back(-5);
        vec.push_back(2);
        vec.push_back(-8);
        vec.push_back(1);
        vec.push_back(0);
        vec.push_back(-1);

        fl::sort(vec.begin(), vec.end());

        CHECK_EQ(vec[0], -8);
        CHECK_EQ(vec[1], -5);
        CHECK_EQ(vec[2], -1);
        CHECK_EQ(vec[3], 0);
        CHECK_EQ(vec[4], 1);
        CHECK_EQ(vec[5], 2);
    }
}
