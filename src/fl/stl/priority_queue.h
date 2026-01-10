#pragma once

#include "fl/stl/stdint.h"                 // for uint64_t
#include "fl/stl/vector.h"                 // for vector
#include "fl/stl/move.h"                   // for remove_reference
#include "fl/stl/utility.h"                // for greater, less
#include "fl/int.h"                        // for size

namespace fl {

// Custom heap operations
template <typename Iterator, typename Compare>
void sift_down(Iterator first, Iterator last, Iterator start, Compare comp) {
    auto root = start;
    auto child = first + 2 * (root - first) + 1;

    while (child < last) {
        if (child + 1 < last && comp(*child, *(child + 1))) {
            ++child;
        }

        if (comp(*root, *child)) {
            auto tmp = *root;
            *root = *child;
            *child = tmp;

            root = child;
            child = first + 2 * (root - first) + 1;
        } else {
            break;
        }
    }
}

template <typename Iterator, typename Compare>
void push_heap(Iterator first, Iterator last, Compare comp) {
    auto pos = last - 1;
    auto parent = first + ((pos - first) - 1) / 2;

    while (pos > first && comp(*parent, *pos)) {
        auto tmp = *parent;
        *parent = *pos;
        *pos = tmp;

        pos = parent;
        parent = first + ((pos - first) - 1) / 2;
    }
}

template <typename Iterator> void push_heap(Iterator first, Iterator last) {
    using value_type = typename fl::remove_reference<decltype(*first)>::type;
    push_heap(first, last, [](const value_type &a, const value_type &b) { return a < b; });
}

template <typename Iterator, typename Compare>
void pop_heap(Iterator first, Iterator last, Compare comp) {
    --last;
    auto tmp = *first;
    *first = *last;
    *last = tmp;

    sift_down(first, last, first, comp);
}

template <typename Iterator> void pop_heap(Iterator first, Iterator last) {
    using value_type = typename fl::remove_reference<decltype(*first)>::type;
    pop_heap(first, last, [](const value_type &a, const value_type &b) { return a < b; });
}

template <typename T, typename Compare = fl::less<T>,
          typename VectorT = vector<T>>
class PriorityQueue {
  public:
    using value_type = T;
    using size_type = fl::size;
    using compare_type = Compare;

    PriorityQueue() = default;
    explicit PriorityQueue(const Compare &comp) : _comp(comp) {}

    void push(const T &value) {
        _data.push_back(value);
        push_heap(_data.begin(), _data.end(), _comp);
    }

    void pop() {
        pop_heap(_data.begin(), _data.end(), _comp);
        _data.pop_back();
    }

    const T &top() const { return _data.front(); }

    bool empty() const { return _data.size() == 0; }
    fl::size size() const { return _data.size(); }

    const Compare &compare() const { return _comp; }

  private:
    VectorT _data;
    Compare _comp;
};

/**
 * @brief Stable priority queue that maintains FIFO ordering for equal-priority elements
 *
 * This is a wrapper around fl::PriorityQueue that adds a monotonic sequence counter
 * to ensure stable (FIFO) ordering when elements have equal priority according to
 * the comparison function.
 *
 * Template Parameters:
 * - T: Element type (must be copyable)
 * - Compare: Comparison function (default: fl::greater<T> for min-heap behavior)
 *
 * By default, this creates a MIN-HEAP (smallest element has highest priority).
 * For custom types with special priority ordering, define operator< and use fl::less<T>:
 * @code
 * struct Task {
 *     int priority;
 *     bool operator<(const Task& o) const { return priority > o.priority; }  // Invert for min-heap
 * };
 * fl::priority_queue_stable<Task, fl::less<Task>> queue;  // Use less with inverted operator<
 * @endcode
 *
 * Example (default min-heap for integers):
 * @code
 * fl::priority_queue_stable<int> queue;
 * queue.push(3);
 * queue.push(1);
 * queue.push(3);  // Same priority as first 3
 * // Pop order: 1, 3 (first), 3 (second) - FIFO for equal priorities
 * @endcode
 */
template<typename T, typename Compare = fl::greater<T>>
class priority_queue_stable {
private:
    struct StableElement {
        T mValue;
        uint64_t mSequence;

        // Comparison: primary by value, secondary by sequence (FIFO)
        bool operator<(const StableElement& other) const {
            Compare comp;
            // Use the Compare function to determine ordering
            if (comp(mValue, other.mValue)) return true;
            if (comp(other.mValue, mValue)) return false;
            // Values are equal according to comparator - use sequence for FIFO ordering
            // INVERTED: smaller sequence should be "greater" (pop first)
            return mSequence > other.mSequence;
        }
    };

public:
    using value_type = T;
    using size_type = fl::size;

    priority_queue_stable() : mNextSequence(0) {}

    /**
     * @brief Push an element into the priority queue
     * @param value Element to insert
     */
    void push(const T& value) {
        mQueue.push({value, mNextSequence++});
    }

    /**
     * @brief Remove the top element from the queue
     *
     * Precondition: !empty()
     */
    void pop() {
        mQueue.pop();
    }

    /**
     * @brief Access the top element (highest priority)
     * @return Reference to the top element
     *
     * Precondition: !empty()
     */
    const T& top() const {
        return mQueue.top().mValue;
    }

    /**
     * @brief Check if the queue is empty
     * @return true if empty, false otherwise
     */
    bool empty() const {
        return mQueue.empty();
    }

    /**
     * @brief Get the number of elements in the queue
     * @return Number of elements
     */
    fl::size size() const {
        return mQueue.size();
    }

    /**
     * @brief Clear all elements from the queue
     */
    void clear() {
        while (!empty()) {
            pop();
        }
        mNextSequence = 0;
    }

private:
    fl::PriorityQueue<StableElement> mQueue;
    uint64_t mNextSequence;
};

} // namespace fl
