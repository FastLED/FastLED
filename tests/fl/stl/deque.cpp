#include "fl/stl/compiler_control.h"
#include "fl/stl/deque.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/new.h"
#include "test.h"
#include "fl/stl/move.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

FL_TEST_CASE("fl::deque - default constructor") {
    deque<int> dq;
    FL_CHECK(dq.empty());
    FL_CHECK_EQ(dq.size(), 0u);
}

FL_TEST_CASE("fl::deque - constructor with count and value") {
    FL_SUBCASE("int type") {
        deque<int> dq(5, 42);
        FL_CHECK_EQ(dq.size(), 5u);
        FL_CHECK_FALSE(dq.empty());
        for (size_t i = 0; i < 5; ++i) {
            FL_CHECK_EQ(dq[i], 42);
        }
    }

    FL_SUBCASE("float type") {
        deque<float> dq(3, 3.14f);
        FL_CHECK_EQ(dq.size(), 3u);
        for (size_t i = 0; i < 3; ++i) {
            FL_CHECK_EQ(dq[i], 3.14f);
        }
    }
}

FL_TEST_CASE("fl::deque - initializer list constructor") {
    deque<int> dq = {1, 2, 3, 4, 5};
    FL_CHECK_EQ(dq.size(), 5u);
    FL_CHECK_EQ(dq[0], 1);
    FL_CHECK_EQ(dq[1], 2);
    FL_CHECK_EQ(dq[2], 3);
    FL_CHECK_EQ(dq[3], 4);
    FL_CHECK_EQ(dq[4], 5);
}

FL_TEST_CASE("fl::deque - copy constructor") {
    deque<int> dq1 = {1, 2, 3};
    deque<int> dq2(dq1);

    FL_CHECK_EQ(dq2.size(), 3u);
    FL_CHECK_EQ(dq2[0], 1);
    FL_CHECK_EQ(dq2[1], 2);
    FL_CHECK_EQ(dq2[2], 3);

    // Modify original to ensure deep copy
    dq1[0] = 99;
    FL_CHECK_EQ(dq2[0], 1);
}

FL_TEST_CASE("fl::deque - move constructor") {
    deque<int> dq1 = {1, 2, 3};
    deque<int> dq2(fl::move(dq1));

    FL_CHECK_EQ(dq2.size(), 3u);
    FL_CHECK_EQ(dq2[0], 1);
    FL_CHECK_EQ(dq2[1], 2);
    FL_CHECK_EQ(dq2[2], 3);

    // Original should be empty after move
    FL_CHECK(dq1.empty());
}

FL_TEST_CASE("fl::deque - copy assignment") {
    deque<int> dq1 = {1, 2, 3};
    deque<int> dq2;
    dq2 = dq1;

    FL_CHECK_EQ(dq2.size(), 3u);
    FL_CHECK_EQ(dq2[0], 1);
    FL_CHECK_EQ(dq2[1], 2);
    FL_CHECK_EQ(dq2[2], 3);

    // Test self-assignment
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING_SELF_ASSIGN_OVERLOADED
    dq1 = dq1;
    FL_DISABLE_WARNING_POP
    FL_CHECK_EQ(dq1.size(), 3u);
}

FL_TEST_CASE("fl::deque - move assignment") {
    deque<int> dq1 = {1, 2, 3};
    deque<int> dq2;
    dq2 = fl::move(dq1);

    FL_CHECK_EQ(dq2.size(), 3u);
    FL_CHECK_EQ(dq2[0], 1);

    // Original should be empty after move
    FL_CHECK(dq1.empty());
}

FL_TEST_CASE("fl::deque - push_back") {
    deque<int> dq;

    dq.push_back(1);
    FL_CHECK_EQ(dq.size(), 1u);
    FL_CHECK_EQ(dq[0], 1);

    dq.push_back(2);
    FL_CHECK_EQ(dq.size(), 2u);
    FL_CHECK_EQ(dq[1], 2);

    dq.push_back(3);
    FL_CHECK_EQ(dq.size(), 3u);
    FL_CHECK_EQ(dq[2], 3);
}

