#pragma once

#include <stdint.h>
#include <stddef.h>

#include "inplacenew.h"
#include "namespace.h"

FASTLED_NAMESPACE_BEGIN


// A fixed sized vector. The user is responsible for making sure that the
// inserts do not exceed the capacity of the vector, otherwise they will fail.
// Because of this limitation, this vector is not a drop in replacement for
// std::vector.
template<typename T, size_t N>
class FixedVector {
private:
    union {
        char raw[N * sizeof(T)];
        T data[N];
    };
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
            void* mem = &data[current_size];
            new (mem) T(value);
            ++current_size;
        }
    }

    // Remove the last element from the vector
    void pop_back() {
        if (current_size > 0) {
            --current_size;
            data[current_size].~T();
        }
    }

    // Clear the vector
    void clear() {
        while (current_size > 0) {
            pop_back();
        }
    }

    // Erase the element at the given iterator position
    iterator erase(iterator pos) {
        if (pos != end()) {
            pos->~T();
            // shift all elements to the left
            for (iterator p = pos; p != end() - 1; ++p) {
                new (p) T(*(p + 1)); // Use copy constructor instead of std::move
                (p + 1)->~T();
            }
            --current_size;
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

    template<typename Predicate>
    iterator find_if(Predicate pred) {
        for (iterator it = begin(); it != end(); ++it) {
            if (pred(*it)) {
                return it;
            }
        }
        return end();
    }

    bool insert(iterator pos, const T& value) {
        if (current_size < N) {
            // shift all elements to the right
            for (iterator p = end(); p != pos; --p) {
                new (p) T(*(p - 1)); // Use copy constructor instead of std::move
                (p - 1)->~T();
            }
            new (pos) T(value);
            ++current_size;
            return true;
        }
        return false;
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
    iterator begin() { return &data[0]; }
    const_iterator begin() const { return &data[0]; }
    iterator end() { return &data[current_size]; }
    const_iterator end() const { return &data[current_size]; }
};

FASTLED_NAMESPACE_END
