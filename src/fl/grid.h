
#pragma once

#include "fl/span.h"
#include "fl/vector.h"
#include "fl/allocator.h"

namespace fl {


template <typename T> class Grid {
  public:
    Grid() = default;

    Grid(u32 width, u32 height) { reset(width, height); }

    void reset(u32 width, u32 height) {
        clear();
        if (width != mWidth || height != mHeight) {
            mWidth = width;
            mHeight = height;
            mData.resize(width * height);

        }
        mSlice = fl::MatrixSlice<T>(mData.data(), width, height, 0, 0,
                                    width, height);
    }


    void clear() {
        for (u32 i = 0; i < mWidth * mHeight; ++i) {
            mData[i] = T();
        }
    }

    vec2<T> minMax() const {
        T minValue = mData[0];
        T maxValue = mData[0];
        for (u32 i = 1; i < mWidth * mHeight; ++i) {
            if (mData[i] < minValue) {
                minValue = mData[i];
            }
            if (mData[i] > maxValue) {
                maxValue = mData[i];
            }
        }
        // *min = minValue;
        // *max = maxValue;
        vec2<T> out(minValue, maxValue);
        return out;
    }

    T &at(u32 x, u32 y) { return access(x, y); }
    const T &at(u32 x, u32 y) const { return access(x, y); }

    T &operator()(u32 x, u32 y) { return at(x, y); }
    const T &operator()(u32 x, u32 y) const { return at(x, y); }

    u32 width() const { return mWidth; }
    u32 height() const { return mHeight; }

    T* data() { return mData.data(); }
    const T* data() const { return mData.data(); }

    fl::size size() const { return mData.size(); }

  private:
    static T &NullValue() {
        static T gNull;
        return gNull;
    }
    T &access(u32 x, u32 y) {
        if (x < mWidth && y < mHeight) {
            return mSlice.at(x, y);
        } else {
            return NullValue(); // safe.
        }
    }
    const T &access(u32 x, u32 y) const {
        if (x < mWidth && y < mHeight) {
            return mSlice.at(x, y);
        } else {
            return NullValue(); // safe.
        }
    }
    fl::vector<T, fl::allocator_psram<T>> mData;
    u32 mWidth = 0;
    u32 mHeight = 0;
    fl::MatrixSlice<T> mSlice;
};

} // namespace fl
