#pragma once

#include "fl/stl/cstring.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/bit_cast.h"
#include "fl/stl/new.h"   // for placement new operator
#include "fl/stl/allocator.h"

#include "fl/stl/initializer_list.h"
#include "fl/alloca.h"

namespace fl {

// Forward declaration for span (defined in fl/slice.h)
template <typename T, fl::size Extent> class span;

/**
 * @brief A fixed-size array implementation similar to std::array
 *
 * This class provides a thin wrapper around a C-style array with
 * STL container-like interface.
 *
 * @tparam T The type of elements
 * @tparam N The number of elements
 */
template <typename T, fl::size N> class array {
  public:
    // Standard container type definitions
    using value_type = T;
    using size_type = fl::size;
    using difference_type = ptrdiff_t;
    using reference = value_type &;
    using const_reference = const value_type &;
    using pointer = value_type *;
    using const_pointer = const value_type *;
    using iterator = pointer;
    using const_iterator = const_pointer;

    // Data member - public to allow aggregate initialization
    T mData[N];

    // Element access
    T &at(fl::size pos) {
        if (pos >= N) {
            return error_value();
        }
        return mData[pos];
    }

    const T &at(fl::size pos) const {
        if (pos >= N) {
            return error_value();
        }
        return mData[pos];
    }

    T &operator[](fl::size pos) { return mData[pos]; }

    const_reference operator[](fl::size pos) const { return mData[pos]; }

    T &front() { return mData[0]; }

    const T &front() const { return mData[0]; }

    T &back() { return mData[N - 1]; }

    const T &back() const { return mData[N - 1]; }

    pointer data() noexcept { return mData; }

    const_pointer data() const noexcept { return mData; }

    // Iterators
    iterator begin() noexcept { return mData; }

    const_iterator begin() const noexcept { return mData; }

    const_iterator cbegin() const noexcept { return mData; }

    iterator end() noexcept { return mData + N; }

    const_iterator end() const noexcept { return mData + N; }

    const_iterator cend() const noexcept { return mData + N; }

    // Capacity
    bool empty() const noexcept { return N == 0; }

    fl::size size() const noexcept { return N; }

    fl::size max_size() const noexcept { return N; }

    // Operations
    void fill(const T &value) {
        for (fl::size i = 0; i < N; ++i) {
            mData[i] = value;
        }
    }

    void swap(array &other) {
        for (fl::size i = 0; i < N; ++i) {
            fl::swap(mData[i], other.mData[i]);
        }
    }

  private:
    static T &error_value() {
        static T empty_value;
        return empty_value;
    }
};

// Non-member functions
template <typename T, fl::size N>
bool operator==(const array<T, N> &lhs, const array<T, N> &rhs) {
    // return std::equal(lhs.begin(), lhs.end(), rhs.begin());
    for (fl::size i = 0; i < N; ++i) {
        if (lhs[i] != rhs[i]) {
            return false;
        }
    }
    return true;
}

template <typename T, fl::size N>
bool operator!=(const array<T, N> &lhs, const array<T, N> &rhs) {
    return !(lhs == rhs);
}

template <typename T, fl::size N>
void swap(array<T, N> &lhs,
          array<T, N> &rhs) noexcept(noexcept(lhs.swap(rhs))) {
    lhs.swap(rhs);
}

// Helper function to create array from span (dynamic extent)
// Copies exactly N elements from span (span must have at least N elements)
template <fl::size N, typename T>
array<T, N> to_array(fl::span<const T, fl::size(-1)> s) {
    array<T, N> result;
    for (fl::size i = 0; i < N; ++i) {
        result.mData[i] = s[i];
    }
    return result;
}

// Helper function to create array from span (static extent)
template <typename T, fl::size N>
array<T, N> to_array(fl::span<const T, N> s) {
    array<T, N> result;
    for (fl::size i = 0; i < N; ++i) {
        result.mData[i] = s[i];
    }
    return result;
}

} // namespace fl
