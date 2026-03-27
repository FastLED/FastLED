#pragma once

#include "fl/stl/type_traits.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace detail {

template <typename T, int K = 2>
class LeakyIntegratorImpl {
  public:
    LeakyIntegratorImpl() FL_NOEXCEPT : mY(T(0)) {}
    explicit LeakyIntegratorImpl(T initial) : mY(initial) {}

    T update(T input) {
        mY = mY + shift_right(input - mY);
        return mY;
    }

    T value() const { return mY; }
    void reset(T initial = T(0)) { mY = initial; }

  private:
    template <typename U = T>
    static typename fl::enable_if<fl::is_floating_point<U>::value, U>::type
    shift_right(U val) { return val / static_cast<U>(1 << K); }

    template <typename U = T>
    static typename fl::enable_if<!fl::is_floating_point<U>::value, U>::type
    shift_right(U val) { return val >> K; }

    T mY;
};

} // namespace detail
} // namespace fl
