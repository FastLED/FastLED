#pragma once

#include <stdint.h>
#include <stddef.h>

#include <utility>
#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

template<typename T, size_t N>
class FixedVector {
private:
    alignas(T) char data[N * sizeof(T)];
    size_t current_size = 0;

public:
    typedef T* iterator;
    typedef const T* const_iterator;
    // Constructor
    constexpr FixedVector() : current_size(0) {}

    // Destructor
    ~FixedVector() {
        clear();
    }

    // Array subscript operator
    T& operator[](size_t index) {
        return *reinterpret_cast<T*>(&data[index * sizeof(T)]);
    }

    // Const array subscript operator
    const T& operator[](size_t index) const {
        return *reinterpret_cast<const T*>(&data[index * sizeof(T)]);
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
            new (&data[current_size * sizeof(T)]) T(value);
            ++current_size;
        }
    }

    // Remove the last element from the vector
    void pop_back() {
        if (current_size > 0) {
            // destroy object
            reinterpret_cast<T*>(&data[(current_size - 1) * sizeof(T)])->~T();
            --current_size;
        }
    }

    // Clear the vector
    void clear() {
        for (size_t i = 0; i < current_size; ++i) {
            reinterpret_cast<T*>(&data[i * sizeof(T)])->~T();
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
        return *reinterpret_cast<T*>(&data[0]);
    }

    const T& front() const {
        return *reinterpret_cast<const T*>(&data[0]);
    }

    T& back() {
        return *reinterpret_cast<T*>(&data[(current_size - 1) * sizeof(T)]);
    }

    const T& back() const {
        return *reinterpret_cast<const T*>(&data[(current_size - 1) * sizeof(T)]);
    }

    // Iterator support
    iterator begin() { return reinterpret_cast<T*>(&data[0]); }
    const_iterator begin() const { return reinterpret_cast<const T*>(&data[0]); }
    iterator end() { return reinterpret_cast<T*>(&data[current_size * sizeof(T)]); }
    const_iterator end() const { return reinterpret_cast<const T*>(&data[current_size * sizeof(T)]); }
};

FASTLED_NAMESPACE_END