FL_TEST_CASE("fl::deque - push_front") {
    deque<int> dq;

    dq.push_front(1);
    FL_CHECK_EQ(dq.size(), 1u);
    FL_CHECK_EQ(dq[0], 1);

    dq.push_front(2);
    FL_CHECK_EQ(dq.size(), 2u);
    FL_CHECK_EQ(dq[0], 2);
    FL_CHECK_EQ(dq[1], 1);

    dq.push_front(3);
    FL_CHECK_EQ(dq.size(), 3u);
    FL_CHECK_EQ(dq[0], 3);
    FL_CHECK_EQ(dq[1], 2);
    FL_CHECK_EQ(dq[2], 1);
}

FL_TEST_CASE("fl::deque - pop_back") {
    deque<int> dq = {1, 2, 3, 4, 5};

    dq.pop_back();
    FL_CHECK_EQ(dq.size(), 4u);
    FL_CHECK_EQ(dq[3], 4);

    dq.pop_back();
    FL_CHECK_EQ(dq.size(), 3u);
    FL_CHECK_EQ(dq[2], 3);

    // Pop from empty should be safe
    deque<int> empty_dq;
    empty_dq.pop_back();
    FL_CHECK(empty_dq.empty());
}

FL_TEST_CASE("fl::deque - pop_front") {
    deque<int> dq = {1, 2, 3, 4, 5};

    dq.pop_front();
    FL_CHECK_EQ(dq.size(), 4u);
    FL_CHECK_EQ(dq[0], 2);

    dq.pop_front();
    FL_CHECK_EQ(dq.size(), 3u);
    FL_CHECK_EQ(dq[0], 3);

    // Pop from empty should be safe
    deque<int> empty_dq;
    empty_dq.pop_front();
    FL_CHECK(empty_dq.empty());
}

FL_TEST_CASE("fl::deque - front and back") {
    deque<int> dq = {1, 2, 3, 4, 5};

    FL_CHECK_EQ(dq.front(), 1);
    FL_CHECK_EQ(dq.back(), 5);

    dq.front() = 10;
    dq.back() = 50;

    FL_CHECK_EQ(dq.front(), 10);
    FL_CHECK_EQ(dq.back(), 50);
}

FL_TEST_CASE("fl::deque - at with bounds checking") {
    deque<int> dq = {1, 2, 3};

    FL_CHECK_EQ(dq.at(0), 1);
    FL_CHECK_EQ(dq.at(1), 2);
    FL_CHECK_EQ(dq.at(2), 3);

    // Out of bounds returns front element (embedded behavior)
    FL_CHECK_EQ(dq.at(100), dq.front());
}

FL_TEST_CASE("fl::deque - operator[]") {
    deque<int> dq = {10, 20, 30, 40, 50};

    FL_CHECK_EQ(dq[0], 10);
    FL_CHECK_EQ(dq[2], 30);
    FL_CHECK_EQ(dq[4], 50);

    dq[1] = 200;
    FL_CHECK_EQ(dq[1], 200);
}

FL_TEST_CASE("fl::deque - clear") {
    deque<int> dq = {1, 2, 3, 4, 5};
    FL_CHECK_EQ(dq.size(), 5u);

    dq.clear();
    FL_CHECK(dq.empty());
    FL_CHECK_EQ(dq.size(), 0u);
}

FL_TEST_CASE("fl::deque - resize") {
    FL_SUBCASE("resize up") {
        deque<int> dq = {1, 2, 3};
        dq.resize(5);
        FL_CHECK_EQ(dq.size(), 5u);
        FL_CHECK_EQ(dq[0], 1);
        FL_CHECK_EQ(dq[1], 2);
        FL_CHECK_EQ(dq[2], 3);
        FL_CHECK_EQ(dq[3], 0); // Default value
        FL_CHECK_EQ(dq[4], 0);
    }

    FL_SUBCASE("resize up with value") {
        deque<int> dq = {1, 2, 3};
        dq.resize(5, 99);
        FL_CHECK_EQ(dq.size(), 5u);
        FL_CHECK_EQ(dq[3], 99);
        FL_CHECK_EQ(dq[4], 99);
    }

    FL_SUBCASE("resize down") {
        deque<int> dq = {1, 2, 3, 4, 5};
        dq.resize(3);
        FL_CHECK_EQ(dq.size(), 3u);
        FL_CHECK_EQ(dq[0], 1);
        FL_CHECK_EQ(dq[1], 2);
        FL_CHECK_EQ(dq[2], 3);
    }

    FL_SUBCASE("resize to same size") {
        deque<int> dq = {1, 2, 3};
        dq.resize(3);
        FL_CHECK_EQ(dq.size(), 3u);
        FL_CHECK_EQ(dq[0], 1);
        FL_CHECK_EQ(dq[1], 2);
        FL_CHECK_EQ(dq[2], 3);
    }
}

