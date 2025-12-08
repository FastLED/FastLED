#pragma once

#include "ftl/cstring.h"
#include "ftl/type_traits.h"
#include "ftl/bit_cast.h"
#include "ftl/new.h"   // for placement new operator
#include "ftl/allocator.h"

#include "ftl/initializer_list.h"
#include "fl/alloca.h"

namespace fl {

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

} // namespace fl
