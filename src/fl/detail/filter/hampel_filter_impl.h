#pragma once

#include "fl/stl/circular_buffer.h"
#include "fl/log.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/span.h"
#include "fl/detail/filter/div_by_count.h"

namespace fl {
namespace detail {

template <typename T, fl::size N = 0>
class HampelFilterImpl {
    static_assert(N == 0 || (N % 2 == 1),
                  "HampelFilter: N must be odd for an unambiguous median");
  public:
    explicit HampelFilterImpl(T threshold = T(3.0f))
        : mSortedCount(0), mThreshold(threshold), mLastValue(T(0)) {}
    explicit HampelFilterImpl(fl::size capacity, T threshold = T(3.0f))
        : mRing(capacity), mSorted(capacity),
          mSortedCount(0), mThreshold(threshold), mLastValue(T(0)) {
        if (capacity % 2 == 0) {
            FL_ERROR("HampelFilter: capacity should be odd, adding 1");
            mRing = circular_buffer<T, N>(capacity + 1);
            mSorted = circular_buffer<T, N>(capacity + 1);
        }
    }

    T update(fl::span<const T> values) {
        if (values.size() == 0) return mLastValue;
        T result = mLastValue;
        for (fl::size i = 0; i < values.size(); ++i) {
            result = update(values[i]);
        }
        return result;
    }

    T update(T input) {
        T output = input;

        if (mSortedCount > 0) {
            T median = mSorted[mSortedCount / 2];
            T mad_sum = T(0);
            for (fl::size i = 0; i < mSortedCount; ++i) {
                mad_sum = mad_sum + fl::abs(mSorted[i] - median);
            }
            T mad = div_by_count(mad_sum, mSortedCount);
            T deviation = fl::abs(input - median);
            if (mad == T(0)) {
                if (!(deviation == T(0))) {
                    output = median;
                }
            } else if (deviation > mThreshold * mad) {
                output = median;
            }
        }

        if (!mRing.full()) {
            T* base = &mSorted[0];
            T* pos = fl::lower_bound(base, base + mSortedCount, input);
            fl::size idx = static_cast<fl::size>(pos - base);
            for (fl::size i = mSortedCount; i > idx; --i) {
                mSorted[i] = mSorted[i - 1];
            }
            mSorted[idx] = input;
            ++mSortedCount;
        } else {
            T oldest = mRing.front();
            T* base = &mSorted[0];
            T* rm_pos = fl::lower_bound(base, base + mSortedCount, oldest);
            fl::size rm = static_cast<fl::size>(rm_pos - base);
            for (fl::size i = rm; i + 1 < mSortedCount; ++i) {
                mSorted[i] = mSorted[i + 1];
            }
            T* ins_pos = fl::lower_bound(base, base + mSortedCount - 1, input);
            fl::size idx = static_cast<fl::size>(ins_pos - base);
            for (fl::size i = mSortedCount - 1; i > idx; --i) {
                mSorted[i] = mSorted[i - 1];
            }
            mSorted[idx] = input;
        }
        mRing.push_back(input);

        mLastValue = output;
        return mLastValue;
    }

    T value() const { return mLastValue; }

    void reset() {
        mRing.clear();
        mSortedCount = 0;
        mLastValue = T(0);
    }

    fl::size size() const { return mRing.size(); }
    fl::size capacity() const { return mRing.capacity(); }

    void resize(fl::size new_capacity) {
        if (new_capacity % 2 == 0) {
            FL_ERROR("HampelFilter: capacity should be odd, adding 1");
            new_capacity += 1;
        }
        mRing = circular_buffer<T, N>(new_capacity);
        mSorted = circular_buffer<T, N>(new_capacity);
        mSortedCount = 0;
        mLastValue = T(0);
    }

  private:
    circular_buffer<T, N> mRing;
    circular_buffer<T, N> mSorted;
    fl::size mSortedCount;
    T mThreshold;
    T mLastValue;
};

} // namespace detail
} // namespace fl
