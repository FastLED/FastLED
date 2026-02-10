#include "fl/stl/priority_queue.h"
#include "fl/stl/vector.h"
#include "fl/stl/stdint.h"
#include "fl/stl/new.h"
#include "test.h"
#include "fl/stl/allocator.h"
#include "fl/stl/utility.h"
#include "fl/stl/cstddef.h"

using namespace fl;

// ============================================================================
// Tests for fl::PriorityQueue (standard max-heap priority queue)
// ============================================================================

FL_TEST_CASE("fl::PriorityQueue basic operations") {
    FL_SUBCASE("default constructor") {
        PriorityQueue<int> pq;
        FL_CHECK(pq.empty());
        FL_CHECK_EQ(pq.size(), 0u);
    }

    FL_SUBCASE("push and top") {
        PriorityQueue<int> pq;
        pq.push(5);
        FL_CHECK(!pq.empty());
        FL_CHECK_EQ(pq.size(), 1u);
        FL_CHECK_EQ(pq.top(), 5);

        pq.push(3);
        FL_CHECK_EQ(pq.size(), 2u);
        FL_CHECK_EQ(pq.top(), 5);  // Max heap by default

        pq.push(7);
        FL_CHECK_EQ(pq.size(), 3u);
        FL_CHECK_EQ(pq.top(), 7);
    }

    FL_SUBCASE("pop operations") {
        PriorityQueue<int> pq;
        pq.push(5);
        pq.push(3);
        pq.push(7);
        pq.push(1);
        pq.push(9);

        FL_CHECK_EQ(pq.top(), 9);
        pq.pop();
        FL_CHECK_EQ(pq.top(), 7);
        pq.pop();
        FL_CHECK_EQ(pq.top(), 5);
        pq.pop();
        FL_CHECK_EQ(pq.top(), 3);
        pq.pop();
        FL_CHECK_EQ(pq.top(), 1);
        pq.pop();
        FL_CHECK(pq.empty());
    }
}

FL_TEST_CASE("fl::PriorityQueue with custom comparator") {
    FL_SUBCASE("min heap with custom comparator") {
        // Use custom comparator for min heap (opposite of default)
        struct Greater {
            bool operator()(int a, int b) const { return a > b; }
        };
        PriorityQueue<int, Greater> pq;
        pq.push(5);
        pq.push(3);
        pq.push(7);
        pq.push(1);
        pq.push(9);

        // Should return smallest element first
        FL_CHECK_EQ(pq.top(), 1);
        pq.pop();
        FL_CHECK_EQ(pq.top(), 3);
        pq.pop();
        FL_CHECK_EQ(pq.top(), 5);
        pq.pop();
        FL_CHECK_EQ(pq.top(), 7);
        pq.pop();
        FL_CHECK_EQ(pq.top(), 9);
    }

    FL_SUBCASE("custom struct with comparator") {
        struct Task {
            int priority;
            int id;

            bool operator<(const Task& other) const {
                return priority < other.priority;
            }

            bool operator==(const Task& other) const {
                return priority == other.priority && id == other.id;
            }
        };

        PriorityQueue<Task> pq;
        pq.push(Task{5, 1});
        pq.push(Task{3, 2});
        pq.push(Task{7, 3});
        pq.push(Task{1, 4});

        FL_CHECK_EQ(pq.top().priority, 7);
        FL_CHECK_EQ(pq.top().id, 3);
        pq.pop();

        FL_CHECK_EQ(pq.top().priority, 5);
        FL_CHECK_EQ(pq.top().id, 1);
    }
}

FL_TEST_CASE("fl::PriorityQueue edge cases") {
    FL_SUBCASE("single element") {
        PriorityQueue<int> pq;
        pq.push(42);
        FL_CHECK_EQ(pq.size(), 1u);
        FL_CHECK_EQ(pq.top(), 42);
        pq.pop();
        FL_CHECK(pq.empty());
    }

    FL_SUBCASE("duplicate elements") {
        PriorityQueue<int> pq;
        pq.push(5);
        pq.push(5);
        pq.push(5);
        pq.push(3);
        pq.push(7);

        FL_CHECK_EQ(pq.top(), 7);
        pq.pop();
        FL_CHECK_EQ(pq.top(), 5);
        pq.pop();
        FL_CHECK_EQ(pq.top(), 5);
        pq.pop();
        FL_CHECK_EQ(pq.top(), 5);
        pq.pop();
        FL_CHECK_EQ(pq.top(), 3);
    }

    FL_SUBCASE("negative numbers") {
        PriorityQueue<int> pq;
        pq.push(-5);
        pq.push(-3);
        pq.push(-7);
        pq.push(0);
        pq.push(-1);

        FL_CHECK_EQ(pq.top(), 0);
        pq.pop();
        FL_CHECK_EQ(pq.top(), -1);
        pq.pop();
        FL_CHECK_EQ(pq.top(), -3);
        pq.pop();
        FL_CHECK_EQ(pq.top(), -5);
        pq.pop();
        FL_CHECK_EQ(pq.top(), -7);
    }
}

