#pragma once

#include "fl/stl/cstring.h"  // IWYU pragma: keep
#include "fl/stl/type_traits.h"
#include "fl/stl/bit_cast.h"  // IWYU pragma: keep
#include "fl/stl/new.h"   // for placement new operator  // IWYU pragma: keep
#include "fl/stl/allocator.h"  // IWYU pragma: keep

#include "fl/stl/initializer_list.h"  // IWYU pragma: keep
#include "fl/stl/alloca.h"  // IWYU pragma: keep
#include "fl/stl/noexcept.h"

namespace fl {

// Forward declaration for span (defined in fl/stl/span.h)
template <typename T, fl::size Extent> class span;  // IWYU pragma: keep

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
    T &at(fl::size pos) FL_NOEXCEPT {
        if (pos >= N) {
            return error_value();
        }
        return mData[pos];
    }

    const T &at(fl::size pos) const FL_NOEXCEPT {
        if (pos >= N) {
            return error_value();
        }
        return mData[pos];
    }

    T &operator[](fl::size pos) FL_NOEXCEPT { return mData[pos]; }

    const_reference operator[](fl::size pos) const FL_NOEXCEPT { return mData[pos]; }

    T &front() FL_NOEXCEPT { return mData[0]; }

    const T &front() const FL_NOEXCEPT { return mData[0]; }

    T &back() FL_NOEXCEPT { return mData[N - 1]; }

    const T &back() const FL_NOEXCEPT { return mData[N - 1]; }

    pointer data() FL_NOEXCEPT { return mData; }

    const_pointer data() const FL_NOEXCEPT { return mData; }

    // Iterators
    iterator begin() FL_NOEXCEPT { return mData; }

    const_iterator begin() const FL_NOEXCEPT { return mData; }

    const_iterator cbegin() const FL_NOEXCEPT { return mData; }

    iterator end() FL_NOEXCEPT { return mData + N; }

    const_iterator end() const FL_NOEXCEPT { return mData + N; }

    const_iterator cend() const FL_NOEXCEPT { return mData + N; }

    // Capacity
    bool empty() const FL_NOEXCEPT { return N == 0; }

    fl::size size() const FL_NOEXCEPT { return N; }

    fl::size max_size() const FL_NOEXCEPT { return N; }

    // Operations
    void fill(const T &value) FL_NOEXCEPT {
        for (fl::size i = 0; i < N; ++i) {
            mData[i] = value;
        }
    }

    void swap(array &other) FL_NOEXCEPT {
        for (fl::size i = 0; i < N; ++i) {
            fl::swap(mData[i], other.mData[i]);
        }
    }

  private:
    static T &error_value() FL_NOEXCEPT {
        static T empty_value;
        return empty_value;
    }
};

// Specialization for zero-size array to avoid T mData[0] UB.
template <typename T> class array<T, 0> {
  public:
    using value_type = T;
    using size_type = fl::size;
    using difference_type = ptrdiff_t;
    using reference = value_type &;
    using const_reference = const value_type &;
    using pointer = value_type *;
    using const_pointer = const value_type *;
    using iterator = pointer;
    using const_iterator = const_pointer;

    // Element access — all return error_value since there are no elements
    T &at(fl::size) FL_NOEXCEPT { return error_value(); }
    const T &at(fl::size) const FL_NOEXCEPT { return error_value(); }
    T &operator[](fl::size) FL_NOEXCEPT { return error_value(); }
    const_reference operator[](fl::size) const FL_NOEXCEPT { return error_value(); }
    T &front() FL_NOEXCEPT { return error_value(); }
    const T &front() const FL_NOEXCEPT { return error_value(); }
    T &back() FL_NOEXCEPT { return error_value(); }
    const T &back() const FL_NOEXCEPT { return error_value(); }
    pointer data() FL_NOEXCEPT { return nullptr; }
    const_pointer data() const FL_NOEXCEPT { return nullptr; }

    // Iterators — begin == end for empty container
    iterator begin() FL_NOEXCEPT { return nullptr; }
    const_iterator begin() const FL_NOEXCEPT { return nullptr; }
    const_iterator cbegin() const FL_NOEXCEPT { return nullptr; }
    iterator end() FL_NOEXCEPT { return nullptr; }
    const_iterator end() const FL_NOEXCEPT { return nullptr; }
    const_iterator cend() const FL_NOEXCEPT { return nullptr; }

    // Capacity
    bool empty() const FL_NOEXCEPT { return true; }
    fl::size size() const FL_NOEXCEPT { return 0; }
    fl::size max_size() const FL_NOEXCEPT { return 0; }

    // Operations
    void fill(const T &) FL_NOEXCEPT {}
    void swap(array &) FL_NOEXCEPT {}

  private:
    static T &error_value() FL_NOEXCEPT {
        static T empty_value;
        return empty_value;
    }
};

// Non-member functions
template <typename T, fl::size N>
bool operator==(const array<T, N> &lhs, const array<T, N> &rhs) FL_NOEXCEPT {
    // return std::equal(lhs.begin(), lhs.end(), rhs.begin());
    for (fl::size i = 0; i < N; ++i) {
        if (lhs[i] != rhs[i]) {
            return false;
        }
    }
    return true;
}

template <typename T, fl::size N>
bool operator!=(const array<T, N> &lhs, const array<T, N> &rhs) FL_NOEXCEPT {
    return !(lhs == rhs);
}

template <typename T, fl::size N>
bool operator<(const array<T, N> &lhs, const array<T, N> &rhs) FL_NOEXCEPT {
    for (fl::size i = 0; i < N; ++i) {
        if (lhs[i] < rhs[i]) {
            return true;
        }
        if (rhs[i] < lhs[i]) {
            return false;
        }
    }
    return false;
}

template <typename T, fl::size N>
bool operator<=(const array<T, N> &lhs, const array<T, N> &rhs) FL_NOEXCEPT {
    return !(rhs < lhs);
}

template <typename T, fl::size N>
bool operator>(const array<T, N> &lhs, const array<T, N> &rhs) FL_NOEXCEPT {
    return rhs < lhs;
}

template <typename T, fl::size N>
bool operator>=(const array<T, N> &lhs, const array<T, N> &rhs) FL_NOEXCEPT {
    return !(lhs < rhs);
}

template <typename T, fl::size N>
void swap(array<T, N> &lhs,
          array<T, N> &rhs) FL_NOEXCEPT {
    lhs.swap(rhs);
}

// Helper function to create array from span (dynamic extent)
// Copies exactly N elements from span (span must have at least N elements)
template <fl::size N, typename T>
array<T, N> to_array(fl::span<const T, fl::size(-1)> s) FL_NOEXCEPT {
    array<T, N> result;
    for (fl::size i = 0; i < N; ++i) {
        result.mData[i] = s[i];
    }
    return result;
}

// Helper function to create array from span (static extent)
template <typename T, fl::size N>
array<T, N> to_array(fl::span<const T, N> s) FL_NOEXCEPT {
    array<T, N> result;
    for (fl::size i = 0; i < N; ++i) {
        result.mData[i] = s[i];
    }
    return result;
}

} // namespace fl
