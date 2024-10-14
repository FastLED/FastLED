#pragma once

#include <stdint.h>
#include <stddef.h>

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

template<typename T, size_t N>
class FixedVector {
private:
    T data[N] = {};
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
            new (&data[i]) T();
        }
        current_size = 0;
    }

    // Erase the element at the given iterator position
    iterator erase(iterator pos) {
        if (pos != end()) {
            // Destroy the element at the position
            pos->~T();
            // shift all elements to the left
            for (iterator p = pos; p != end() - 1; ++p) {
                *p = *(p + 1);
            }
            --current_size;
            // Ensure the last element is properly destructed and default-constructed
            (end())->~T();
            new (end()) T();
        }
        return pos;
    }

    iterator erase(const T& value) {
        iterator it = find(value);
        if (it != end()) {
            erase(it);
        }
        return it;
    }

    iterator find(const T& value) {
        for (iterator it = begin(); it != end(); ++it) {
            if (*it == value) {
                return it;
            }
        }
        return end();
    }

    const_iterator find(const T& value) const {
        for (const_iterator it = begin(); it != end(); ++it) {
            if (*it == value) {
                return it;
            }
        }
        return end();
    }

    bool has(const T& value) const {
        return find(value) != end();
    }

    // Access to first and last elements
    T& front() {
        return data[0];
    }

    const T& front() const {
        return data[0];
    }

    T& back() {
        return data[current_size - 1];
    }

    const T& back() const {
        return data[current_size - 1];
    }

    // Iterator support
    iterator begin() { return data; }
    const_iterator begin() const { return data; }
    iterator end() { return data + current_size; }
    const_iterator end() const { return data + current_size; }
};

FASTLED_NAMESPACE_END
