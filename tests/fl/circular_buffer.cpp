#include "fl/circular_buffer.h"
#include "doctest.h"

using namespace fl;

TEST_CASE("fl::StaticCircularBuffer - basic operations") {
    SUBCASE("constructor creates empty buffer") {
        StaticCircularBuffer<int, 5> buffer;
        CHECK(buffer.empty());
        CHECK_EQ(buffer.size(), 0);
        CHECK_EQ(buffer.capacity(), 5);
        CHECK_FALSE(buffer.full());
    }

    SUBCASE("push and pop single element") {
        StaticCircularBuffer<int, 5> buffer;
        buffer.push(42);
        CHECK_FALSE(buffer.empty());
        CHECK_EQ(buffer.size(), 1);

        int value;
        CHECK(buffer.pop(value));
        CHECK_EQ(value, 42);
        CHECK(buffer.empty());
        CHECK_EQ(buffer.size(), 0);
    }

    SUBCASE("push multiple elements") {
        StaticCircularBuffer<int, 5> buffer;
        buffer.push(1);
        buffer.push(2);
        buffer.push(3);
        CHECK_EQ(buffer.size(), 3);

        int value;
        CHECK(buffer.pop(value));
        CHECK_EQ(value, 1);
        CHECK(buffer.pop(value));
        CHECK_EQ(value, 2);
        CHECK(buffer.pop(value));
        CHECK_EQ(value, 3);
        CHECK(buffer.empty());
    }

    SUBCASE("fill to capacity") {
        StaticCircularBuffer<int, 5> buffer;
        for (int i = 0; i < 5; i++) {
            buffer.push(i);
        }
        CHECK_EQ(buffer.size(), 5);
        CHECK(buffer.full());
        CHECK_FALSE(buffer.empty());
    }

    SUBCASE("overflow behavior - overwrites oldest") {
        StaticCircularBuffer<int, 3> buffer;
        buffer.push(1);
        buffer.push(2);
        buffer.push(3);
        CHECK(buffer.full());

        // Push 4th element - should overwrite 1
        buffer.push(4);
        CHECK_EQ(buffer.size(), 3);
        CHECK(buffer.full());

        int value;
        CHECK(buffer.pop(value));
        CHECK_EQ(value, 2); // 1 was overwritten
        CHECK(buffer.pop(value));
        CHECK_EQ(value, 3);
        CHECK(buffer.pop(value));
        CHECK_EQ(value, 4);
        CHECK(buffer.empty());
    }
}

TEST_CASE("fl::StaticCircularBuffer - clear operation") {
    StaticCircularBuffer<int, 5> buffer;
    buffer.push(1);
    buffer.push(2);
    buffer.push(3);
    CHECK_EQ(buffer.size(), 3);

    buffer.clear();
    CHECK(buffer.empty());
    CHECK_EQ(buffer.size(), 0);
    CHECK_FALSE(buffer.full());
}

TEST_CASE("fl::StaticCircularBuffer - pop from empty buffer") {
    StaticCircularBuffer<int, 5> buffer;
    int value = 999;
    CHECK_FALSE(buffer.pop(value));
    CHECK_EQ(value, 999); // Value should not be modified
}

TEST_CASE("fl::StaticCircularBuffer - wraparound behavior") {
    StaticCircularBuffer<int, 4> buffer;

    // Fill buffer
    for (int i = 0; i < 4; i++) {
        buffer.push(i);
    }

    // Pop some elements
    int value;
    buffer.pop(value); // Pop 0
    buffer.pop(value); // Pop 1

    // Push more elements (should wrap around)
    buffer.push(10);
    buffer.push(11);

    // Verify order
    CHECK(buffer.pop(value));
    CHECK_EQ(value, 2);
    CHECK(buffer.pop(value));
    CHECK_EQ(value, 3);
    CHECK(buffer.pop(value));
    CHECK_EQ(value, 10);
    CHECK(buffer.pop(value));
    CHECK_EQ(value, 11);
    CHECK(buffer.empty());
}

TEST_CASE("fl::StaticCircularBuffer - different types") {
    SUBCASE("double type") {
        StaticCircularBuffer<double, 3> buffer;
        buffer.push(3.14);
        buffer.push(2.71);

        double value;
        CHECK(buffer.pop(value));
        CHECK(value == doctest::Approx(3.14));
        CHECK(buffer.pop(value));
        CHECK(value == doctest::Approx(2.71));
    }

    SUBCASE("struct type") {
        struct Point { int x, y; };
        StaticCircularBuffer<Point, 3> buffer;

        buffer.push({1, 2});
        buffer.push({3, 4});

        Point p;
        CHECK(buffer.pop(p));
        CHECK_EQ(p.x, 1);
        CHECK_EQ(p.y, 2);
        CHECK(buffer.pop(p));
        CHECK_EQ(p.x, 3);
        CHECK_EQ(p.y, 4);
    }
}

