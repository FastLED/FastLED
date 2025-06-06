
#pragma once

#include "fl/slice.h"
#include "fl/vector.h"

namespace fl {

template <typename T> class Grid {
  public:
    Grid() = default;

    Grid(uint32_t width, uint32_t height) { reset(width, height); }

    void reset(uint32_t width, uint32_t height) {
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
        for (uint32_t i = 0; i < mWidth * mHeight; ++i) {
            mData[i] = T();
        }
    }

    vec2<T> minMax() const {
        T minValue = mData[0];
        T maxValue = mData[0];
        for (uint32_t i = 1; i < mWidth * mHeight; ++i) {
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

    T &at(uint32_t x, uint32_t y) { return access(x, y); }
    const T &at(uint32_t x, uint32_t y) const { return access(x, y); }

    T &operator()(uint32_t x, uint32_t y) { return at(x, y); }
    const T &operator()(uint32_t x, uint32_t y) const { return at(x, y); }

    uint32_t width() const { return mWidth; }
    uint32_t height() const { return mHeight; }

    T* data() { return mData.data(); }
    const T* data() const { return mData.data(); }

    size_t size() const { return mData.size(); }

  private:
    static T &NullValue() {
        static T gNull;
        return gNull;
    }
    T &access(uint32_t x, uint32_t y) {
        if (x < mWidth && y < mHeight) {
            return mSlice.at(x, y);
        } else {
            return NullValue(); // safe.
        }
    }
    const T &access(uint32_t x, uint32_t y) const {
        if (x < mWidth && y < mHeight) {
            return mSlice.at(x, y);
        } else {
            return NullValue(); // safe.
        }
    }
    fl::vector<T> mData;
    uint32_t mWidth = 0;
    uint32_t mHeight = 0;
    fl::MatrixSlice<T> mSlice;
};

} // namespace fl