
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "fixed_vector.h"

TEST_CASE("Fixed vector simple") {
    FASTLED_NAMESPACE_USE;
    FixedVector<int, 5> vec;

    SUBCASE("Initial state") {
        CHECK(vec.size() == 0);
        CHECK(vec.capacity() == 5);
        CHECK(vec.empty());
    }

    SUBCASE("Push back and access") {
        vec.push_back(10);
        vec.push_back(20);
        vec.push_back(30);

        CHECK(vec.size() == 3);
        CHECK_FALSE(vec.empty());
        CHECK(vec[0] == 10);
        CHECK(vec[1] == 20);
        CHECK(vec[2] == 30);
    }

    SUBCASE("Push back beyond capacity") {
        for (int i = 0; i < 7; ++i) {
            vec.push_back(i * 10);
        }

        CHECK(vec.size() == 5);
        CHECK(vec.capacity() == 5);
        CHECK(vec[4] == 40);
    }

    SUBCASE("Clear") {
        vec.push_back(10);
        vec.push_back(20);
        vec.clear();

        CHECK(vec.size() == 0);
        CHECK(vec.empty());
    }
}

TEST_CASE("Fixed vector advanced") {
    FASTLED_NAMESPACE_USE;
    FixedVector<int, 5> vec;

    SUBCASE("Pop back") {
        vec.push_back(10);
        vec.push_back(20);
        vec.pop_back();

        CHECK(vec.size() == 1);
        CHECK(vec[0] == 10);
    }

    SUBCASE("Front and back") {
        vec.push_back(10);
        vec.push_back(20);
        vec.push_back(30);

        CHECK(vec.front() == 10);
        CHECK(vec.back() == 30);
    }

    SUBCASE("Iterator") {
        vec.push_back(10);
        vec.push_back(20);
        vec.push_back(30);

        int sum = 0;
        for (auto it = vec.begin(); it != vec.end(); ++it) {
            sum += *it;
        }

        CHECK(sum == 60);
    }

    SUBCASE("Erase") {
        vec.push_back(10);
        vec.push_back(20);
        vec.push_back(30);

        vec.erase(vec.begin() + 1);

        CHECK(vec.size() == 2);
        CHECK(vec[0] == 10);
        CHECK(vec[1] == 30);
    }

    SUBCASE("Find and has") {
        vec.push_back(10);
        vec.push_back(20);
        vec.push_back(30);

        CHECK(vec.has(20));
        CHECK_FALSE(vec.has(40));

        auto it = vec.find(20);
        CHECK(it != vec.end());
        CHECK(*it == 20);

        it = vec.find(40);
        CHECK(it == vec.end());
    }
}

TEST_CASE("Fixed vector with custom type") {
    FASTLED_NAMESPACE_USE;
    struct Point {
        int x, y;
        Point(int x = 0, int y = 0) : x(x), y(y) {}
        bool operator==(const Point& other) const { return x == other.x && y == other.y; }
    };

    FixedVector<Point, 3> vec;

    SUBCASE("Push and access custom type") {
        vec.push_back(Point(1, 2));
        vec.push_back(Point(3, 4));

        CHECK(vec.size() == 2);
        CHECK(vec[0].x == 1);
        CHECK(vec[0].y == 2);
        CHECK(vec[1].x == 3);
        CHECK(vec[1].y == 4);
    }

    SUBCASE("Find custom type") {
        vec.push_back(Point(1, 2));
        vec.push_back(Point(3, 4));

        auto it = vec.find(Point(3, 4));
        CHECK(it != vec.end());
        CHECK(it->x == 3);
        CHECK(it->y == 4);
    }
}
