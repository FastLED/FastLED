#pragma once

#include "fl/vector.h"

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

template <typename Iterator>
void push_heap(Iterator first, Iterator last) {
    push_heap(first, last, [](const auto& a, const auto& b) { return a < b; });
}

template <typename Iterator, typename Compare>
void pop_heap(Iterator first, Iterator last, Compare comp) {
    --last;
    auto tmp = *first;
    *first = *last;
    *last = tmp;
    
    sift_down(first, last, first, comp);
}

template <typename Iterator>
void pop_heap(Iterator first, Iterator last) {
    pop_heap(first, last, [](const auto& a, const auto& b) { return a < b; });
}

template <typename T, typename VectorT = fl::HeapVector<T>>
class PriorityQueue {
  public:
    using value_type = T;
    using size_type = size_t;

    PriorityQueue() = default;

    void push(const T &value) {
        _data.push_back(value);
        push_heap(_data.begin(), _data.end());
    }

    void pop() {
        pop_heap(_data.begin(), _data.end());
        _data.pop_back();
    }

    const T &top() const { return _data.front(); }

    bool empty() const { return _data.size() == 0; }
    size_type size() const { return _data.size(); }

  private:
    VectorT _data;
};

}  // namespace fl
