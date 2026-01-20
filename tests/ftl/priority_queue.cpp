#include "fl/stl/priority_queue.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/stl/vector.h"
#include "fl/stl/utility.h"

using namespace fl;

TEST_CASE("fl::PriorityQueue basic operations") {
    SUBCASE("default constructor") {
        PriorityQueue<int> pq;
        CHECK(pq.empty());
        CHECK_EQ(pq.size(), 0u);
    }

    SUBCASE("push and top") {
        PriorityQueue<int> pq;
        pq.push(5);
        CHECK(!pq.empty());
        CHECK_EQ(pq.size(), 1u);
        CHECK_EQ(pq.top(), 5);

        pq.push(3);
        CHECK_EQ(pq.size(), 2u);
        CHECK_EQ(pq.top(), 5);  // Max heap by default

        pq.push(7);
        CHECK_EQ(pq.size(), 3u);
        CHECK_EQ(pq.top(), 7);
    }

    SUBCASE("pop operations") {
        PriorityQueue<int> pq;
        pq.push(5);
        pq.push(3);
        pq.push(7);
        pq.push(1);
        pq.push(9);

        CHECK_EQ(pq.top(), 9);
        pq.pop();
        CHECK_EQ(pq.top(), 7);
        pq.pop();
        CHECK_EQ(pq.top(), 5);
        pq.pop();
        CHECK_EQ(pq.top(), 3);
        pq.pop();
        CHECK_EQ(pq.top(), 1);
        pq.pop();
        CHECK(pq.empty());
    }
}

TEST_CASE("fl::PriorityQueue with custom comparator") {
    SUBCASE("min heap with custom comparator") {
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
        CHECK_EQ(pq.top(), 1);
        pq.pop();
        CHECK_EQ(pq.top(), 3);
        pq.pop();
        CHECK_EQ(pq.top(), 5);
        pq.pop();
        CHECK_EQ(pq.top(), 7);
        pq.pop();
        CHECK_EQ(pq.top(), 9);
    }

    SUBCASE("custom struct with comparator") {
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

        CHECK_EQ(pq.top().priority, 7);
        CHECK_EQ(pq.top().id, 3);
        pq.pop();

        CHECK_EQ(pq.top().priority, 5);
        CHECK_EQ(pq.top().id, 1);
    }
}

TEST_CASE("fl::PriorityQueue edge cases") {
    SUBCASE("single element") {
        PriorityQueue<int> pq;
        pq.push(42);
        CHECK_EQ(pq.size(), 1u);
        CHECK_EQ(pq.top(), 42);
        pq.pop();
        CHECK(pq.empty());
    }

    SUBCASE("duplicate elements") {
        PriorityQueue<int> pq;
        pq.push(5);
        pq.push(5);
        pq.push(5);
        pq.push(3);
        pq.push(7);

        CHECK_EQ(pq.top(), 7);
        pq.pop();
        CHECK_EQ(pq.top(), 5);
        pq.pop();
        CHECK_EQ(pq.top(), 5);
        pq.pop();
        CHECK_EQ(pq.top(), 5);
        pq.pop();
        CHECK_EQ(pq.top(), 3);
    }

    SUBCASE("negative numbers") {
        PriorityQueue<int> pq;
        pq.push(-5);
        pq.push(-3);
        pq.push(-7);
        pq.push(0);
        pq.push(-1);

        CHECK_EQ(pq.top(), 0);
        pq.pop();
        CHECK_EQ(pq.top(), -1);
        pq.pop();
        CHECK_EQ(pq.top(), -3);
        pq.pop();
        CHECK_EQ(pq.top(), -5);
        pq.pop();
        CHECK_EQ(pq.top(), -7);
    }
}