FL_TEST_CASE("fl::deque - swap") {
    deque<int> dq1 = {1, 2, 3};
    deque<int> dq2 = {4, 5, 6, 7};

    dq1.swap(dq2);

    FL_CHECK_EQ(dq1.size(), 4u);
    FL_CHECK_EQ(dq1[0], 4);
    FL_CHECK_EQ(dq1[3], 7);

    FL_CHECK_EQ(dq2.size(), 3u);
    FL_CHECK_EQ(dq2[0], 1);
    FL_CHECK_EQ(dq2[2], 3);

    // Test self-swap
    dq1.swap(dq1);
    FL_CHECK_EQ(dq1.size(), 4u);
}

FL_TEST_CASE("fl::deque - iterator") {
    deque<int> dq = {1, 2, 3, 4, 5};

    FL_SUBCASE("forward iteration") {
        int expected = 1;
        for (auto it = dq.begin(); it != dq.end(); ++it) {
            FL_CHECK_EQ(*it, expected++);
        }
    }

    FL_SUBCASE("range-based for loop") {
        int expected = 1;
        for (int val : dq) {
            FL_CHECK_EQ(val, expected++);
        }
    }

    FL_SUBCASE("iterator modification") {
        for (auto it = dq.begin(); it != dq.end(); ++it) {
            *it *= 2;
        }
        FL_CHECK_EQ(dq[0], 2);
        FL_CHECK_EQ(dq[1], 4);
        FL_CHECK_EQ(dq[2], 6);
    }

    FL_SUBCASE("post-increment") {
        auto it = dq.begin();
        auto it2 = it++;
        FL_CHECK_EQ(*it2, 1);
        FL_CHECK_EQ(*it, 2);
    }

    FL_SUBCASE("pre-decrement") {
        auto it = dq.end();
        --it;
        FL_CHECK_EQ(*it, 5);
    }

    FL_SUBCASE("post-decrement") {
        auto it = dq.end();
        --it;
        auto it2 = it--;
        FL_CHECK_EQ(*it2, 5);
        FL_CHECK_EQ(*it, 4);
    }
}

FL_TEST_CASE("fl::deque - const_iterator") {
    const deque<int> dq = {1, 2, 3, 4, 5};

    FL_SUBCASE("forward iteration") {
        int expected = 1;
        for (auto it = dq.begin(); it != dq.end(); ++it) {
            FL_CHECK_EQ(*it, expected++);
        }
    }

    FL_SUBCASE("range-based for loop") {
        int expected = 1;
        for (const int val : dq) {
            FL_CHECK_EQ(val, expected++);
        }
    }
}

FL_TEST_CASE("fl::deque - capacity management") {
    deque<int> dq;

    // Initially empty
    FL_CHECK_EQ(dq.capacity(), 0u);

    // Push elements to trigger capacity growth
    for (int i = 0; i < 10; ++i) {
        dq.push_back(i);
    }

    FL_CHECK_GE(dq.capacity(), 10u);
    FL_CHECK_EQ(dq.size(), 10u);
}

FL_TEST_CASE("fl::deque - mixed push_front and push_back") {
    deque<int> dq;

    dq.push_back(3);
    dq.push_back(4);
    dq.push_front(2);
    dq.push_front(1);
    dq.push_back(5);

    FL_CHECK_EQ(dq.size(), 5u);
    FL_CHECK_EQ(dq[0], 1);
    FL_CHECK_EQ(dq[1], 2);
    FL_CHECK_EQ(dq[2], 3);
    FL_CHECK_EQ(dq[3], 4);
    FL_CHECK_EQ(dq[4], 5);
}

