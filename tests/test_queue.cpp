// Test file for fl::queue

#include "test.h"
#include "fl/queue.h"
#include "fl/vector.h"
#include "fl/deque.h"

using namespace fl;

TEST_CASE("Basic Queue Operations") {
    fl::queue<int> q;

    SUBCASE("Initial state") {
        CHECK(q.empty());
        CHECK_EQ(q.size(), 0);
    }

    SUBCASE("Push and front/back access") {
        q.push(10);
        q.push(20);
        q.push(30);

        CHECK_FALSE(q.empty());
        CHECK_EQ(q.size(), 3);
        CHECK_EQ(q.front(), 10);  // First in
        CHECK_EQ(q.back(), 30);   // Last in
    }

    SUBCASE("FIFO behavior (pop)") {
        q.push(10);
        q.push(20);
        q.push(30);

        CHECK_EQ(q.front(), 10);
        q.pop();
        CHECK_EQ(q.front(), 20);
        q.pop();
        CHECK_EQ(q.front(), 30);
        q.pop();
        CHECK(q.empty());
    }

    SUBCASE("Size changes correctly") {
        CHECK_EQ(q.size(), 0);
        
        q.push(1);
        CHECK_EQ(q.size(), 1);
        
        q.push(2);
        CHECK_EQ(q.size(), 2);
        
        q.pop();
        CHECK_EQ(q.size(), 1);
        
        q.pop();
        CHECK_EQ(q.size(), 0);
        CHECK(q.empty());
    }
}

TEST_CASE("Queue Copy and Move Semantics") {
    SUBCASE("Copy constructor") {
        fl::queue<int> q1;
        q1.push(1);
        q1.push(2);
        q1.push(3);

        fl::queue<int> q2(q1);
        CHECK_EQ(q2.size(), 3);
        CHECK_EQ(q2.front(), 1);
        CHECK_EQ(q2.back(), 3);
        
        // Original should be unchanged
        CHECK_EQ(q1.size(), 3);
        CHECK_EQ(q1.front(), 1);
    }

    SUBCASE("Copy assignment") {
        fl::queue<int> q1;
        q1.push(1);
        q1.push(2);

        fl::queue<int> q2;
        q2.push(99);  // Different data
        
        q2 = q1;
        CHECK_EQ(q2.size(), 2);
        CHECK_EQ(q2.front(), 1);
        CHECK_EQ(q2.back(), 2);
    }

    SUBCASE("Move constructor") {
        fl::queue<int> q1;
        q1.push(1);
        q1.push(2);
        q1.push(3);

        fl::queue<int> q2(fl::move(q1));
        CHECK_EQ(q2.size(), 3);
        CHECK_EQ(q2.front(), 1);
        CHECK_EQ(q2.back(), 3);
    }

    SUBCASE("Move assignment") {
        fl::queue<int> q1;
        q1.push(1);
        q1.push(2);

        fl::queue<int> q2;
        q2 = fl::move(q1);
        CHECK_EQ(q2.size(), 2);
        CHECK_EQ(q2.front(), 1);
        CHECK_EQ(q2.back(), 2);
    }
}

TEST_CASE("Queue with Custom Container") {
    // Test that queue can use a different underlying container
    // In this case, we'll test with fl::vector as the underlying container
    // Note: This requires that the underlying container supports:
    // push_back, pop_front, front, back, empty, size, swap
    
    SUBCASE("Queue with deque container (default)") {
        fl::queue<int, fl::deque<int>> q;
        q.push(1);
        q.push(2);
        CHECK_EQ(q.size(), 2);
        CHECK_EQ(q.front(), 1);
        q.pop();
        CHECK_EQ(q.front(), 2);
    }
}

TEST_CASE("Queue Swap Functionality") {
    fl::queue<int> q1;
    fl::queue<int> q2;

    q1.push(1);
    q1.push(2);
    
    q2.push(10);
    q2.push(20);
    q2.push(30);

    q1.swap(q2);

    CHECK_EQ(q1.size(), 3);
    CHECK_EQ(q1.front(), 10);
    CHECK_EQ(q1.back(), 30);

    CHECK_EQ(q2.size(), 2);
    CHECK_EQ(q2.front(), 1);
    CHECK_EQ(q2.back(), 2);
}

TEST_CASE("Queue Container Access") {
    fl::queue<int> q;
    q.push(1);
    q.push(2);
    q.push(3);

    SUBCASE("Const container access") {
        const auto& container = q.get_container();
        CHECK_EQ(container.size(), 3);
        CHECK_EQ(container.front(), 1);
        CHECK_EQ(container.back(), 3);
    }

    SUBCASE("Non-const container access") {
        auto& container = q.get_container();
        CHECK_EQ(container.size(), 3);
        
        // We can modify through the container
        container.push_back(4);
        CHECK_EQ(q.size(), 4);
        CHECK_EQ(q.back(), 4);
    }
}

TEST_CASE("Queue with Move-Only Type") {
    // Test with a type that can only be moved, not copied
    struct MoveOnly {
        int value;
        
        MoveOnly() : value(0) {}
        explicit MoveOnly(int v) : value(v) {}
        
        // Move constructor and assignment
        MoveOnly(MoveOnly&& other) : value(other.value) {
            other.value = -1;  // Mark as moved
        }
        
        MoveOnly& operator=(MoveOnly&& other) {
            if (this != &other) {
                value = other.value;
                other.value = -1;  // Mark as moved
            }
            return *this;
        }
        
        // Delete copy constructor and assignment
        MoveOnly(const MoveOnly&) = delete;
        MoveOnly& operator=(const MoveOnly&) = delete;
    };

    fl::queue<MoveOnly> q;
    
    MoveOnly item1(42);
    q.push(fl::move(item1));
    CHECK_EQ(item1.value, -1);  // Should be moved
    
    MoveOnly item2(99);
    q.push(fl::move(item2));
    CHECK_EQ(item2.value, -1);  // Should be moved
    
    CHECK_EQ(q.size(), 2);
    CHECK_EQ(q.front().value, 42);
    CHECK_EQ(q.back().value, 99);
    
    q.pop();
    CHECK_EQ(q.front().value, 99);
}

TEST_CASE("Queue Stress Test") {
    fl::queue<int> q;
    
    // Push a lot of elements
    const int num_elements = 1000;
    for (int i = 0; i < num_elements; ++i) {
        q.push(i);
    }
    
    CHECK_EQ(q.size(), num_elements);
    CHECK_EQ(q.front(), 0);
    CHECK_EQ(q.back(), num_elements - 1);
    
    // Pop all elements and verify FIFO order
    for (int i = 0; i < num_elements; ++i) {
        CHECK_EQ(q.front(), i);
        q.pop();
    }
    
    CHECK(q.empty());
} 