TEST_CASE("fl::PriorityQueue with different types") {
    SUBCASE("double values") {
        PriorityQueue<double> pq;
        pq.push(3.14);
        pq.push(2.71);
        pq.push(1.41);
        pq.push(4.20);

        CHECK(pq.top() == doctest::Approx(4.20));
        pq.pop();
        CHECK(pq.top() == doctest::Approx(3.14));
        pq.pop();
        CHECK(pq.top() == doctest::Approx(2.71));
        pq.pop();
        CHECK(pq.top() == doctest::Approx(1.41));
    }

    SUBCASE("char values") {
        PriorityQueue<char> pq;
        pq.push('d');
        pq.push('a');
        pq.push('z');
        pq.push('m');

        CHECK_EQ(pq.top(), 'z');
        pq.pop();
        CHECK_EQ(pq.top(), 'm');
        pq.pop();
        CHECK_EQ(pq.top(), 'd');
        pq.pop();
        CHECK_EQ(pq.top(), 'a');
    }
}

TEST_CASE("fl::PriorityQueue stress test") {
    SUBCASE("many elements") {
        PriorityQueue<int> pq;

        // Push 100 elements in random order
        for (int i = 0; i < 100; ++i) {
            pq.push(i);
        }

        CHECK_EQ(pq.size(), 100u);

        // Pop all elements, should come out in descending order
        int prev = 100;
        while (!pq.empty()) {
            int curr = pq.top();
            CHECK(curr < prev);
            prev = curr;
            pq.pop();
        }

        CHECK(pq.empty());
    }

    SUBCASE("alternating push and pop") {
        PriorityQueue<int> pq;

        for (int i = 0; i < 10; ++i) {
            pq.push(i);
            pq.push(i + 10);
            if (i % 2 == 0) {
                pq.pop();
            }
        }

        // Should have some elements left
        CHECK(!pq.empty());

        // Verify they come out in descending order
        int prev = 1000;
        while (!pq.empty()) {
            int curr = pq.top();
            CHECK(curr <= prev);
            prev = curr;
            pq.pop();
        }
    }
}

TEST_CASE("fl::push_heap and pop_heap functions") {
    SUBCASE("push_heap basic") {
        vector<int> v;
        v.push_back(5);
        push_heap(v.begin(), v.end());
        CHECK_EQ(v.front(), 5);

        v.push_back(3);
        push_heap(v.begin(), v.end());
        CHECK_EQ(v.front(), 5);

        v.push_back(7);
        push_heap(v.begin(), v.end());
        CHECK_EQ(v.front(), 7);
    }

    SUBCASE("pop_heap basic") {
        vector<int> v;
        v.push_back(5);
        v.push_back(3);
        v.push_back(7);
        v.push_back(1);

        // Build heap first
        for (size_t i = 1; i < v.size(); ++i) {
            push_heap(v.begin(), v.begin() + i + 1);
        }

        CHECK_EQ(v.front(), 7);

        pop_heap(v.begin(), v.end());
        CHECK_EQ(v.back(), 7);
        v.pop_back();

        CHECK_EQ(v.front(), 5);

        pop_heap(v.begin(), v.end());
        CHECK_EQ(v.back(), 5);
        v.pop_back();

        CHECK_EQ(v.front(), 3);
    }

    SUBCASE("push_heap with custom comparator") {
        vector<int> v;
        auto comp = [](int a, int b) { return a > b; };  // Min heap

        v.push_back(5);
        push_heap(v.begin(), v.end(), comp);

        v.push_back(3);
        push_heap(v.begin(), v.end(), comp);

        v.push_back(7);
        push_heap(v.begin(), v.end(), comp);

        CHECK_EQ(v.front(), 3);  // Smallest element at front for min heap
    }
}

TEST_CASE("fl::sift_down function") {
    SUBCASE("basic sift_down") {
        vector<int> v = {1, 7, 5, 3, 2};

        // Sift down the first element
        sift_down(v.begin(), v.end(), v.begin(),
                  [](int a, int b) { return a < b; });

        // After sift down, heap property should be maintained
        // Root should be larger than its children
        CHECK(v[0] >= v[1]);
        CHECK(v[0] >= v[2]);
    }
}