FL_TEST_CASE("fl::deque - mixed pop_front and pop_back") {
    deque<int> dq = {1, 2, 3, 4, 5};

    dq.pop_front();
    FL_CHECK_EQ(dq.size(), 4u);
    FL_CHECK_EQ(dq[0], 2);

    dq.pop_back();
    FL_CHECK_EQ(dq.size(), 3u);
    FL_CHECK_EQ(dq[2], 4);

    dq.pop_front();
    FL_CHECK_EQ(dq.size(), 2u);
    FL_CHECK_EQ(dq[0], 3);
    FL_CHECK_EQ(dq[1], 4);
}

FL_TEST_CASE("fl::deque - wrap-around behavior") {
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

    FL_CHECK_EQ(dq[0], 101);
    FL_CHECK_EQ(dq[1], 100);
    FL_CHECK_EQ(dq[2], 2);
    FL_CHECK_EQ(dq[3], 3);
    FL_CHECK_EQ(dq[4], 4);
}

FL_TEST_CASE("fl::deque - stress test with many operations") {
    deque<int> dq;

    // Push many elements
    for (int i = 0; i < 100; ++i) {
        dq.push_back(i);
    }
    FL_CHECK_EQ(dq.size(), 100u);

    // Remove half from front
    for (int i = 0; i < 50; ++i) {
        dq.pop_front();
    }
    FL_CHECK_EQ(dq.size(), 50u);
    FL_CHECK_EQ(dq.front(), 50);

    // Push more to front
    for (int i = 0; i < 25; ++i) {
        dq.push_front(i);
    }
    FL_CHECK_EQ(dq.size(), 75u);
}

FL_TEST_CASE("fl::deque - typedefs") {
    FL_SUBCASE("deque_int") {
        deque_int dq = {1, 2, 3};
        FL_CHECK_EQ(dq.size(), 3u);
        FL_CHECK_EQ(dq[0], 1);
    }

    FL_SUBCASE("deque_float") {
        deque_float dq = {1.5f, 2.5f, 3.5f};
        FL_CHECK_EQ(dq.size(), 3u);
        FL_CHECK_EQ(dq[0], 1.5f);
    }

    FL_SUBCASE("deque_double") {
        deque_double dq = {1.5, 2.5, 3.5};
        FL_CHECK_EQ(dq.size(), 3u);
        FL_CHECK_EQ(dq[0], 1.5);
    }
}

FL_TEST_CASE("fl::deque - empty deque operations") {
    deque<int> dq;

    FL_CHECK(dq.empty());
    FL_CHECK_EQ(dq.size(), 0u);

    // Ensure begin() == end() for empty deque
    FL_CHECK(dq.begin() == dq.end());
}

FL_TEST_CASE("fl::deque - move semantics with push") {
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
    FL_CHECK_EQ(dq[0].value, 42);
    FL_CHECK_EQ(obj.value, -1); // Moved from

    MoveOnlyType obj2(99);
    dq.push_front(fl::move(obj2));
    FL_CHECK_EQ(dq[0].value, 99);
    FL_CHECK_EQ(obj2.value, -1); // Moved from
}

// ============================================================================
// NEW FUNCTIONALITY TESTS (Iterator Arithmetic, Insert/Erase, etc.)
// ============================================================================

FL_TEST_CASE("fl::deque - iterator arithmetic") {
    deque<int> dq = {10, 20, 30, 40, 50};

    FL_SUBCASE("iterator operator+") {
        auto it = dq.begin();
        auto it2 = it + 2;
        FL_CHECK_EQ(*it2, 30);
        FL_CHECK_EQ(*it, 10);  // Original unchanged
    }

    FL_SUBCASE("iterator operator+=") {
        auto it = dq.begin();
        it += 2;
        FL_CHECK_EQ(*it, 30);
    }

    FL_SUBCASE("iterator operator-") {
        auto it = dq.end();
        --it;  // Back to last element (5)
        auto it2 = it - 2;
        FL_CHECK_EQ(*it2, 30);
    }

    FL_SUBCASE("iterator operator-=") {
        auto it = dq.end();
        --it;
        it -= 2;
        FL_CHECK_EQ(*it, 30);
    }

    FL_SUBCASE("iterator operator[]") {
        auto it = dq.begin();
        FL_CHECK_EQ(it[0], 10);
        FL_CHECK_EQ(it[2], 30);
        FL_CHECK_EQ(it[4], 50);
    }

    FL_SUBCASE("iterator difference operator") {
        auto it1 = dq.begin();
        auto it2 = dq.begin() + 3;
        FL_CHECK_EQ(it2 - it1, 3u);
    }

    FL_SUBCASE("iterator comparison operators") {
        auto it1 = dq.begin();
        auto it2 = dq.begin() + 2;
        auto it3 = dq.begin() + 2;

        FL_CHECK(it1 < it2);
        FL_CHECK(it1 <= it2);
        FL_CHECK(it2 > it1);
        FL_CHECK(it2 >= it1);
        FL_CHECK(it2 == it3);
        FL_CHECK(it1 != it2);
    }
}

