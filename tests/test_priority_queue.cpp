// g++ --std=c++11 test.cpp

#include "test.h"
#include "fl/priority_queue.h"

using namespace fl;

TEST_CASE("Simple Priority Queue") {
    PriorityQueue<int> pq;
    
    // Test empty queue
    CHECK(pq.empty());
    CHECK(pq.size() == 0);
    
    // Test push and top
    pq.push(5);
    CHECK(!pq.empty());
    CHECK(pq.size() == 1);
    CHECK(pq.top() == 5);
    
    // Test priority ordering (default is max heap)
    pq.push(10);
    CHECK(pq.size() == 2);
    CHECK(pq.top() == 10);
    
    pq.push(3);
    CHECK(pq.size() == 3);
    CHECK(pq.top() == 10);
    
    pq.push(15);
    CHECK(pq.size() == 4);
    CHECK(pq.top() == 15);
    
    // Test pop
    pq.pop();
    CHECK(pq.size() == 3);
    CHECK(pq.top() == 10);
    
    pq.pop();
    CHECK(pq.size() == 2);
    CHECK(pq.top() == 5);
    
    pq.pop();
    CHECK(pq.size() == 1);
    CHECK(pq.top() == 3);
    
    pq.pop();
    CHECK(pq.empty());
    CHECK(pq.size() == 0);
}

TEST_CASE("Priority Queue with Custom Type") {
    struct Task {
        int priority;
        const char* name;
        
        bool operator<(const Task& other) const {
            return priority < other.priority;
        }
    };
    
    PriorityQueue<Task> pq;
    
    pq.push({1, "Low priority task"});
    pq.push({5, "Medium priority task"});
    pq.push({10, "High priority task"});
    
    CHECK(pq.size() == 3);
    CHECK(pq.top().priority == 10);
    CHECK(pq.top().name == "High priority task");
    
    pq.pop();
    CHECK(pq.top().priority == 5);
    CHECK(pq.top().name == "Medium priority task");
}


TEST_CASE("Priority Queue with Custom Comparator") {
    struct MinHeapCompare {
        bool operator()(int a, int b) const {
            return a > b; // For min heap
        }
    };
    
    // Create a min heap using custom heap operations
    HeapVector<int> vec;
    vec.push_back(5);
    vec.push_back(10);
    vec.push_back(3);
    
    // Use our custom heap functions with a custom comparator
    push_heap(vec.begin(), vec.end(), MinHeapCompare());
    CHECK(vec[0] == 3); // Min element should be at the top
    
    vec.push_back(1);
    push_heap(vec.begin(), vec.end(), MinHeapCompare());
    CHECK(vec[0] == 1); // New min element
    
    // Test pop_heap with custom comparator
    pop_heap(vec.begin(), vec.end(), MinHeapCompare());
    CHECK(vec[0] == 3); // Next min element
    vec.pop_back(); // Remove the element we popped to the end
}
