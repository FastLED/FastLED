
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "fixed_vector.h"

#include "namespace.h"
FASTLED_USING_NAMESPACE

TEST_CASE("Fixed vector simple") {
    FASTLED_USING_NAMESPACE;
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

TEST_CASE("Fixed vector insert") {
    FASTLED_USING_NAMESPACE;
    FixedVector<int, 5> vec;

    SUBCASE("Insert at beginning") {
        vec.push_back(20);
        vec.push_back(30);
        bool inserted = vec.insert(vec.begin(), 10);

        CHECK(inserted);
        CHECK(vec.size() == 3);
        CHECK(vec[0] == 10);
        CHECK(vec[1] == 20);
        CHECK(vec[2] == 30);
    }

    SUBCASE("Insert in middle") {
        vec.push_back(10);
        vec.push_back(30);
        bool inserted = vec.insert(vec.begin() + 1, 20);

        CHECK(inserted);
        CHECK(vec.size() == 3);
        CHECK(vec[0] == 10);
        CHECK(vec[1] == 20);
        CHECK(vec[2] == 30);
    }

    SUBCASE("Insert at end") {
        vec.push_back(10);
        vec.push_back(20);
        bool inserted = vec.insert(vec.end(), 30);

        CHECK(inserted);
        CHECK(vec.size() == 3);
        CHECK(vec[0] == 10);
        CHECK(vec[1] == 20);
        CHECK(vec[2] == 30);
    }

    SUBCASE("Insert when full") {
        vec.push_back(10);
        vec.push_back(20);
        vec.push_back(30);
        vec.push_back(40);
        vec.push_back(50);
        bool inserted = vec.insert(vec.begin() + 2, 25);

        CHECK_FALSE(inserted);
        CHECK(vec.size() == 5);
        CHECK(vec[0] == 10);
        CHECK(vec[1] == 20);
        CHECK(vec[2] == 30);
        CHECK(vec[3] == 40);
        CHECK(vec[4] == 50);
    }
}

TEST_CASE("Fixed vector find_if with predicate") {
    FASTLED_USING_NAMESPACE;
    FixedVector<int, 5> vec;

    SUBCASE("Find even number") {
        vec.push_back(1);
        vec.push_back(2);
        vec.push_back(3);
        vec.push_back(4);
        vec.push_back(5);

        auto it = vec.find_if([](int n) { return n % 2 == 0; });
        CHECK(it != vec.end());
        CHECK(*it == 2);
    }

    SUBCASE("Find number greater than 3") {
        vec.push_back(1);
        vec.push_back(2);
        vec.push_back(3);
        vec.push_back(4);
        vec.push_back(5);

        auto it = vec.find_if([](int n) { return n > 3; });
        CHECK(it != vec.end());
        CHECK(*it == 4);
    }

    SUBCASE("Find non-existent condition") {
        vec.push_back(1);
        vec.push_back(3);
        vec.push_back(5);

        auto it = vec.find_if([](int n) { return n % 2 == 0; });
        CHECK(it == vec.end());
    }

    SUBCASE("Find in empty vector") {
        auto it = vec.find_if([](int n) { return true; });
        CHECK(it == vec.end());
    }
}

TEST_CASE("FixedVector construction and destruction") {
    FASTLED_USING_NAMESPACE;
    
    static int live_object_count = 0;

    struct TestObject {
        int value;
        TestObject(int v = 0) : value(v) { ++live_object_count; }
        ~TestObject() { --live_object_count; }
        TestObject(const TestObject& other) : value(other.value) { ++live_object_count; }
        TestObject& operator=(const TestObject& other) {
            value = other.value;
            return *this;
        }
    };

    SUBCASE("Construction and destruction") {
        REQUIRE_EQ(0, live_object_count);
        live_object_count = 0;
        {
            FixedVector<TestObject, 3> vec;
            CHECK(live_object_count == 0);

            vec.push_back(TestObject(1));
            vec.push_back(TestObject(2));
            vec.push_back(TestObject(3));

            CHECK(live_object_count == 3);  // 3 objects in the vector

            vec.pop_back();
            CHECK(live_object_count == 2);  // 2 objects left in the vector
        }
        // vec goes out of scope here
        REQUIRE_EQ(live_object_count, 0);
    }

    SUBCASE("Clear") {
        live_object_count = 0;
        {
            FixedVector<TestObject, 3> vec;
            vec.push_back(TestObject(1));
            vec.push_back(TestObject(2));

            CHECK(live_object_count == 2);

            vec.clear();

            CHECK(live_object_count == 0);  // All objects should be destroyed after clear
        }
        CHECK(live_object_count == 0);
    }
}

TEST_CASE("Fixed vector advanced") {
    FASTLED_USING_NAMESPACE;
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
    FASTLED_USING_NAMESPACE;
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
