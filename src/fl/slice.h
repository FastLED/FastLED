#pragma once

#include <stddef.h>
#include <stdint.h>

#include "fl/clamp.h"
#include "fl/geometry.h"
#include "fl/allocator.h"

namespace fl {

template <typename T, size_t INLINED_SIZE> class FixedVector;

template <typename T, typename Allocator> class HeapVector;

template <typename T, size_t INLINED_SIZE> class InlinedVector;

// Slice<int> is equivalent to int* with a length. It is used to pass around
// arrays of integers with a length, without needing to pass around a separate
// length parameter.
// It works just like an array of objects, but it also knows its length.
template <typename T> class Slice {
  public:
    Slice() : mData(nullptr), mSize(0) {}
    Slice(T *data, size_t size) : mData(data), mSize(size) {}

    template<typename Alloc>
    Slice(const HeapVector<T, Alloc> &vector)
        : mData(vector.data()), mSize(vector.size()) {}

    template <size_t INLINED_SIZE>
    Slice(const FixedVector<T, INLINED_SIZE> &vector)
        : mData(vector.data()), mSize(vector.size()) {}

    template <size_t INLINED_SIZE>
    Slice(const InlinedVector<T, INLINED_SIZE> &vector)
        : mData(vector.data()), mSize(vector.size()) {}

    template <typename U, typename Alloc>
    Slice(const HeapVector<U, Alloc> &vector)
        : mData(vector.data()), mSize(vector.size()) {}

    template <typename U, size_t INLINED_SIZE>
    Slice(const FixedVector<U, INLINED_SIZE> &vector)
        : mData(vector.data()), mSize(vector.size()) {}

    template <typename U, size_t INLINED_SIZE>
    Slice(const InlinedVector<U, INLINED_SIZE> &vector)
        : mData(vector.data()), mSize(vector.size()) {}

    template <size_t ARRAYSIZE>
    Slice(T (&array)[ARRAYSIZE]) : mData(array), mSize(ARRAYSIZE) {}

    template <typename U, size_t ARRAYSIZE>
    Slice(T (&array)[ARRAYSIZE]) : mData(array), mSize(ARRAYSIZE) {}

    template <typename Iterator>
    Slice(Iterator begin, Iterator end)
        : mData(&(*begin)), mSize(end - begin) {}

    Slice(const Slice &other) : mData(other.mData), mSize(other.mSize) {}

    Slice &operator=(const Slice &other) {
        mData = other.mData;
        mSize = other.mSize;
        return *this;
    }

    // Automatic promotion to const Slice<const T>
    operator Slice<const T>() const { return Slice<const T>(mData, mSize); }

    T &operator[](size_t index) {
        // No bounds checking in embedded environment
        return mData[index];
    }

    const T &operator[](size_t index) const {
        // No bounds checking in embedded environment
        return mData[index];
    }

    T *begin() const { return mData; }

    T *end() const { return mData + mSize; }

    size_t length() const { return mSize; }

    const T *data() const { return mData; }

    T *data() { return mData; }

    size_t size() const { return mSize; }

    Slice<T> slice(size_t start, size_t end) const {
        // No bounds checking in embedded environment
        return Slice<T>(mData + start, end - start);
    }

    Slice<T> slice(size_t start) const {
        // No bounds checking in embedded environment
        return Slice<T>(mData + start, mSize - start);
    }

    // Find the first occurrence of a value in the slice
    // Returns the index of the first occurrence if found, or size_t(-1) if not
    // found
    size_t find(const T &value) const {
        for (size_t i = 0; i < mSize; ++i) {
            if (mData[i] == value) {
                return i;
            }
        }
        return size_t(-1);
    }

    bool pop_front() {
        if (mSize == 0) {
            return false;
        }
        ++mData;
        --mSize;
        return true;
    }

    bool pop_back() {
        if (mSize == 0) {
            return false;
        }
        --mSize;
        return true;
    }

    T &front() { return *mData; }

    const T &front() const { return *mData; }

    T &back() { return *(mData + mSize - 1); }

    const T &back() const { return *(mData + mSize - 1); }

    bool empty() { return mSize == 0; }

  private:
    T *mData;
    size_t mSize;
};
template <typename T> class MatrixSlice {
  public:
    // represents a window into a matrix
    // bottom-left and top-right corners are passed as plain ints
    MatrixSlice() = default;
    MatrixSlice(T *data, int32_t dataWidth, int32_t dataHeight,
                int32_t bottomLeftX, int32_t bottomLeftY, int32_t topRightX,
                int32_t topRightY)
        : mData(data), mDataWidth(dataWidth), mDataHeight(dataHeight),
          mBottomLeft{bottomLeftX, bottomLeftY},
          mTopRight{topRightX, topRightY} {}

    MatrixSlice(const MatrixSlice &other) = default;
    MatrixSlice &operator=(const MatrixSlice &other) = default;

    // outputs a vec2 but takes x,y as inputs
    vec2<int32_t> getParentCoord(int32_t x_local, int32_t y_local) const {
        return {x_local + mBottomLeft.x, y_local + mBottomLeft.y};
    }

    vec2<int32_t> getLocalCoord(int32_t x_world, int32_t y_world) const {
        // clamp to [mBottomLeft, mTopRight]
        int32_t x_clamped = fl::clamp(x_world, mBottomLeft.x, mTopRight.x);
        int32_t y_clamped = fl::clamp(y_world, mBottomLeft.y, mTopRight.y);
        // convert to local
        return {x_clamped - mBottomLeft.x, y_clamped - mBottomLeft.y};
    }

    // element access via (x,y)
    T &operator()(int32_t x, int32_t y) { return at(x, y); }

    // Add access like slice[y][x]
    T *operator[](int32_t row) {
        int32_t parentRow = row + mBottomLeft.y;
        return mData + parentRow * mDataWidth + mBottomLeft.x;
    }

    T &at(int32_t x, int32_t y) {
        auto parent = getParentCoord(x, y);
        return mData[parent.x + parent.y * mDataWidth];
    }

    const T &at(int32_t x, int32_t y) const {
        auto parent = getParentCoord(x, y);
        return mData[parent.x + parent.y * mDataWidth];
    }

  private:
    T *mData = nullptr;
    int32_t mDataWidth = 0;
    int32_t mDataHeight = 0;
    vec2<int32_t> mBottomLeft;
    vec2<int32_t> mTopRight;
};

} // namespace fl