FL_TEST_CASE("fl::deque - const_iterator arithmetic") {
    const deque<int> dq = {10, 20, 30, 40, 50};

    FL_SUBCASE("const_iterator operator+") {
        auto it = dq.begin();
        auto it2 = it + 2;
        FL_CHECK_EQ(*it2, 30);
    }

    FL_SUBCASE("const_iterator operator[]") {
        auto it = dq.begin();
        FL_CHECK_EQ(it[0], 10);
        FL_CHECK_EQ(it[2], 30);
    }
}

FL_TEST_CASE("fl::deque - cbegin/cend/crbegin/crend") {
    deque<int> dq = {1, 2, 3, 4, 5};

    FL_SUBCASE("cbegin/cend") {
        int expected = 1;
        for (auto it = dq.cbegin(); it != dq.cend(); ++it) {
            FL_CHECK_EQ(*it, expected++);
        }
    }

    FL_SUBCASE("crbegin/crend") {
        int expected = 5;
        for (auto it = dq.crbegin(); it != dq.crend(); ++it) {
            FL_CHECK_EQ(*it, expected--);
        }
    }
}

FL_TEST_CASE("fl::deque - insert single element") {
    deque<int> dq = {1, 2, 4, 5};

    FL_SUBCASE("insert at position") {
        auto it = dq.insert(dq.begin() + 2, 3);
        FL_CHECK_EQ(dq.size(), 5u);
        FL_CHECK_EQ(dq[0], 1);
        FL_CHECK_EQ(dq[1], 2);
        FL_CHECK_EQ(dq[2], 3);
        FL_CHECK_EQ(dq[3], 4);
        FL_CHECK_EQ(dq[4], 5);
        FL_CHECK_EQ(*it, 3);
    }

    FL_SUBCASE("insert at front") {
        dq.insert(dq.begin(), 0);
        FL_CHECK_EQ(dq.size(), 5u);
        FL_CHECK_EQ(dq[0], 0);
        FL_CHECK_EQ(dq[1], 1);
    }

    FL_SUBCASE("insert at back") {
        dq.insert(dq.end(), 6);
        FL_CHECK_EQ(dq.size(), 5u);
        FL_CHECK_EQ(dq[4], 6);
    }
}

FL_TEST_CASE("fl::deque - insert multiple elements") {
    deque<int> dq = {1, 2, 5};

    auto it = dq.insert(dq.begin() + 2, 3, 3);  // Insert three 3's at position 2
    FL_CHECK_EQ(dq.size(), 6u);
    FL_CHECK_EQ(dq[0], 1);
    FL_CHECK_EQ(dq[1], 2);
    FL_CHECK_EQ(dq[2], 3);
    FL_CHECK_EQ(dq[3], 3);
    FL_CHECK_EQ(dq[4], 3);
    FL_CHECK_EQ(dq[5], 5);
    FL_CHECK_EQ(*it, 3);
}