FL_TEST_CASE("fl::PriorityQueue with different types") {
    FL_SUBCASE("double values") {
        PriorityQueue<double> pq;
        pq.push(3.14);
        pq.push(2.71);
        pq.push(1.41);
        pq.push(4.20);

        FL_CHECK(pq.top() == doctest::Approx(4.20));
        pq.pop();
        FL_CHECK(pq.top() == doctest::Approx(3.14));
        pq.pop();
        FL_CHECK(pq.top() == doctest::Approx(2.71));
        pq.pop();
        FL_CHECK(pq.top() == doctest::Approx(1.41));
    }

    FL_SUBCASE("char values") {
        PriorityQueue<char> pq;
        pq.push('d');
        pq.push('a');
        pq.push('z');
        pq.push('m');

        FL_CHECK_EQ(pq.top(), 'z');
        pq.pop();
        FL_CHECK_EQ(pq.top(), 'm');
        pq.pop();
        FL_CHECK_EQ(pq.top(), 'd');
        pq.pop();
        FL_CHECK_EQ(pq.top(), 'a');
    }
}

FL_TEST_CASE("fl::PriorityQueue stress test") {
    FL_SUBCASE("many elements") {
        PriorityQueue<int> pq;

        // Push 100 elements in random order
        for (int i = 0; i < 100; ++i) {
            pq.push(i);
        }

        FL_CHECK_EQ(pq.size(), 100u);

        // Pop all elements, should come out in descending order
        int prev = 100;
        while (!pq.empty()) {
            int curr = pq.top();
            FL_CHECK(curr < prev);
            prev = curr;
            pq.pop();
        }

        FL_CHECK(pq.empty());
    }

    FL_SUBCASE("alternating push and pop") {
        PriorityQueue<int> pq;

        for (int i = 0; i < 10; ++i) {
            pq.push(i);
            pq.push(i + 10);
            if (i % 2 == 0) {
                pq.pop();
            }
        }

        // Should have some elements left
        FL_CHECK(!pq.empty());

        // Verify they come out in descending order
        int prev = 1000;
        while (!pq.empty()) {
            int curr = pq.top();
            FL_CHECK(curr <= prev);
            prev = curr;
            pq.pop();
        }
    }
}

FL_TEST_CASE("fl::push_heap and pop_heap functions") {
    FL_SUBCASE("push_heap basic") {
        vector<int> v;
        v.push_back(5);
        push_heap(v.begin(), v.end());
        FL_CHECK_EQ(v.front(), 5);

        v.push_back(3);
        push_heap(v.begin(), v.end());
        FL_CHECK_EQ(v.front(), 5);

        v.push_back(7);
        push_heap(v.begin(), v.end());
        FL_CHECK_EQ(v.front(), 7);
    }

    FL_SUBCASE("pop_heap basic") {
        vector<int> v;
        v.push_back(5);
        v.push_back(3);
        v.push_back(7);
        v.push_back(1);

        // Build heap first
        for (size_t i = 1; i < v.size(); ++i) {
            push_heap(v.begin(), v.begin() + i + 1);
        }

        FL_CHECK_EQ(v.front(), 7);

        pop_heap(v.begin(), v.end());
        FL_CHECK_EQ(v.back(), 7);
        v.pop_back();

        FL_CHECK_EQ(v.front(), 5);

        pop_heap(v.begin(), v.end());
        FL_CHECK_EQ(v.back(), 5);
        v.pop_back();

        FL_CHECK_EQ(v.front(), 3);
    }

    FL_SUBCASE("push_heap with custom comparator") {
        vector<int> v;
        auto comp = [](int a, int b) { return a > b; };  // Min heap

        v.push_back(5);
        push_heap(v.begin(), v.end(), comp);

        v.push_back(3);
        push_heap(v.begin(), v.end(), comp);

        v.push_back(7);
        push_heap(v.begin(), v.end(), comp);

        FL_CHECK_EQ(v.front(), 3);  // Smallest element at front for min heap
    }
}

FL_TEST_CASE("fl::sift_down function") {
    FL_SUBCASE("basic sift_down") {
        vector<int> v = {1, 7, 5, 3, 2};

        // Sift down the first element
        sift_down(v.begin(), v.end(), v.begin(),
                  [](int a, int b) { return a < b; });

        // After sift down, heap property should be maintained
        // Root should be larger than its children
        FL_CHECK(v[0] >= v[1]);
        FL_CHECK(v[0] >= v[2]);
    }
}

// ============================================================================
// Tests for fl::priority_queue_stable (stable min-heap with FIFO for equal priorities)
// ============================================================================

