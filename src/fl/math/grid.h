
#pragma once

#include "fl/math/geometry.h"
#include "fl/stl/vector.h"
#include "fl/stl/int.h"
#include "fl/stl/span.h"
#include "fl/stl/noexcept.h"

namespace fl {

template <typename T> class allocator_psram;

template <typename T> class Grid {
  public:
    Grid() FL_NO_EXCEPT = default;

    Grid(u32 width, u32 height) FL_NO_EXCEPT { reset(width, height); }

    void reset(u32 width, u32 height) FL_NO_EXCEPT {
        clear();
        if (width != mWidth || height != mHeight) {
            mWidth = width;
            mHeight = height;
            mData.resize(width * height);

        }
        mSlice = fl::MatrixSlice<T>(mData.data(), width, height, 0, 0,
                                    width, height);
    }

    void clear() FL_NO_EXCEPT {
        for (u32 i = 0; i < mWidth * mHeight; ++i) {
            mData[i] = T();
        }
    }

    vec2<T> minMax() const FL_NO_EXCEPT {
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

    T &at(u32 x, u32 y) FL_NO_EXCEPT { return access(x, y); }
    const T &at(u32 x, u32 y) const FL_NO_EXCEPT { return access(x, y); }

    T &operator()(u32 x, u32 y) FL_NO_EXCEPT { return at(x, y); }
    const T &operator()(u32 x, u32 y) const FL_NO_EXCEPT { return at(x, y); }

    u32 width() const FL_NO_EXCEPT { return mWidth; }
    u32 height() const FL_NO_EXCEPT { return mHeight; }

    fl::span<T> span() FL_NO_EXCEPT { return fl::span<T>(mData); }
    fl::span<const T> span() const FL_NO_EXCEPT { return fl::span<const T>(mData); }

    fl::size size() const FL_NO_EXCEPT { return mData.size(); }

  private:
    static T &NullValue() FL_NO_EXCEPT {
        static T gNull;
        return gNull;
    }
    T &access(u32 x, u32 y) FL_NO_EXCEPT {
        if (x < mWidth && y < mHeight) {
            return mSlice.at(x, y);
        } else {
            return NullValue(); // safe.
        }
    }
    const T &access(u32 x, u32 y) const FL_NO_EXCEPT {
        if (x < mWidth && y < mHeight) {
            return mSlice.at(x, y);
        } else {
            return NullValue(); // safe.
        }
    }
    fl::vector_psram<T> mData;
    u32 mWidth = 0;
    u32 mHeight = 0;
    fl::MatrixSlice<T> mSlice;
};

} // namespace fl
