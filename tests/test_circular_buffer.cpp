
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "fl/circular_buffer.h"

#include "fl/namespace.h"

using namespace fl;

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

TEST_CASE("circular_buffer large data block operations") {
    CircularBuffer<int> buffer(100);

    // Test adding a large block of data (10x buffer capacity)
    const size_t large_data_size = 1000;
    for (size_t i = 0; i < large_data_size; ++i) {
        buffer.push_back(static_cast<int>(i));
    }

    // Buffer should be full and contain the last 100 elements
    CHECK(buffer.full());
    CHECK_EQ(buffer.size(), 100);
    CHECK_EQ(buffer.front(), static_cast<int>(large_data_size - 100));
    CHECK_EQ(buffer.back(), static_cast<int>(large_data_size - 1));

    // Verify all elements in buffer are correct (last 100 values)
    for (size_t i = 0; i < buffer.size(); ++i) {
        CHECK_EQ(buffer[i], static_cast<int>(large_data_size - 100 + i));
    }

    // Test popping all elements to ensure integrity
    int value;
    for (size_t i = 0; i < 100; ++i) {
        CHECK_EQ(buffer.pop_front(&value), true);
        CHECK_EQ(value, static_cast<int>(large_data_size - 100 + i));
    }
    CHECK(buffer.empty());
}

TEST_CASE("circular_buffer stress test with rapid operations") {
    CircularBuffer<int> buffer(50);

    // Stress test: rapid push/pop operations
    const size_t stress_iterations = 1000; // Reduced from 10000
    size_t total_added = 0;
    size_t total_removed = 0;
    FL_UNUSED(total_added);
    FL_UNUSED(total_removed);
    
    for (size_t i = 0; i < stress_iterations; ++i) {
        // Add 3 elements
        buffer.push_back(static_cast<int>(i * 3));
        buffer.push_back(static_cast<int>(i * 3 + 1));
        buffer.push_back(static_cast<int>(i * 3 + 2));
        total_added += 3;

        // Remove 2 elements when possible
        if (buffer.size() >= 2) {
            int dummy;
            buffer.pop_front(&dummy);
            buffer.pop_front(&dummy);
            total_removed += 2;
        }

        // Verify buffer doesn't exceed capacity
        CHECK(buffer.size() <= 50);
        CHECK_FALSE(buffer.size() > buffer.capacity());
    }

    // Verify the buffer is within expected bounds
    // Since we add 3 and remove 2 per iteration (when possible),
    // the buffer should have grown but be constrained by capacity
    CHECK(buffer.size() <= 50);
    CHECK_FALSE(buffer.empty());
    
    // Additional verification: if we've done many iterations,
    // the buffer should be close to or at capacity
    if (stress_iterations >= 50) {
        CHECK(buffer.size() >= 45); // Should be near capacity
    }
}

TEST_CASE("circular_buffer wraparound integrity test") {
    CircularBuffer<int> buffer(7); // Prime number for interesting wraparound behavior

    // Fill buffer multiple times to test wraparound
    const size_t cycles = 20; // Reduced from 100
    for (size_t cycle = 0; cycle < cycles; ++cycle) {
        // Fill the buffer completely
        for (size_t i = 0; i < 7; ++i) {
            buffer.push_back(static_cast<int>(cycle * 7 + i));
        }

        // Verify buffer state after each fill
        CHECK(buffer.full());
        CHECK_EQ(buffer.size(), 7);

        // Check that elements are in correct order
        for (size_t i = 0; i < 7; ++i) {
            CHECK_EQ(buffer[i], static_cast<int>(cycle * 7 + i));
        }

        // Empty the buffer partially (remove 3 elements)
        for (size_t i = 0; i < 3; ++i) {
            int value;
            CHECK_EQ(buffer.pop_front(&value), true);
            CHECK_EQ(value, static_cast<int>(cycle * 7 + i));
        }

        CHECK_EQ(buffer.size(), 4);
    }
}

