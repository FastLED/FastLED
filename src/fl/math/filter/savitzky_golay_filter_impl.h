#pragma once

#include "fl/stl/circular_buffer.h"
#include "fl/system/log.h"
#include "fl/math/filter/div_by_count.h"
#include "fl/stl/span.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace detail {

template <typename T, fl::size N = 0>
class SavitzkyGolayFilterImpl {
    static_assert(N == 0 || (N % 2 == 1),
                  "SavitzkyGolayFilter: N must be odd for symmetric polynomial fit");
  public:
    SavitzkyGolayFilterImpl() FL_NOEXCEPT : mLastValue(T(0)) {}
    explicit SavitzkyGolayFilterImpl(fl::size capacity)
        : mBuf(capacity), mLastValue(T(0)) {
        if (capacity % 2 == 0) {
            FL_ERROR("SavitzkyGolayFilter: capacity should be odd, adding 1");
            mBuf = circular_buffer<T, N>(capacity + 1);
        }
    }

    T update(T input) {
        mBuf.push_back(input);
        return recompute(input);
    }

    T update(fl::span<const T> values) {
        if (values.size() == 0) return mLastValue;
        for (fl::size i = 0; i < values.size(); ++i) {
            mBuf.push_back(values[i]);
        }
        return recompute(values[values.size() - 1]);
    }

    T value() const { return mLastValue; }
    void reset() { mBuf.clear(); mLastValue = T(0); }
    bool full() const { return mBuf.full(); }
    fl::size size() const { return mBuf.size(); }
    fl::size capacity() const { return mBuf.capacity(); }

    void resize(fl::size new_capacity) {
        if (new_capacity % 2 == 0) {
            FL_ERROR("SavitzkyGolayFilter: capacity should be odd, adding 1");
            new_capacity += 1;
        }
        mBuf = circular_buffer<T, N>(new_capacity);
        mLastValue = T(0);
    }

  private:
    T recompute(T fallback) {
        fl::size n = mBuf.size();

        if (n < 5) {
            T sum = T(0);
            for (fl::size i = 0; i < n; ++i) sum = sum + mBuf[i];
            mLastValue = div_by_count(sum, n);
            return mLastValue;
        }

        int M = static_cast<int>((n - 1) / 2);
        int base_term = 3 * M * (M + 1) - 1;

        T weighted_sum = T(0);
        T weight_total = T(0);
        for (fl::size j = 0; j < n; ++j) {
            int i = static_cast<int>(j) - M;
            int c = 3 * (base_term - 5 * i * i);
            T w = T(static_cast<float>(c));
            weighted_sum = weighted_sum + mBuf[j] * w;
            weight_total = weight_total + w;
        }

        if (!(weight_total == T(0))) {
            mLastValue = weighted_sum / weight_total;
        } else {
            mLastValue = fallback;
        }
        return mLastValue;
    }

    circular_buffer<T, N> mBuf;
    T mLastValue;
};

} // namespace detail
} // namespace fl