FL_TEST_CASE("fl::deque - erase single element") {
    deque<int> dq = {1, 2, 3, 4, 5};

    FL_SUBCASE("erase from middle") {
        auto it = dq.erase(dq.begin() + 2);
        FL_CHECK_EQ(dq.size(), 4u);
        FL_CHECK_EQ(dq[0], 1);
        FL_CHECK_EQ(dq[1], 2);
        FL_CHECK_EQ(dq[2], 4);  // Shifted left
        FL_CHECK_EQ(dq[3], 5);
        FL_CHECK_EQ(*it, 4);  // Iterator points to next element
    }

    FL_SUBCASE("erase from front") {
        dq.erase(dq.begin());
        FL_CHECK_EQ(dq.size(), 4u);
        FL_CHECK_EQ(dq[0], 2);
    }

    FL_SUBCASE("erase from back") {
        dq.erase(dq.end() - 1);
        FL_CHECK_EQ(dq.size(), 4u);
        FL_CHECK_EQ(dq[3], 4);
    }
}

FL_TEST_CASE("fl::deque - erase range") {
    deque<int> dq = {1, 2, 3, 4, 5, 6, 7};

    auto it = dq.erase(dq.begin() + 2, dq.begin() + 5);  // Erase 3, 4, 5
    FL_CHECK_EQ(dq.size(), 4u);
    FL_CHECK_EQ(dq[0], 1);
    FL_CHECK_EQ(dq[1], 2);
    FL_CHECK_EQ(dq[2], 6);
    FL_CHECK_EQ(dq[3], 7);
    FL_CHECK_EQ(*it, 6);
}

FL_TEST_CASE("fl::deque - emplace_back") {
    struct TestObj {
        int x, y;
        TestObj(int a, int b) : x(a), y(b) {}
    };

    deque<TestObj> dq;
    auto& obj = dq.emplace_back(10, 20);
    FL_CHECK_EQ(dq.size(), 1u);
    FL_CHECK_EQ(dq[0].x, 10);
    FL_CHECK_EQ(dq[0].y, 20);
    FL_CHECK_EQ(obj.x, 10);
}

FL_TEST_CASE("fl::deque - emplace_front") {
    struct TestObj {
        int x, y;
        TestObj(int a, int b) : x(a), y(b) {}
    };

    deque<TestObj> dq;
    dq.push_back(TestObj(5, 6));
    auto& obj = dq.emplace_front(10, 20);
    FL_CHECK_EQ(dq.size(), 2u);
    FL_CHECK_EQ(dq[0].x, 10);
    FL_CHECK_EQ(dq[0].y, 20);
    FL_CHECK_EQ(dq[1].x, 5);
    FL_CHECK_EQ(obj.x, 10);
}

FL_TEST_CASE("fl::deque - emplace at position") {
    struct TestObj {
        int x, y;
        TestObj(int a, int b) : x(a), y(b) {}
    };

    deque<TestObj> dq;
    dq.push_back(TestObj(1, 2));
    dq.push_back(TestObj(5, 6));

    auto it = dq.emplace(dq.begin() + 1, 10, 20);
    FL_CHECK_EQ(dq.size(), 3u);
    FL_CHECK_EQ(dq[0].x, 1);
    FL_CHECK_EQ(dq[1].x, 10);
    FL_CHECK_EQ(dq[2].x, 5);
    FL_CHECK_EQ((*it).x, 10);
}

FL_TEST_CASE("fl::deque - assign") {
    deque<int> dq = {1, 2, 3};

    dq.assign(5, 42);
    FL_CHECK_EQ(dq.size(), 5u);
    for (size_t i = 0; i < 5; ++i) {
        FL_CHECK_EQ(dq[i], 42);
    }
}

FL_TEST_CASE("fl::deque - reserve") {
    deque<int> dq;

    FL_CHECK_EQ(dq.capacity(), 0u);

    dq.reserve(100);
    FL_CHECK_GE(dq.capacity(), 100u);
    FL_CHECK_EQ(dq.size(), 0u);  // Size unchanged

    // Add elements without reallocation
    for (int i = 0; i < 100; ++i) {
        dq.push_back(i);
    }
    FL_CHECK_EQ(dq.size(), 100u);
}

FL_TEST_CASE("fl::deque - max_size") {
    deque<int> dq;
    FL_CHECK_GT(dq.max_size(), 0u);
}

