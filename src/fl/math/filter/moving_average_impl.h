#pragma once

#include "fl/stl/circular_buffer.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace detail {

template <typename T, fl::size N = 0>
class MovingAverageImpl {
  public:
    MovingAverageImpl() FL_NOEXCEPT : mSum(T(0)) {}
    explicit MovingAverageImpl(fl::size capacity) : mBuf(capacity), mSum(T(0)) {}

    T update(T input) {
        if (mBuf.full()) {
            mSum = mSum - mBuf.front();
        }
        mBuf.push_back(input);
        mSum = mSum + input;
        return value();
    }

    T value() const {
        fl::size count = mBuf.size();
        if (count == 0) {
            return T(0);
        }
        return divByCount(mSum, count);
    }

    void reset() {
        mBuf.clear();
        mSum = T(0);
    }

    bool full() const { return mBuf.full(); }
    fl::size size() const { return mBuf.size(); }
    fl::size capacity() const { return mBuf.capacity(); }

    void resize(fl::size new_capacity) {
        mBuf = circular_buffer<T, N>(new_capacity);
        mSum = T(0);
    }

  private:
    template <typename U = T>
    static typename fl::enable_if<fl::is_floating_point<U>::value, U>::type
    divByCount(U sum, fl::size count) {
        return sum / static_cast<U>(count);
    }

    template <typename U = T>
    static typename fl::enable_if<fl::is_integral<U>::value, U>::type
    divByCount(U sum, fl::size count) {
        return sum / static_cast<U>(count);
    }

    template <typename U = T>
    static typename fl::enable_if<!fl::is_floating_point<U>::value &&
                                  !fl::is_integral<U>::value, U>::type
    divByCount(U sum, fl::size count) {
        return sum / U(static_cast<float>(count));
    }

    circular_buffer<T, N> mBuf;
    T mSum;
};

} // namespace detail
} // namespace fl
