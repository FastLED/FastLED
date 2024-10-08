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
    T* erase(const T* it) {
        if (it >= begin() && it < end()) {
            size_t index = it - begin();
            // TODO: Is std::move safe here?
            // std::move(it + 1, end(), const_cast<T*>(it));
            // no move
            for (size_t i = index + 1; i < current_size; ++i) {
                data[i - 1] = data[i];
            }
            --current_size;
            return const_cast<T*>(it);
        }
        return nullptr;
    }

    // Iterator support
    T* begin() { return data; }
    const T* begin() const { return data; }
    T* end() { return data + current_size; }
    const T* end() const { return data + current_size; }
};

FASTLED_NAMESPACE_END
