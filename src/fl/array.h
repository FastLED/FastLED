#pragma once

#include "fl/cstring.h"
#include "fl/type_traits.h"
#include "fl/bit_cast.h"
#include "fl/new.h"   // for placement new operator
#include "fl/allocator.h"

#include "fl/initializer_list.h"
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
    T data_[N];

    // Element access
    T &at(fl::size pos) {
        if (pos >= N) {
            return error_value();
        }
        return data_[pos];
    }

    const T &at(fl::size pos) const {
        if (pos >= N) {
            return error_value();
        }
        return data_[pos];
    }

    T &operator[](fl::size pos) { return data_[pos]; }

    const_reference operator[](fl::size pos) const { return data_[pos]; }

    T &front() { return data_[0]; }

    const T &front() const { return data_[0]; }

    T &back() { return data_[N - 1]; }

    const T &back() const { return data_[N - 1]; }

    pointer data() noexcept { return data_; }

    const_pointer data() const noexcept { return data_; }

    // Iterators
    iterator begin() noexcept { return data_; }

    const_iterator begin() const noexcept { return data_; }

    const_iterator cbegin() const noexcept { return data_; }

    iterator end() noexcept { return data_ + N; }

    const_iterator end() const noexcept { return data_ + N; }

    // Capacity
    bool empty() const noexcept { return N == 0; }

    fl::size size() const noexcept { return N; }

    fl::size max_size() const noexcept { return N; }

    // Operations
    void fill(const T &value) {
        for (fl::size i = 0; i < N; ++i) {
            data_[i] = value;
        }
    }

    void swap(array &other) {
        for (fl::size i = 0; i < N; ++i) {
            fl::swap(data_[i], other.data_[i]);
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
