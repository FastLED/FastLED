#pragma once

#include "fl/point.h"
#include <stddef.h>
#include <stdint.h>

namespace fl {

// Slice<int> is equivalent to int* with a length. It is used to pass around
// arrays of integers with a length, without needing to pass around a separate
// length parameter.
// It works just like an array of objects, but it also knows its length.
template <typename T> class Slice {
  public:
    Slice() : mData(nullptr), mSize(0) {}
    Slice(T *data, size_t size) : mData(data), mSize(size) {}

    Slice(const Slice &other) : mData(other.mData), mSize(other.mSize) {}

    Slice &operator=(const Slice &other) {
        mData = other.mData;
        mSize = other.mSize;
        return *this;
    }

    // Automatic promotion to const Slice<const T>
    operator Slice<const T>() const { return *this; }

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

template <typename T, typename point_t = point_xy<int>> class MatrixSlice {
  public:
    // represents a window into a matrix
    MatrixSlice(T *data, size_t width, size_t height,
                point_t bottomLeft, point_t topRight)
        : mData(data), mDataWidth(width), mDataHeight(height),
          mBottomLeft(bottomLeft), mTopRight(topRight) {}

    point_t getParentCoord(point_t p) const {
        // convert from local coordinates to parent coordinates
        return point_t(p.x + mBottomLeft.x, p.y + mBottomLeft.y);
    }

    point_t getLocalCoord(point_t p) const {
        // convert from parent coordinates to local coordinates
        if (p.x < mBottomLeft.x || p.x > mTopRight.x || p.y < mBottomLeft.y ||
            p.y > mTopRight.y) {
            return point_t(0, 0);
        }
        return point_t(p.x - mBottomLeft.x, p.y - mBottomLeft.y);
    }

    T &operator()(point_t p) {
        // convert from local coordinates to parent coordinates
        return at(p);
    }

    T& at(point_t p) {
        // convert from local coordinates to parent coordinates
        point_t parentCoord = getParentCoord(p);
        return mData[parentCoord.x + parentCoord.y * mDataWidth];
    }

  private:
    T *mData;
    size_t mDataWidth;
    size_t mDataHeight;
    point_t mBottomLeft;
    point_t mTopRight;
};

} // namespace fl