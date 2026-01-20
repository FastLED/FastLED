#include "fl/stl/deque.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/stl/move.h"

using namespace fl;

TEST_CASE("fl::deque - default constructor") {
    deque<int> dq;
    CHECK(dq.empty());
    CHECK_EQ(dq.size(), 0u);
}

TEST_CASE("fl::deque - constructor with count and value") {
    SUBCASE("int type") {
        deque<int> dq(5, 42);
        CHECK_EQ(dq.size(), 5u);
        CHECK_FALSE(dq.empty());
        for (size_t i = 0; i < 5; ++i) {
            CHECK_EQ(dq[i], 42);
        }
    }

    SUBCASE("float type") {
        deque<float> dq(3, 3.14f);
        CHECK_EQ(dq.size(), 3u);
        for (size_t i = 0; i < 3; ++i) {
            CHECK_EQ(dq[i], 3.14f);
        }
    }
}

TEST_CASE("fl::deque - initializer list constructor") {
    deque<int> dq = {1, 2, 3, 4, 5};
    CHECK_EQ(dq.size(), 5u);
    CHECK_EQ(dq[0], 1);
    CHECK_EQ(dq[1], 2);
    CHECK_EQ(dq[2], 3);
    CHECK_EQ(dq[3], 4);
    CHECK_EQ(dq[4], 5);
}

TEST_CASE("fl::deque - copy constructor") {
    deque<int> dq1 = {1, 2, 3};
    deque<int> dq2(dq1);

    CHECK_EQ(dq2.size(), 3u);
    CHECK_EQ(dq2[0], 1);
    CHECK_EQ(dq2[1], 2);
    CHECK_EQ(dq2[2], 3);

    // Modify original to ensure deep copy
    dq1[0] = 99;
    CHECK_EQ(dq2[0], 1);
}

TEST_CASE("fl::deque - move constructor") {
    deque<int> dq1 = {1, 2, 3};
    deque<int> dq2(fl::move(dq1));

    CHECK_EQ(dq2.size(), 3u);
    CHECK_EQ(dq2[0], 1);
    CHECK_EQ(dq2[1], 2);
    CHECK_EQ(dq2[2], 3);

    // Original should be empty after move
    CHECK(dq1.empty());
}

TEST_CASE("fl::deque - copy assignment") {
    deque<int> dq1 = {1, 2, 3};
    deque<int> dq2;
    dq2 = dq1;

    CHECK_EQ(dq2.size(), 3u);
    CHECK_EQ(dq2[0], 1);
    CHECK_EQ(dq2[1], 2);
    CHECK_EQ(dq2[2], 3);

    // Test self-assignment
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wself-assign-overloaded"
    dq1 = dq1;
    #pragma GCC diagnostic pop
    CHECK_EQ(dq1.size(), 3u);
}

TEST_CASE("fl::deque - move assignment") {
    deque<int> dq1 = {1, 2, 3};
    deque<int> dq2;
    dq2 = fl::move(dq1);

    CHECK_EQ(dq2.size(), 3u);
    CHECK_EQ(dq2[0], 1);

    // Original should be empty after move
    CHECK(dq1.empty());
}

TEST_CASE("fl::deque - push_back") {
    deque<int> dq;

    dq.push_back(1);
    CHECK_EQ(dq.size(), 1u);
    CHECK_EQ(dq[0], 1);

    dq.push_back(2);
    CHECK_EQ(dq.size(), 2u);
    CHECK_EQ(dq[1], 2);

    dq.push_back(3);
    CHECK_EQ(dq.size(), 3u);
    CHECK_EQ(dq[2], 3);
}

TEST_CASE("fl::deque - push_front") {
    deque<int> dq;

    dq.push_front(1);
    CHECK_EQ(dq.size(), 1u);
    CHECK_EQ(dq[0], 1);

    dq.push_front(2);
    CHECK_EQ(dq.size(), 2u);
    CHECK_EQ(dq[0], 2);
    CHECK_EQ(dq[1], 1);

    dq.push_front(3);
    CHECK_EQ(dq.size(), 3u);
    CHECK_EQ(dq[0], 3);
    CHECK_EQ(dq[1], 2);
    CHECK_EQ(dq[2], 1);
}

TEST_CASE("fl::deque - pop_back") {
    deque<int> dq = {1, 2, 3, 4, 5};

    dq.pop_back();
    CHECK_EQ(dq.size(), 4u);
    CHECK_EQ(dq[3], 4);

    dq.pop_back();
    CHECK_EQ(dq.size(), 3u);
    CHECK_EQ(dq[2], 3);

    // Pop from empty should be safe
    deque<int> empty_dq;
    empty_dq.pop_back();
    CHECK(empty_dq.empty());
}

TEST_CASE("fl::deque - pop_front") {
    deque<int> dq = {1, 2, 3, 4, 5};

    dq.pop_front();
    CHECK_EQ(dq.size(), 4u);
    CHECK_EQ(dq[0], 2);

    dq.pop_front();
    CHECK_EQ(dq.size(), 3u);
    CHECK_EQ(dq[0], 3);

    // Pop from empty should be safe
    deque<int> empty_dq;
    empty_dq.pop_front();
    CHECK(empty_dq.empty());
}