TEST_CASE("circular_buffer bulk operations without overflow") {
    CircularBuffer<int> buffer(1000);

    // Test 1: Add elements in chunks
    const size_t chunk_size = 250;
    const size_t num_chunks = 8; // Reduced from 20 - Total: 2000 elements (2x buffer capacity)

    for (size_t chunk = 0; chunk < num_chunks; ++chunk) {
        for (size_t i = 0; i < chunk_size; ++i) {
            buffer.push_back(static_cast<int>(chunk * chunk_size + i));
        }

        // Verify buffer constraints are maintained
        CHECK(buffer.size() <= 1000);
        CHECK_FALSE(buffer.size() > buffer.capacity());
    }

    // Buffer should be full with the last 1000 elements
    CHECK(buffer.full());
    CHECK_EQ(buffer.size(), 1000);

    // Verify the buffer contains the correct last 1000 elements
    size_t expected_start = (num_chunks * chunk_size) - 1000; // 1000
    for (size_t i = 0; i < buffer.size(); ++i) {
        CHECK_EQ(buffer[i], static_cast<int>(expected_start + i));
    }

    // Test 2: Mixed operations (bulk add, partial remove)
    for (size_t round = 0; round < 20; ++round) { // Reduced from 100
        // Add 50 elements
        for (size_t i = 0; i < 50; ++i) {
            buffer.push_back(static_cast<int>(10000 + round * 50 + i));
        }

        // Remove 30 elements
        for (size_t i = 0; i < 30; ++i) {
            int dummy;
            if (!buffer.empty()) {
                buffer.pop_front(&dummy);
            }
        }

        // Verify constraints
        CHECK(buffer.size() <= 1000);
        CHECK_FALSE(buffer.size() > buffer.capacity());
    }
}

TEST_CASE("circular_buffer edge case with maximum indices") {
    CircularBuffer<int> buffer(5);

    // Test that internal index calculations don't overflow
    // by simulating many wraparounds
    const size_t many_operations = 10000; // Reduced from 1000000

    for (size_t i = 0; i < many_operations; ++i) {
        buffer.push_back(static_cast<int>(i % 100));

        // Occasionally pop elements to create varied states
        if (i % 7 == 0 && !buffer.empty()) {
            int dummy;
            buffer.pop_front(&dummy);
        }

        // Verify buffer integrity
        CHECK(buffer.size() <= 5);
        CHECK_FALSE(buffer.size() > buffer.capacity());

        // Occasionally verify element access doesn't crash
        if (i % 1000 == 0 && !buffer.empty()) {
            volatile int test_front = buffer.front();
            volatile int test_back = buffer.back();
            for (size_t j = 0; j < buffer.size(); ++j) {
                volatile int test_indexed = buffer[j];
                (void)test_front;
                (void)test_back;
                (void)test_indexed;
            }
        }
    }
}

TEST_CASE("circular_buffer memory safety with alternating operations") {
    CircularBuffer<int> buffer(10);

    // Pattern that could potentially cause memory issues if buffer logic is wrong
    for (size_t iteration = 0; iteration < 100; ++iteration) { // Reduced from 1000
        // Fill buffer
        for (size_t i = 0; i < 15; ++i) { // Overfill by 5
            buffer.push_back(static_cast<int>(iteration * 15 + i));
        }

        // Verify buffer state
        CHECK(buffer.full());
        CHECK_EQ(buffer.size(), 10);

        // Empty buffer completely
        while (!buffer.empty()) {
            int value;
            CHECK_EQ(buffer.pop_front(&value), true);
        }

        CHECK(buffer.empty());
        CHECK_EQ(buffer.size(), 0);

        // Fill from back
        for (size_t i = 0; i < 12; ++i) { // Overfill by 2
            buffer.push_front(static_cast<int>(iteration * 12 + i));
        }

        // Verify buffer state
        CHECK(buffer.full());
        CHECK_EQ(buffer.size(), 10);

        // Empty from back
        while (!buffer.empty()) {
            int value;
            CHECK_EQ(buffer.pop_back(&value), true);
        }

        CHECK(buffer.empty());
    }
}