TEST_CASE("fl::DynamicCircularBuffer - basic operations") {
    SUBCASE("constructor creates empty buffer") {
        DynamicCircularBuffer<int> buffer(5);
        CHECK(buffer.empty());
        CHECK_EQ(buffer.size(), 0);
        CHECK_EQ(buffer.capacity(), 5);
        CHECK_FALSE(buffer.full());
    }

    SUBCASE("push_back and pop_front") {
        DynamicCircularBuffer<int> buffer(5);
        CHECK(buffer.push_back(42));
        CHECK_FALSE(buffer.empty());
        CHECK_EQ(buffer.size(), 1);

        int value;
        CHECK(buffer.pop_front(&value));
        CHECK_EQ(value, 42);
        CHECK(buffer.empty());
    }

    SUBCASE("pop_front without destination") {
        DynamicCircularBuffer<int> buffer(3);
        buffer.push_back(1);
        buffer.push_back(2);
        CHECK_EQ(buffer.size(), 2);

        CHECK(buffer.pop_front()); // Pop without retrieving value
        CHECK_EQ(buffer.size(), 1);

        int value;
        CHECK(buffer.pop_front(&value));
        CHECK_EQ(value, 2);
    }

    SUBCASE("push_front and pop_back") {
        DynamicCircularBuffer<int> buffer(5);
        CHECK(buffer.push_front(42));
        CHECK_EQ(buffer.size(), 1);

        int value;
        CHECK(buffer.pop_back(&value));
        CHECK_EQ(value, 42);
        CHECK(buffer.empty());
    }

    SUBCASE("multiple push_back operations") {
        DynamicCircularBuffer<int> buffer(5);
        for (int i = 0; i < 5; i++) {
            buffer.push_back(i);
        }
        CHECK(buffer.full());
        CHECK_EQ(buffer.size(), 5);

        for (int i = 0; i < 5; i++) {
            int value;
            CHECK(buffer.pop_front(&value));
            CHECK_EQ(value, i);
        }
        CHECK(buffer.empty());
    }

    SUBCASE("overflow - push_back overwrites oldest") {
        DynamicCircularBuffer<int> buffer(3);
        buffer.push_back(1);
        buffer.push_back(2);
        buffer.push_back(3);
        buffer.push_back(4); // Should overwrite 1

        CHECK_EQ(buffer.size(), 3);
        int value;
        buffer.pop_front(&value);
        CHECK_EQ(value, 2); // 1 was overwritten
    }

    SUBCASE("overflow - push_front overwrites oldest") {
        DynamicCircularBuffer<int> buffer(3);
        buffer.push_front(1);
        buffer.push_front(2);
        buffer.push_front(3);
        buffer.push_front(4); // Should overwrite oldest

        CHECK_EQ(buffer.size(), 3);
    }
}

TEST_CASE("fl::DynamicCircularBuffer - front and back access") {
    DynamicCircularBuffer<int> buffer(5);
    buffer.push_back(10);
    buffer.push_back(20);
    buffer.push_back(30);

    CHECK_EQ(buffer.front(), 10);
    CHECK_EQ(buffer.back(), 30);

    // Modify through references
    buffer.front() = 100;
    buffer.back() = 300;

    CHECK_EQ(buffer.front(), 100);
    CHECK_EQ(buffer.back(), 300);
}

TEST_CASE("fl::DynamicCircularBuffer - const front and back access") {
    DynamicCircularBuffer<int> buffer(5);
    buffer.push_back(10);
    buffer.push_back(20);

    const DynamicCircularBuffer<int>& const_buffer = buffer;
    CHECK_EQ(const_buffer.front(), 10);
    CHECK_EQ(const_buffer.back(), 20);
}

TEST_CASE("fl::DynamicCircularBuffer - operator[] access") {
    DynamicCircularBuffer<int> buffer(5);
    buffer.push_back(10);
    buffer.push_back(20);
    buffer.push_back(30);
    buffer.push_back(40);

    CHECK_EQ(buffer[0], 10);
    CHECK_EQ(buffer[1], 20);
    CHECK_EQ(buffer[2], 30);
    CHECK_EQ(buffer[3], 40);

    // Modify through operator[]
    buffer[1] = 200;
    CHECK_EQ(buffer[1], 200);
}

TEST_CASE("fl::DynamicCircularBuffer - const operator[] access") {
    DynamicCircularBuffer<int> buffer(5);
    buffer.push_back(10);
    buffer.push_back(20);

    const DynamicCircularBuffer<int>& const_buffer = buffer;
    CHECK_EQ(const_buffer[0], 10);
    CHECK_EQ(const_buffer[1], 20);
}

TEST_CASE("fl::DynamicCircularBuffer - clear operation") {
    DynamicCircularBuffer<int> buffer(5);
    buffer.push_back(1);
    buffer.push_back(2);
    buffer.push_back(3);
    CHECK_EQ(buffer.size(), 3);

    buffer.clear();
    CHECK(buffer.empty());
    CHECK_EQ(buffer.size(), 0);
}

