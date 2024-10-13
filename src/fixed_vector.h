#pragma once

#include <stdint.h>
#include <stddef.h>

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

template<typename T, size_t N>
class FixedVector {
private:
    T data[N];
    size_t current_size = 0;

public:
    typedef T* iterator;
    typedef const T* const_iterator;
    // Constructor
    constexpr FixedVector() = default;

    // Array subscript operator
    T& operator[](size_t index) {
        return data[index];
    }

    // Const array subscript operator
    const T& operator[](size_t index) const {
        return data[index];
    }

    // Get the current size of the vector
    constexpr size_t size() const {
        return current_size;
    }

    constexpr bool empty() const {
        return current_size == 0;
    }

    // Get the capacity of the vector
    constexpr size_t capacity() const {
        return N;
    }

    // Add an element to the end of the vector
    void push_back(const T& value) {
        if (current_size < N) {
            data[current_size++] = value;
        }
    }

    // Remove the last element from the vector
    void pop_back() {
        if (current_size > 0) {
            --current_size;
        }
    }

    // Clear the vector
    void clear() {
        for (size_t i = 0; i < current_size; ++i) {
            data[i].~T();
            data[i] = T();
        }
        current_size = 0;
    }

    // Erase the element at the given iterator position
    iterator erase(const T& it) {
        T* ptr = find(it);
        if (ptr != end()) {
            // shift all elements to the left
            for (T* p = ptr; p != end() - 1; ++p) {
                *p = *(p + 1);
            }
            --current_size;
        }
        return ptr;
    }

    iterator find(const T& it) {
        for (size_t i = 0; i < current_size; ++i) {
            if (data[i] == it) {
                return data + i;
            }
        }
        return end();
    }

    const_iterator find(const T& it) const {
        for (size_t i = 0; i < current_size; ++i) {
            if (data[i] == it) {
                return data + i;
            }
        }
        return end();
    }

    bool has(const T& it) const {
        return find(it) != end();
    }


    // Iterator support
    iterator begin() { return data; }
    const_iterator begin() const { return data; }
    iterator end() { return data + current_size; }
    const_iterator end() const { return data + current_size; }
};

FASTLED_NAMESPACE_END