FL_TEST_CASE("priority_queue_stable: basic operations") {
    fl::priority_queue_stable<int> queue;

    FL_REQUIRE(queue.empty());
    FL_REQUIRE_EQ(queue.size(), 0u);

    queue.push(5);
    FL_REQUIRE_FALSE(queue.empty());
    FL_REQUIRE_EQ(queue.size(), 1u);
    FL_REQUIRE_EQ(queue.top(), 5);

    queue.pop();
    FL_REQUIRE(queue.empty());
}

FL_TEST_CASE("priority_queue_stable: ordering") {
    fl::priority_queue_stable<int> queue;

    // Push in random order
    queue.push(3);
    queue.push(1);
    queue.push(4);
    queue.push(2);

    // Should pop in descending order (max-heap with fl::less default)
    FL_REQUIRE_EQ(queue.top(), 4);
    queue.pop();
    FL_REQUIRE_EQ(queue.top(), 3);
    queue.pop();
    FL_REQUIRE_EQ(queue.top(), 2);
    queue.pop();
    FL_REQUIRE_EQ(queue.top(), 1);
    queue.pop();
    FL_REQUIRE(queue.empty());
}

FL_TEST_CASE("priority_queue_stable: FIFO for equal priorities") {
    fl::priority_queue_stable<int> queue;

    // Push elements with same priority - should maintain FIFO order
    queue.push(5);
    queue.push(5);
    queue.push(5);

    // All have same priority
    FL_REQUIRE_EQ(queue.size(), 3u);

    for (int i = 0; i < 3; i++) {
        FL_REQUIRE_EQ(queue.top(), 5);
        queue.pop();
    }
    FL_REQUIRE(queue.empty());
}

struct ScheduledCall {
    uint32_t mExecuteAt;
    int mId;  // Used to track FIFO order

    // Comparison operator for min-heap (earlier times have higher priority)
    // priority_queue_stable uses fl::less by default (max-heap), so invert comparison
    // Inverted: earlier (smaller) times should be "greater" for higher priority
    bool operator<(const ScheduledCall& other) const {
        return mExecuteAt > other.mExecuteAt;  // Inverted: smaller time = higher priority
    }
};

FL_TEST_CASE("priority_queue_stable: scheduled calls with different times") {
    fl::priority_queue_stable<ScheduledCall> queue;  // Uses default fl::less for max-heap

    // Schedule calls at different times
    queue.push({1000, 1});
    queue.push({3000, 2});
    queue.push({2000, 3});

    // Should execute in time order (earlier first)
    FL_REQUIRE_EQ(queue.top().mExecuteAt, 1000u);
    FL_REQUIRE_EQ(queue.top().mId, 1);
    queue.pop();

    FL_REQUIRE_EQ(queue.top().mExecuteAt, 2000u);
    FL_REQUIRE_EQ(queue.top().mId, 3);
    queue.pop();

    FL_REQUIRE_EQ(queue.top().mExecuteAt, 3000u);
    FL_REQUIRE_EQ(queue.top().mId, 2);
    queue.pop();

    FL_REQUIRE(queue.empty());
}

FL_TEST_CASE("priority_queue_stable: scheduled calls with same time (FIFO)") {
    fl::priority_queue_stable<ScheduledCall> queue;  // Uses default fl::less for max-heap

    // Schedule multiple calls at the same timestamp - should execute in FIFO order
    queue.push({1000, 1});
    queue.push({1000, 2});
    queue.push({1000, 3});
    queue.push({1000, 4});

    // Should execute in FIFO order (1, 2, 3, 4)
    FL_REQUIRE_EQ(queue.top().mId, 1);
    queue.pop();
    FL_REQUIRE_EQ(queue.top().mId, 2);
    queue.pop();
    FL_REQUIRE_EQ(queue.top().mId, 3);
    queue.pop();
    FL_REQUIRE_EQ(queue.top().mId, 4);
    queue.pop();

    FL_REQUIRE(queue.empty());
}

FL_TEST_CASE("priority_queue_stable: mixed times") {
    fl::priority_queue_stable<ScheduledCall> queue;  // Uses default fl::less for max-heap

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

    FL_REQUIRE_EQ(executionOrder.size(), 5u);
    FL_REQUIRE_EQ(executionOrder[0], 1);
    FL_REQUIRE_EQ(executionOrder[1], 3);
    FL_REQUIRE_EQ(executionOrder[2], 5);
    FL_REQUIRE_EQ(executionOrder[3], 2);
    FL_REQUIRE_EQ(executionOrder[4], 4);
}

FL_TEST_CASE("priority_queue_stable: clear") {
    fl::priority_queue_stable<int> queue;

    queue.push(1);
    queue.push(2);
    queue.push(3);
    FL_REQUIRE_EQ(queue.size(), 3u);

    queue.clear();
    FL_REQUIRE(queue.empty());
    FL_REQUIRE_EQ(queue.size(), 0u);

    // Should be able to use after clear
    queue.push(10);
    FL_REQUIRE_EQ(queue.size(), 1u);
    FL_REQUIRE_EQ(queue.top(), 10);
}