TEST_CASE("fl::deque - front and back") {
    deque<int> dq = {1, 2, 3, 4, 5};

    CHECK_EQ(dq.front(), 1);
    CHECK_EQ(dq.back(), 5);

    dq.front() = 10;
    dq.back() = 50;

    CHECK_EQ(dq.front(), 10);
    CHECK_EQ(dq.back(), 50);
}

TEST_CASE("fl::deque - at with bounds checking") {
    deque<int> dq = {1, 2, 3};

    CHECK_EQ(dq.at(0), 1);
    CHECK_EQ(dq.at(1), 2);
    CHECK_EQ(dq.at(2), 3);

    // Out of bounds returns front element (embedded behavior)
    CHECK_EQ(dq.at(100), dq.front());
}

TEST_CASE("fl::deque - operator[]") {
    deque<int> dq = {10, 20, 30, 40, 50};

    CHECK_EQ(dq[0], 10);
    CHECK_EQ(dq[2], 30);
    CHECK_EQ(dq[4], 50);

    dq[1] = 200;
    CHECK_EQ(dq[1], 200);
}

TEST_CASE("fl::deque - clear") {
    deque<int> dq = {1, 2, 3, 4, 5};
    CHECK_EQ(dq.size(), 5u);

    dq.clear();
    CHECK(dq.empty());
    CHECK_EQ(dq.size(), 0u);
}

TEST_CASE("fl::deque - resize") {
    SUBCASE("resize up") {
        deque<int> dq = {1, 2, 3};
        dq.resize(5);
        CHECK_EQ(dq.size(), 5u);
        CHECK_EQ(dq[0], 1);
        CHECK_EQ(dq[1], 2);
        CHECK_EQ(dq[2], 3);
        CHECK_EQ(dq[3], 0); // Default value
        CHECK_EQ(dq[4], 0);
    }

    SUBCASE("resize up with value") {
        deque<int> dq = {1, 2, 3};
        dq.resize(5, 99);
        CHECK_EQ(dq.size(), 5u);
        CHECK_EQ(dq[3], 99);
        CHECK_EQ(dq[4], 99);
    }

    SUBCASE("resize down") {
        deque<int> dq = {1, 2, 3, 4, 5};
        dq.resize(3);
        CHECK_EQ(dq.size(), 3u);
        CHECK_EQ(dq[0], 1);
        CHECK_EQ(dq[1], 2);
        CHECK_EQ(dq[2], 3);
    }

    SUBCASE("resize to same size") {
        deque<int> dq = {1, 2, 3};
        dq.resize(3);
        CHECK_EQ(dq.size(), 3u);
        CHECK_EQ(dq[0], 1);
        CHECK_EQ(dq[1], 2);
        CHECK_EQ(dq[2], 3);
    }
}

TEST_CASE("fl::deque - swap") {
    deque<int> dq1 = {1, 2, 3};
    deque<int> dq2 = {4, 5, 6, 7};

    dq1.swap(dq2);

    CHECK_EQ(dq1.size(), 4u);
    CHECK_EQ(dq1[0], 4);
    CHECK_EQ(dq1[3], 7);

    CHECK_EQ(dq2.size(), 3u);
    CHECK_EQ(dq2[0], 1);
    CHECK_EQ(dq2[2], 3);

    // Test self-swap
    dq1.swap(dq1);
    CHECK_EQ(dq1.size(), 4u);
}

TEST_CASE("fl::deque - iterator") {
    deque<int> dq = {1, 2, 3, 4, 5};

    SUBCASE("forward iteration") {
        int expected = 1;
        for (auto it = dq.begin(); it != dq.end(); ++it) {
            CHECK_EQ(*it, expected++);
        }
    }

    SUBCASE("range-based for loop") {
        int expected = 1;
        for (int val : dq) {
            CHECK_EQ(val, expected++);
        }
    }

    SUBCASE("iterator modification") {
        for (auto it = dq.begin(); it != dq.end(); ++it) {
            *it *= 2;
        }
        CHECK_EQ(dq[0], 2);
        CHECK_EQ(dq[1], 4);
        CHECK_EQ(dq[2], 6);
    }

    SUBCASE("post-increment") {
        auto it = dq.begin();
        auto it2 = it++;
        CHECK_EQ(*it2, 1);
        CHECK_EQ(*it, 2);
    }

    SUBCASE("pre-decrement") {
        auto it = dq.end();
        --it;
        CHECK_EQ(*it, 5);
    }

    SUBCASE("post-decrement") {
        auto it = dq.end();
        --it;
        auto it2 = it--;
        CHECK_EQ(*it2, 5);
        CHECK_EQ(*it, 4);
    }
}

