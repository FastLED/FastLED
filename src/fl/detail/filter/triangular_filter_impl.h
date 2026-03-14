#pragma once

#include "fl/stl/circular_buffer.h"
#include "fl/system/log.h"
#include "fl/stl/math.h"
#include "fl/stl/span.h"

namespace fl {
namespace detail {

template <typename T, fl::size N = 0>
class TriangularFilterImpl {
    static_assert(N == 0 || (N % 2 == 1),
                  "TriangularFilter: N must be odd for a symmetric tent shape");
  public:
    TriangularFilterImpl() : mLastValue(T(0)) {}
    explicit TriangularFilterImpl(fl::size capacity)
        : mBuf(capacity), mLastValue(T(0)) {
        if (capacity % 2 == 0) {
            FL_ERROR("TriangularFilter: capacity should be odd, adding 1");
            mBuf = circular_buffer<T, N>(capacity + 1);
        }
    }

    T update(T input) {
        mBuf.push_back(input);
        return recompute();
    }

    T update(fl::span<const T> values) {
        if (values.size() == 0) return mLastValue;
        for (fl::size i = 0; i < values.size(); ++i) {
            mBuf.push_back(values[i]);
        }
        return recompute();
    }

    T value() const { return mLastValue; }
    void reset() { mBuf.clear(); mLastValue = T(0); }
    bool full() const { return mBuf.full(); }
    fl::size size() const { return mBuf.size(); }
    fl::size capacity() const { return mBuf.capacity(); }

    void resize(fl::size new_capacity) {
        if (new_capacity % 2 == 0) {
            FL_ERROR("TriangularFilter: capacity should be odd, adding 1");
            new_capacity += 1;
        }
        mBuf = circular_buffer<T, N>(new_capacity);
        mLastValue = T(0);
    }

  private:
    T recompute() {
        fl::size n = mBuf.size();
        T weighted_sum = T(0);
        T weight_total = T(0);
        for (fl::size i = 0; i < n; ++i) {
            fl::size w_int = fl::min(i + 1, n - i);
            T w = T(static_cast<float>(w_int));
            weighted_sum = weighted_sum + mBuf[i] * w;
            weight_total = weight_total + w;
        }
        mLastValue = weighted_sum / weight_total;
        return mLastValue;
    }

    circular_buffer<T, N> mBuf;
    T mLastValue;
};

} // namespace detail
} // namespace fl
