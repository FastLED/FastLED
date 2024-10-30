
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "fx/detail/circular_buffer.h"

#include "namespace.h"
FASTLED_USING_NAMESPACE

TEST_CASE("circular_buffer basic operations") {
    CircularBuffer<int> buffer(5);

    CHECK(buffer.empty());
    CHECK_EQ(buffer.size(), 0);

    buffer.push_back(1);
    buffer.push_back(2);
    buffer.push_back(3);

    CHECK_EQ(buffer.size(), 3);
    CHECK_FALSE(buffer.empty());
    CHECK_FALSE(buffer.full());

    CHECK_EQ(buffer.front(), 1);
    CHECK_EQ(buffer.back(), 3);

    int value;
    CHECK_EQ(buffer.pop_front(&value), true);
    CHECK_EQ(value, 1);
    CHECK_EQ(buffer.size(), 2);
    CHECK_EQ(buffer.front(), 2);
}

TEST_CASE("circular_buffer operator[]") {
    CircularBuffer<int> buffer(5);

    CHECK(buffer.empty());
    CHECK_EQ(buffer.size(), 0);

    buffer.push_back(1);
    buffer.push_back(2);
    CHECK_EQ(buffer.size(), 2);
    CHECK_EQ(buffer[0], 1);
    CHECK_EQ(buffer[1], 2);
    buffer.pop_front(nullptr);
    CHECK_EQ(2, buffer[0]);
    buffer.push_back(3);
    CHECK_EQ(2, buffer[0]);
    CHECK_EQ(3, buffer[1]);
    buffer.pop_back(nullptr);
    CHECK_EQ(2, buffer[0]);
}

TEST_CASE("circular_buffer overflow behavior") {
    CircularBuffer<int> buffer(3);

    buffer.push_back(1);
    buffer.push_back(2);
    buffer.push_back(3);
    CHECK(buffer.full());

    buffer.push_back(4);
    CHECK(buffer.full());
    CHECK_EQ(buffer.size(), 3);

    int value;
    CHECK_EQ(buffer.pop_front(&value), true);
    CHECK_EQ(value, 2);
    CHECK_EQ(buffer.pop_front(&value), true);
    CHECK_EQ(value, 3);
    CHECK_EQ(buffer.pop_front(&value), true);
    CHECK_EQ(value, 4);
    CHECK(buffer.empty());

    // CHECK_EQ(buffer.pop_front(), 0);  // Returns default-constructed int (0) when empty
    CHECK_EQ(buffer.pop_front(&value), false);
}

TEST_CASE("circular_buffer edge cases") {
    CircularBuffer<int> buffer(1);

    CHECK(buffer.empty());
    CHECK_FALSE(buffer.full());

    buffer.push_back(42);
    CHECK_FALSE(buffer.empty());
    CHECK(buffer.full());

    buffer.push_back(43);
    CHECK_EQ(buffer.front(), 43);
    CHECK_EQ(buffer.back(), 43);

    int value;
    bool ok = buffer.pop_front(&value);
    CHECK(ok);
    CHECK_EQ(value, 43);
    CHECK(buffer.empty());
}

TEST_CASE("circular_buffer clear operation") {
    CircularBuffer<int> buffer(5);

    buffer.push_back(1);
    buffer.push_back(2);
    buffer.push_back(3);

    CHECK_EQ(buffer.size(), 3);

    buffer.clear();

    CHECK(buffer.empty());
    CHECK_EQ(buffer.size(), 0);

    buffer.push_back(4);
    CHECK_EQ(buffer.front(), 4);
    CHECK_EQ(buffer.back(), 4);
}

TEST_CASE("circular_buffer indexing") {
    CircularBuffer<int> buffer(5);

    buffer.push_back(10);
    buffer.push_back(20);
    buffer.push_back(30);

    CHECK_EQ(buffer[0], 10);
    CHECK_EQ(buffer[1], 20);
    CHECK_EQ(buffer[2], 30);

    buffer.pop_front(nullptr);
    buffer.push_back(40);

    CHECK_EQ(buffer[0], 20);
    CHECK_EQ(buffer[1], 30);
    CHECK_EQ(buffer[2], 40);
}

TEST_CASE("circular_buffer with custom type") {
    struct CustomType {
        int value;
        CustomType(int v = 0) : value(v) {}
        bool operator==(const CustomType& other) const { return value == other.value; }
    };

    CircularBuffer<CustomType> buffer(3);

    buffer.push_back(CustomType(1));
    buffer.push_back(CustomType(2));
    buffer.push_back(CustomType(3));

    CHECK_EQ(buffer.front().value, 1);
    CHECK_EQ(buffer.back().value, 3);

    buffer.push_back(CustomType(4));

    CustomType value;
    CHECK_EQ(buffer.pop_front(&value), true);
    CHECK_EQ(value.value, 2);
    CHECK_EQ(buffer.pop_front(&value), true);
    CHECK_EQ(value.value, 3);
    CHECK_EQ(buffer.pop_front(&value), true);
    CHECK_EQ(value.value, 4);
}