TEST_CASE("fl::DynamicCircularBuffer - pop from empty buffer") {
    DynamicCircularBuffer<int> buffer(5);
    int value = 999;
    CHECK_FALSE(buffer.pop_front(&value));
    CHECK_EQ(value, 999); // Value should not be modified

    CHECK_FALSE(buffer.pop_back(&value));
    CHECK_EQ(value, 999);
}

TEST_CASE("fl::DynamicCircularBuffer - wraparound with operator[]") {
    DynamicCircularBuffer<int> buffer(4);

    // Fill buffer
    for (int i = 0; i < 4; i++) {
        buffer.push_back(i);
    }

    // Pop some and push more to cause wraparound
    buffer.pop_front();
    buffer.pop_front();
    buffer.push_back(10);
    buffer.push_back(11);

    // Verify access through operator[]
    CHECK_EQ(buffer[0], 2);
    CHECK_EQ(buffer[1], 3);
    CHECK_EQ(buffer[2], 10);
    CHECK_EQ(buffer[3], 11);
}

TEST_CASE("fl::DynamicCircularBuffer - bidirectional operations") {
    DynamicCircularBuffer<int> buffer(5);

    // Mix push_back, push_front, pop_front, pop_back
    buffer.push_back(5);
    buffer.push_front(3);
    buffer.push_back(7);
    buffer.push_front(1);

    // Buffer should be: 1, 3, 5, 7
    CHECK_EQ(buffer.size(), 4);
    CHECK_EQ(buffer[0], 1);
    CHECK_EQ(buffer[1], 3);
    CHECK_EQ(buffer[2], 5);
    CHECK_EQ(buffer[3], 7);

    int value;
    buffer.pop_back(&value);
    CHECK_EQ(value, 7);
    buffer.pop_front(&value);
    CHECK_EQ(value, 1);

    // Buffer should be: 3, 5
    CHECK_EQ(buffer.size(), 2);
    CHECK_EQ(buffer.front(), 3);
    CHECK_EQ(buffer.back(), 5);
}

TEST_CASE("fl::DynamicCircularBuffer - different types") {
    SUBCASE("double type") {
        DynamicCircularBuffer<double> buffer(3);
        buffer.push_back(3.14);
        buffer.push_back(2.71);

        double value;
        buffer.pop_front(&value);
        CHECK(value == doctest::Approx(3.14));
        buffer.pop_front(&value);
        CHECK(value == doctest::Approx(2.71));
    }

    SUBCASE("struct type") {
        struct Point { int x, y; };
        DynamicCircularBuffer<Point> buffer(3);

        buffer.push_back({1, 2});
        buffer.push_back({3, 4});

        Point p;
        buffer.pop_front(&p);
        CHECK_EQ(p.x, 1);
        CHECK_EQ(p.y, 2);
        buffer.pop_front(&p);
        CHECK_EQ(p.x, 3);
        CHECK_EQ(p.y, 4);
    }
}

TEST_CASE("fl::CircularBuffer - alias for DynamicCircularBuffer") {
    // Verify that CircularBuffer is an alias for DynamicCircularBuffer
    CircularBuffer<int> buffer(5);
    buffer.push_back(42);

    int value;
    CHECK(buffer.pop_front(&value));
    CHECK_EQ(value, 42);
}

TEST_CASE("fl::StaticCircularBuffer - capacity is constexpr") {
    StaticCircularBuffer<int, 10> buffer;
    constexpr auto cap = buffer.capacity();
    CHECK_EQ(cap, 10);
}

TEST_CASE("fl::DynamicCircularBuffer - stress test with many operations") {
    DynamicCircularBuffer<int> buffer(100);

    // Push many elements
    for (int i = 0; i < 100; i++) {
        buffer.push_back(i);
    }
    CHECK(buffer.full());
    CHECK_EQ(buffer.size(), 100);

    // Pop half
    for (int i = 0; i < 50; i++) {
        int value;
        buffer.pop_front(&value);
        CHECK_EQ(value, i);
    }
    CHECK_EQ(buffer.size(), 50);

    // Push more
    for (int i = 100; i < 150; i++) {
        buffer.push_back(i);
    }
    CHECK(buffer.full());

    // Verify contents
    for (int i = 0; i < 100; i++) {
        CHECK_EQ(buffer[i], 50 + i);
    }
}

TEST_CASE("fl::StaticCircularBuffer - single element capacity") {
    StaticCircularBuffer<int, 1> buffer;
    CHECK_EQ(buffer.capacity(), 1);

    buffer.push(42);
    CHECK(buffer.full());
    CHECK_EQ(buffer.size(), 1);

    int value;
    CHECK(buffer.pop(value));
    CHECK_EQ(value, 42);
    CHECK(buffer.empty());
}

TEST_CASE("fl::DynamicCircularBuffer - single element capacity") {
    DynamicCircularBuffer<int> buffer(1);
    CHECK_EQ(buffer.capacity(), 1);

    buffer.push_back(42);
    CHECK(buffer.full());
    CHECK_EQ(buffer.size(), 1);

    int value;
    CHECK(buffer.pop_front(&value));
    CHECK_EQ(value, 42);
    CHECK(buffer.empty());
}