TEST_CASE("fl::deque - const_iterator") {
    const deque<int> dq = {1, 2, 3, 4, 5};

    SUBCASE("forward iteration") {
        int expected = 1;
        for (auto it = dq.begin(); it != dq.end(); ++it) {
            CHECK_EQ(*it, expected++);
        }
    }

    SUBCASE("range-based for loop") {
        int expected = 1;
        for (const int val : dq) {
            CHECK_EQ(val, expected++);
        }
    }
}

TEST_CASE("fl::deque - capacity management") {
    deque<int> dq;

    // Initially empty
    CHECK_EQ(dq.capacity(), 0u);

    // Push elements to trigger capacity growth
    for (int i = 0; i < 10; ++i) {
        dq.push_back(i);
    }

    CHECK_GE(dq.capacity(), 10u);
    CHECK_EQ(dq.size(), 10u);
}

TEST_CASE("fl::deque - mixed push_front and push_back") {
    deque<int> dq;

    dq.push_back(3);
    dq.push_back(4);
    dq.push_front(2);
    dq.push_front(1);
    dq.push_back(5);

    CHECK_EQ(dq.size(), 5u);
    CHECK_EQ(dq[0], 1);
    CHECK_EQ(dq[1], 2);
    CHECK_EQ(dq[2], 3);
    CHECK_EQ(dq[3], 4);
    CHECK_EQ(dq[4], 5);
}

TEST_CASE("fl::deque - mixed pop_front and pop_back") {
    deque<int> dq = {1, 2, 3, 4, 5};

    dq.pop_front();
    CHECK_EQ(dq.size(), 4u);
    CHECK_EQ(dq[0], 2);

    dq.pop_back();
    CHECK_EQ(dq.size(), 3u);
    CHECK_EQ(dq[2], 4);

    dq.pop_front();
    CHECK_EQ(dq.size(), 2u);
    CHECK_EQ(dq[0], 3);
    CHECK_EQ(dq[1], 4);
}

TEST_CASE("fl::deque - wrap-around behavior") {
    deque<int> dq;

    // Push to back
    for (int i = 0; i < 5; ++i) {
        dq.push_back(i);
    }

    // Pop from front
    dq.pop_front();
    dq.pop_front();

    // Push to front (should wrap around)
    dq.push_front(100);
    dq.push_front(101);

    CHECK_EQ(dq[0], 101);
    CHECK_EQ(dq[1], 100);
    CHECK_EQ(dq[2], 2);
    CHECK_EQ(dq[3], 3);
    CHECK_EQ(dq[4], 4);
}

TEST_CASE("fl::deque - stress test with many operations") {
    deque<int> dq;

    // Push many elements
    for (int i = 0; i < 100; ++i) {
        dq.push_back(i);
    }
    CHECK_EQ(dq.size(), 100u);

    // Remove half from front
    for (int i = 0; i < 50; ++i) {
        dq.pop_front();
    }
    CHECK_EQ(dq.size(), 50u);
    CHECK_EQ(dq.front(), 50);

    // Push more to front
    for (int i = 0; i < 25; ++i) {
        dq.push_front(i);
    }
    CHECK_EQ(dq.size(), 75u);
}

TEST_CASE("fl::deque - typedefs") {
    SUBCASE("deque_int") {
        deque_int dq = {1, 2, 3};
        CHECK_EQ(dq.size(), 3u);
        CHECK_EQ(dq[0], 1);
    }

    SUBCASE("deque_float") {
        deque_float dq = {1.5f, 2.5f, 3.5f};
        CHECK_EQ(dq.size(), 3u);
        CHECK_EQ(dq[0], 1.5f);
    }

    SUBCASE("deque_double") {
        deque_double dq = {1.5, 2.5, 3.5};
        CHECK_EQ(dq.size(), 3u);
        CHECK_EQ(dq[0], 1.5);
    }
}

TEST_CASE("fl::deque - empty deque operations") {
    deque<int> dq;

    CHECK(dq.empty());
    CHECK_EQ(dq.size(), 0u);

    // Ensure begin() == end() for empty deque
    CHECK(dq.begin() == dq.end());
}

TEST_CASE("fl::deque - move semantics with push") {
    struct MoveOnlyType {
        int value;
        MoveOnlyType(int v) : value(v) {}
        MoveOnlyType(const MoveOnlyType&) = delete;
        MoveOnlyType& operator=(const MoveOnlyType&) = delete;
        MoveOnlyType(MoveOnlyType&& other) : value(other.value) { other.value = -1; }
        MoveOnlyType& operator=(MoveOnlyType&&) = default;
    };

    deque<MoveOnlyType> dq;

    MoveOnlyType obj(42);
    dq.push_back(fl::move(obj));
    CHECK_EQ(dq[0].value, 42);
    CHECK_EQ(obj.value, -1); // Moved from

    MoveOnlyType obj2(99);
    dq.push_front(fl::move(obj2));
    CHECK_EQ(dq[0].value, 99);
    CHECK_EQ(obj2.value, -1); // Moved from
}