TEST_CASE("circular_buffer writing to full buffer") {
    CircularBuffer<int> buffer(3);

    // Fill the buffer
    buffer.push_back(1);
    buffer.push_back(2);
    buffer.push_back(3);
    CHECK(buffer.full());

    // Write to full buffer
    buffer.push_back(4);
    CHECK(buffer.full());
    CHECK_EQ(buffer.size(), 3);

    // Check that the oldest element was overwritten
    CHECK_EQ(buffer[0], 2);
    CHECK_EQ(buffer[1], 3);
    CHECK_EQ(buffer[2], 4);

    // Write multiple elements to full buffer
    buffer.push_back(5);
    buffer.push_back(6);
    CHECK(buffer.full());
    CHECK_EQ(buffer.size(), 3);

    // Check that the buffer contains only the most recent elements
    CHECK_EQ(buffer[0], 4);
    CHECK_EQ(buffer[1], 5);
    CHECK_EQ(buffer[2], 6);

    // Verify front() and back()
    CHECK_EQ(buffer.front(), 4);
    CHECK_EQ(buffer.back(), 6);

    // Pop all elements and verify
    //CHECK_EQ(buffer.pop_front(), 4);
    //CHECK_EQ(buffer.pop_front(), 5);
    //CHECK_EQ(buffer.pop_front(), 6);
    int value;
    CHECK_EQ(buffer.pop_front(&value), true);
    CHECK_EQ(value, 4);
    CHECK_EQ(buffer.pop_front(&value), true);
    CHECK_EQ(value, 5);
    CHECK_EQ(buffer.pop_front(&value), true);
    CHECK_EQ(value, 6);
    CHECK(buffer.empty());
}

#if 1

TEST_CASE("circular_buffer zero capacity") {
    CircularBuffer<int> buffer(0);

    CHECK(buffer.empty());
    CHECK(buffer.full());
    CHECK_EQ(buffer.size(), 0);

    // Attempt to push an element
    buffer.push_back(1);

    // Buffer should now contain one element
    CHECK(buffer.empty());
    CHECK(buffer.full());
    CHECK_EQ(buffer.size(), 0);

    // Attempt to pop an element
    // CHECK_EQ(buffer.pop_front(), 0);
    int value;
    CHECK_EQ(buffer.pop_front(&value), false);
    
    // Buffer should be empty again
    CHECK(buffer.empty());
    CHECK(buffer.full());
    CHECK_EQ(buffer.size(), 0);
}

#endif  

TEST_CASE("circular_buffer pop_back operation") {
    CircularBuffer<int> buffer(5);

    buffer.push_back(1);
    buffer.push_back(2);
    buffer.push_back(3);

    int value;
    CHECK_EQ(buffer.pop_back(&value), true);
    CHECK_EQ(value, 3);
    CHECK_EQ(buffer.size(), 2);
    CHECK_EQ(buffer.back(), 2);

    CHECK_EQ(buffer.pop_back(&value), true);
    CHECK_EQ(value, 2);
    CHECK_EQ(buffer.size(), 1);
    CHECK_EQ(buffer.front(), 1);
    CHECK_EQ(buffer.back(), 1);

    CHECK_EQ(buffer.pop_back(&value), true);
    CHECK_EQ(value, 1);
    CHECK(buffer.empty());

    CHECK_EQ(buffer.pop_back(&value), false);
}

TEST_CASE("circular_buffer push_front operation") {
    CircularBuffer<int> buffer(3);

    buffer.push_front(1);
    buffer.push_front(2);
    buffer.push_front(3);

    CHECK_EQ(buffer.size(), 3);
    CHECK_EQ(buffer.front(), 3);
    CHECK_EQ(buffer.back(), 1);

    buffer.push_front(4);
    CHECK_EQ(buffer.size(), 3);
    CHECK_EQ(buffer.front(), 4);
    CHECK_EQ(buffer.back(), 2);

    int value;
    CHECK_EQ(buffer.pop_back(&value), true);
    CHECK_EQ(value, 2);
    CHECK_EQ(buffer.pop_back(&value), true);
    CHECK_EQ(value, 3);
    CHECK_EQ(buffer.pop_back(&value), true);
    CHECK_EQ(value, 4);
    CHECK(buffer.empty());
}