FL_TEST_CASE("fl::deque - shrink_to_fit") {
    deque<int> dq;

    // Add many elements
    for (int i = 0; i < 100; ++i) {
        dq.push_back(i);
    }
    fl::size large_capacity = dq.capacity();

    // Remove most elements
    while (dq.size() > 5) {
        dq.pop_back();
    }

    // Capacity should still be large
    FL_CHECK_EQ(dq.capacity(), large_capacity);

    // Shrink
    dq.shrink_to_fit();
    FL_CHECK_EQ(dq.capacity(), 5u);
    FL_CHECK_EQ(dq.size(), 5u);

    // Data should still be intact
    for (int i = 0; i < 5; ++i) {
        FL_CHECK_EQ(dq[i], i);
    }
}

FL_TEST_CASE("fl::deque - shrink_to_fit empty") {
    deque<int> dq = {1, 2, 3, 4, 5};

    dq.clear();
    dq.shrink_to_fit();

    FL_CHECK_EQ(dq.capacity(), 0u);
    FL_CHECK(dq.empty());
}

FL_TEST_CASE("fl::deque - comparison operator==") {
    deque<int> dq1 = {1, 2, 3};
    deque<int> dq2 = {1, 2, 3};
    deque<int> dq3 = {1, 2, 4};
    deque<int> dq4 = {1, 2};

    FL_CHECK(dq1 == dq2);
    FL_CHECK_FALSE(dq1 == dq3);
    FL_CHECK_FALSE(dq1 == dq4);
}

FL_TEST_CASE("fl::deque - comparison operator!=") {
    deque<int> dq1 = {1, 2, 3};
    deque<int> dq2 = {1, 2, 3};
    deque<int> dq3 = {1, 2, 4};

    FL_CHECK_FALSE(dq1 != dq2);
    FL_CHECK(dq1 != dq3);
}

FL_TEST_CASE("fl::deque - comparison operators < <= > >=") {
    deque<int> dq1 = {1, 2, 3};
    deque<int> dq2 = {1, 2, 4};
    deque<int> dq3 = {1, 2, 3};

    FL_CHECK(dq1 < dq2);
    FL_CHECK(dq1 <= dq2);
    FL_CHECK(dq2 > dq1);
    FL_CHECK(dq2 >= dq1);
    FL_CHECK(dq1 <= dq3);
    FL_CHECK(dq1 >= dq3);
}

FL_TEST_CASE("fl::deque - insert and erase stress test") {
    deque<int> dq = {1, 2, 3, 4, 5};

    // Insert in middle
    dq.insert(dq.begin() + 2, 99);
    FL_CHECK_EQ(dq[2], 99);
    FL_CHECK_EQ(dq.size(), 6u);

    // Erase from middle
    dq.erase(dq.begin() + 2);
    FL_CHECK_EQ(dq[2], 3);
    FL_CHECK_EQ(dq.size(), 5u);

    // Erase range
    dq.erase(dq.begin() + 1, dq.begin() + 3);
    FL_CHECK_EQ(dq.size(), 3u);
    FL_CHECK_EQ(dq[0], 1);
    FL_CHECK_EQ(dq[1], 4);
    FL_CHECK_EQ(dq[2], 5);
}

FL_TEST_CASE("fl::deque - mixed operations with new methods") {
    deque<int> dq;

    // Reserve space
    dq.reserve(50);

    // Add elements via push
    for (int i = 0; i < 5; ++i) {
        dq.push_back(i);
    }
    // dq = [0, 1, 2, 3, 4], size = 5

    // Insert in middle
    dq.insert(dq.begin() + 2, 99);
    // dq = [0, 1, 99, 2, 3, 4], size = 6

    // Emplace at position
    dq.emplace(dq.begin() + 3, 88);
    // dq = [0, 1, 99, 88, 2, 3, 4], size = 7

    // Erase
    dq.erase(dq.begin() + 4);
    // dq = [0, 1, 99, 88, 3, 4], size = 6

    // Check final state
    FL_CHECK_EQ(dq.size(), 6u);
    FL_CHECK_EQ(dq[0], 0);
    FL_CHECK_EQ(dq[1], 1);
    FL_CHECK_EQ(dq[2], 99);
    FL_CHECK_EQ(dq[3], 88);
    FL_CHECK_EQ(dq[4], 3);
    FL_CHECK_EQ(dq[5], 4);
}

} // FL_TEST_FILE
