#include "fl/stl/priority_queue.h"
#include "fl/stl/vector.h"
#include "fl/stl/stdint.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/stl/allocator.h"
#include "fl/stl/utility.h"

TEST_CASE("priority_queue_stable: basic operations") {
    fl::priority_queue_stable<int> queue;

    REQUIRE(queue.empty());
    REQUIRE_EQ(queue.size(), 0u);

    queue.push(5);
    REQUIRE_FALSE(queue.empty());
    REQUIRE_EQ(queue.size(), 1u);
    REQUIRE_EQ(queue.top(), 5);

    queue.pop();
    REQUIRE(queue.empty());
}

TEST_CASE("priority_queue_stable: ordering") {
    fl::priority_queue_stable<int> queue;

    // Push in random order
    queue.push(3);
    queue.push(1);
    queue.push(4);
    queue.push(2);

    // Should pop in ascending order (min-heap)
    REQUIRE_EQ(queue.top(), 1);
    queue.pop();
    REQUIRE_EQ(queue.top(), 2);
    queue.pop();
    REQUIRE_EQ(queue.top(), 3);
    queue.pop();
    REQUIRE_EQ(queue.top(), 4);
    queue.pop();
    REQUIRE(queue.empty());
}

TEST_CASE("priority_queue_stable: FIFO for equal priorities") {
    fl::priority_queue_stable<int> queue;

    // Push elements with same priority - should maintain FIFO order
    queue.push(5);
    queue.push(5);
    queue.push(5);

    // All have same priority
    REQUIRE_EQ(queue.size(), 3u);

    for (int i = 0; i < 3; i++) {
        REQUIRE_EQ(queue.top(), 5);
        queue.pop();
    }
    REQUIRE(queue.empty());
}

struct ScheduledCall {
    uint32_t mExecuteAt;
    int mId;  // Used to track FIFO order

    // Comparison operator for min-heap (earlier times have higher priority)
    // priority_queue_stable uses fl::greater by default, so use natural comparison
    bool operator<(const ScheduledCall& other) const {
        return mExecuteAt < other.mExecuteAt;  // Natural: smaller time = higher priority
    }
};

TEST_CASE("priority_queue_stable: scheduled calls with different times") {
    fl::priority_queue_stable<ScheduledCall> queue;  // Uses default fl::greater for min-heap

    // Schedule calls at different times
    queue.push({1000, 1});
    queue.push({3000, 2});
    queue.push({2000, 3});

    // Should execute in time order
    REQUIRE_EQ(queue.top().mExecuteAt, 1000u);
    REQUIRE_EQ(queue.top().mId, 1);
    queue.pop();

    REQUIRE_EQ(queue.top().mExecuteAt, 2000u);
    REQUIRE_EQ(queue.top().mId, 3);
    queue.pop();

    REQUIRE_EQ(queue.top().mExecuteAt, 3000u);
    REQUIRE_EQ(queue.top().mId, 2);
    queue.pop();

    REQUIRE(queue.empty());
}

TEST_CASE("priority_queue_stable: scheduled calls with same time (FIFO)") {
    fl::priority_queue_stable<ScheduledCall> queue;  // Uses default fl::greater for min-heap

    // Schedule multiple calls at the same timestamp - should execute in FIFO order
    queue.push({1000, 1});
    queue.push({1000, 2});
    queue.push({1000, 3});
    queue.push({1000, 4});

    // Should execute in FIFO order (1, 2, 3, 4)
    REQUIRE_EQ(queue.top().mId, 1);
    queue.pop();
    REQUIRE_EQ(queue.top().mId, 2);
    queue.pop();
    REQUIRE_EQ(queue.top().mId, 3);
    queue.pop();
    REQUIRE_EQ(queue.top().mId, 4);
    queue.pop();

    REQUIRE(queue.empty());
}

TEST_CASE("priority_queue_stable: mixed times") {
    fl::priority_queue_stable<ScheduledCall> queue;  // Uses default fl::greater for min-heap

    // Mix of same and different times
    queue.push({1000, 1});
    queue.push({2000, 2});
    queue.push({1000, 3});  // Same as first
    queue.push({3000, 4});
    queue.push({1000, 5});  // Same as first two

    // Should execute: 1, 3, 5 (all at 1000, FIFO), then 2 (2000), then 4 (3000)
    fl::vector<int> executionOrder;
    while (!queue.empty()) {
        executionOrder.push_back(queue.top().mId);
        queue.pop();
    }

    REQUIRE_EQ(executionOrder.size(), 5u);
    REQUIRE_EQ(executionOrder[0], 1);
    REQUIRE_EQ(executionOrder[1], 3);
    REQUIRE_EQ(executionOrder[2], 5);
    REQUIRE_EQ(executionOrder[3], 2);
    REQUIRE_EQ(executionOrder[4], 4);
}

TEST_CASE("priority_queue_stable: clear") {
    fl::priority_queue_stable<int> queue;

    queue.push(1);
    queue.push(2);
    queue.push(3);
    REQUIRE_EQ(queue.size(), 3u);

    queue.clear();
    REQUIRE(queue.empty());
    REQUIRE_EQ(queue.size(), 0u);

    // Should be able to use after clear
    queue.push(10);
    REQUIRE_EQ(queue.size(), 1u);
    REQUIRE_EQ(queue.top(), 10);
}
