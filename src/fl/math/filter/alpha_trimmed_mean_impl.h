#pragma once

#include "fl/stl/circular_buffer.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/span.h"
#include "fl/math/filter/div_by_count.h"

namespace fl {
namespace detail {

template <typename T, fl::size N = 0>
class AlphaTrimmedMeanImpl {
  public:
    explicit AlphaTrimmedMeanImpl(fl::size trim_count = 1)
        : mSortedCount(0), mTrimCount(trim_count), mLastValue(T(0)) {}
    explicit AlphaTrimmedMeanImpl(fl::size capacity, fl::size trim_count)
        : mRing(capacity), mSorted(capacity),
          mSortedCount(0), mTrimCount(trim_count), mLastValue(T(0)) {}

    T update(fl::span<const T> values) {
        if (values.size() == 0) return mLastValue;
        for (fl::size i = 0; i < values.size(); ++i) {
            mRing.push_back(values[i]);
        }
        // Rebuild sorted array from ring contents
        mSortedCount = mRing.size();
        for (fl::size i = 0; i < mSortedCount; ++i) {
            mSorted[i] = mRing[i];
        }
        fl::sort(&mSorted[0], &mSorted[0] + mSortedCount);
        // Compute trimmed mean
        fl::size lo = mTrimCount;
        fl::size hi = (mSortedCount > mTrimCount) ? mSortedCount - mTrimCount : 0;
        if (lo >= hi) {
            mLastValue = mSorted[mSortedCount / 2];
        } else {
            T sum = T(0);
            for (fl::size i = lo; i < hi; ++i) {
                sum = sum + mSorted[i];
            }
            mLastValue = div_by_count(sum, hi - lo);
        }
        return mLastValue;
    }

    T update(T input) {
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

        fl::size lo = mTrimCount;
        fl::size hi = (mSortedCount > mTrimCount) ? mSortedCount - mTrimCount : 0;
        if (lo >= hi) {
            mLastValue = mSorted[mSortedCount / 2];
        } else {
            T sum = T(0);
            for (fl::size i = lo; i < hi; ++i) {
                sum = sum + mSorted[i];
            }
            mLastValue = div_by_count(sum, hi - lo);
        }
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

    void resize(fl::size new_capacity, fl::size trim_count) {
        mRing = circular_buffer<T, N>(new_capacity);
        mSorted = circular_buffer<T, N>(new_capacity);
        mSortedCount = 0;
        mTrimCount = trim_count;
        mLastValue = T(0);
    }

  private:
    circular_buffer<T, N> mRing;
    circular_buffer<T, N> mSorted;
    fl::size mSortedCount;
    fl::size mTrimCount;
    T mLastValue;
};

} // namespace detail
} // namespace fl
