#pragma once

#include "fl/stl/circular_buffer.h"
#include "fl/math/math.h"
#include "fl/stl/span.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace detail {

template <typename T, fl::size N = 0>
class BilateralFilterImpl {
  public:
    explicit BilateralFilterImpl(T sigma_range = T(1.0f))
        FL_NO_EXCEPT : mSigmaRange(sigma_range), mLastValue(T(0)) {}
    explicit BilateralFilterImpl(fl::size capacity, T sigma_range)
        FL_NO_EXCEPT : mBuf(capacity), mSigmaRange(sigma_range), mLastValue(T(0)) {}

    T update(T input) FL_NO_EXCEPT {
        mBuf.push_back(input);
        return recompute(input);
    }

    T update(fl::span<const T> values) FL_NO_EXCEPT {
        if (values.size() == 0) return mLastValue;
        for (fl::size i = 0; i < values.size(); ++i) {
            mBuf.push_back(values[i]);
        }
        return recompute(values[values.size() - 1]);
    }

    T value() const FL_NO_EXCEPT { return mLastValue; }
    void reset() FL_NO_EXCEPT { mBuf.clear(); mLastValue = T(0); }
    bool full() const FL_NO_EXCEPT { return mBuf.full(); }
    fl::size size() const FL_NO_EXCEPT { return mBuf.size(); }
    fl::size capacity() const FL_NO_EXCEPT { return mBuf.capacity(); }

    void resize(fl::size new_capacity) FL_NO_EXCEPT {
        mBuf = circular_buffer<T, N>(new_capacity);
        mLastValue = T(0);
    }

  private:
    T recompute(T reference) FL_NO_EXCEPT {
        fl::size n = mBuf.size();

        T two_sigma_sq = T(2.0f) * mSigmaRange * mSigmaRange;

        if (two_sigma_sq == T(0)) {
            mLastValue = reference;
            return mLastValue;
        }

        T weighted_sum = T(0);
        T weight_total = T(0);

        for (fl::size i = 0; i < n; ++i) {
            T diff = mBuf[i] - reference;
            T range_weight = fl::exp(-(diff * diff) / two_sigma_sq);
            weighted_sum = weighted_sum + mBuf[i] * range_weight;
            weight_total = weight_total + range_weight;
        }

        if (!(weight_total == T(0))) {
            mLastValue = weighted_sum / weight_total;
        } else {
            mLastValue = reference;
        }
        return mLastValue;
    }

    circular_buffer<T, N> mBuf;
    T mSigmaRange;
    T mLastValue;
};

} // namespace detail
} // namespace fl
