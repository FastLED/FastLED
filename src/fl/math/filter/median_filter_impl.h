#pragma once

#include "fl/stl/circular_buffer.h"
#include "fl/system/log.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/span.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace detail {

template <typename T, fl::size N = 0>
class MedianFilterImpl {
    static_assert(N == 0 || (N % 2 == 1),
                  "MedianFilter: N must be odd for an unambiguous median");
  public:
    MedianFilterImpl() FL_NOEXCEPT : mSortedCount(0), mLastMedian(T(0)) {}
    explicit MedianFilterImpl(fl::size capacity)
        : mRing(capacity), mSorted(capacity),
          mSortedCount(0), mLastMedian(T(0)) {
        if (capacity % 2 == 0) {
            FL_ERROR("MedianFilter: capacity should be odd, adding 1");
            mRing = circular_buffer<T, N>(capacity + 1);
            mSorted = circular_buffer<T, N>(capacity + 1);
        }
    }

    T update(fl::span<const T> values) {
        if (values.size() == 0) return mLastMedian;
        for (fl::size i = 0; i < values.size(); ++i) {
            mRing.push_back(values[i]);
        }
        // Rebuild sorted array from ring contents
        mSortedCount = mRing.size();
        for (fl::size i = 0; i < mSortedCount; ++i) {
            mSorted[i] = mRing[i];
        }
        fl::sort(&mSorted[0], &mSorted[0] + mSortedCount);
        mLastMedian = mSorted[mSortedCount / 2];
        return mLastMedian;
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
            T* ins_pos =
                fl::lower_bound(base, base + mSortedCount - 1, input);
            fl::size idx = static_cast<fl::size>(ins_pos - base);
            for (fl::size i = mSortedCount - 1; i > idx; --i) {
                mSorted[i] = mSorted[i - 1];
            }
            mSorted[idx] = input;
        }

        mRing.push_back(input);
        mLastMedian = mSorted[mSortedCount / 2];
        return mLastMedian;
    }

    T value() const { return mLastMedian; }

    void reset() {
        mRing.clear();
        mSortedCount = 0;
        mLastMedian = T(0);
    }

    fl::size size() const { return mRing.size(); }
    fl::size capacity() const { return mRing.capacity(); }

    void resize(fl::size new_capacity) {
        if (new_capacity % 2 == 0) {
            FL_ERROR("MedianFilter: capacity should be odd, adding 1");
            new_capacity += 1;
        }
        mRing = circular_buffer<T, N>(new_capacity);
        mSorted = circular_buffer<T, N>(new_capacity);
        mSortedCount = 0;
        mLastMedian = T(0);
    }

  private:
    circular_buffer<T, N> mRing;
    circular_buffer<T, N> mSorted;
    fl::size mSortedCount;
    T mLastMedian;
};

} // namespace detail
} // namespace fl
