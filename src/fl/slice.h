#pragma once


#include "fl/stdint.h"
#include "fl/clamp.h"
#include "fl/geometry.h"
#include "fl/allocator.h"

namespace fl {

template <typename T, fl::size INLINED_SIZE> class FixedVector;

template <typename T, typename Allocator> class HeapVector;

template <typename T, fl::size INLINED_SIZE> class InlinedVector;

template <typename T, fl::size N> class array;

// Slice<int> is equivalent to int* with a length. It is used to pass around
// arrays of integers with a length, without needing to pass around a separate
// length parameter.
// It works just like an array of objects, but it also knows its length.
template <typename T> class Slice {
  public:
    Slice() : mData(nullptr), mSize(0) {}
    Slice(T *data, fl::size size) : mData(data), mSize(size) {}

    // ======= CONTAINER CONSTRUCTORS =======
    // Simple constructors that work for all cases
    template<typename Alloc>
    Slice(const HeapVector<T, Alloc> &vector)
        : mData(vector.data()), mSize(vector.size()) {}

    template <fl::size INLINED_SIZE>
    Slice(const FixedVector<T, INLINED_SIZE> &vector)
        : mData(vector.data()), mSize(vector.size()) {}

    template <fl::size INLINED_SIZE>
    Slice(const InlinedVector<T, INLINED_SIZE> &vector)
        : mData(vector.data()), mSize(vector.size()) {}

    // Additional constructors for const conversion (U -> const U)
    template<typename U, typename Alloc>
    Slice(const HeapVector<U, Alloc> &vector)
        : mData(vector.data()), mSize(vector.size()) {}

    template<typename U, fl::size INLINED_SIZE>
    Slice(const FixedVector<U, INLINED_SIZE> &vector)
        : mData(vector.data()), mSize(vector.size()) {}

    template<typename U, fl::size INLINED_SIZE>
    Slice(const InlinedVector<U, INLINED_SIZE> &vector)
        : mData(vector.data()), mSize(vector.size()) {}

    // ======= NON-CONST CONTAINER CONVERSIONS =======
    // Non-const versions for mutable spans
    template<typename Alloc>
    Slice(HeapVector<T, Alloc> &vector)
        : mData(vector.data()), mSize(vector.size()) {}

    template <fl::size INLINED_SIZE>
    Slice(FixedVector<T, INLINED_SIZE> &vector)
        : mData(vector.data()), mSize(vector.size()) {}

    template <fl::size INLINED_SIZE>
    Slice(InlinedVector<T, INLINED_SIZE> &vector)
        : mData(vector.data()), mSize(vector.size()) {}



    // ======= FL::ARRAY CONVERSIONS =======
    // fl::array<T> -> Slice<T>
    template <fl::size N>
    Slice(const array<T, N> &arr)
        : mData(arr.data()), mSize(N) {}

    template <fl::size N>
    Slice(array<T, N> &arr)
        : mData(arr.data()), mSize(N) {}

    // fl::array<U> -> Slice<T> (for type conversions like U -> const U)
    template <typename U, fl::size N>
    Slice(const array<U, N> &arr)
        : mData(arr.data()), mSize(N) {}

    template <typename U, fl::size N>
    Slice(array<U, N> &arr)
        : mData(arr.data()), mSize(N) {}

    // ======= C-STYLE ARRAY CONVERSIONS =======
    // T[] -> Slice<T>
    template <fl::size ARRAYSIZE>
    Slice(T (&array)[ARRAYSIZE]) 
        : mData(array), mSize(ARRAYSIZE) {}

    // U[] -> Slice<T> (for type conversions like U -> const U)
    template <typename U, fl::size ARRAYSIZE>
    Slice(U (&array)[ARRAYSIZE]) 
        : mData(array), mSize(ARRAYSIZE) {}

    // const U[] -> Slice<T> (for const arrays)
    template <typename U, fl::size ARRAYSIZE>
    Slice(const U (&array)[ARRAYSIZE]) 
        : mData(array), mSize(ARRAYSIZE) {}

    // ======= ITERATOR CONVERSIONS =======
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

    T &operator[](fl::size index) {
        // No bounds checking in embedded environment
        return mData[index];
    }

    const T &operator[](fl::size index) const {
        // No bounds checking in embedded environment
        return mData[index];
    }

    T *begin() const { return mData; }

    T *end() const { return mData + mSize; }

    fl::size length() const { return mSize; }

    const T *data() const { return mData; }

    T *data() { return mData; }

    fl::size size() const { return mSize; }

    Slice<T> slice(fl::size start, fl::size end) const {
        // No bounds checking in embedded environment
        return Slice<T>(mData + start, end - start);
    }

    Slice<T> slice(fl::size start) const {
        // No bounds checking in embedded environment
        return Slice<T>(mData + start, mSize - start);
    }

    // Find the first occurrence of a value in the slice
    // Returns the index of the first occurrence if found, or fl::size(-1) if not
    // found
    fl::size find(const T &value) const {
        for (fl::size i = 0; i < mSize; ++i) {
            if (mData[i] == value) {
                return i;
            }
        }
        return fl::size(-1);
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
    fl::size mSize;
};
template <typename T> class MatrixSlice {
  public:
    // represents a window into a matrix
    // bottom-left and top-right corners are passed as plain ints
    MatrixSlice() = default;
    MatrixSlice(T *data, i32 dataWidth, i32 dataHeight,
                i32 bottomLeftX, i32 bottomLeftY, i32 topRightX,
                i32 topRightY)
        : mData(data), mDataWidth(dataWidth), mDataHeight(dataHeight),
          mBottomLeft{bottomLeftX, bottomLeftY},
          mTopRight{topRightX, topRightY} {}

    MatrixSlice(const MatrixSlice &other) = default;
    MatrixSlice &operator=(const MatrixSlice &other) = default;

    // outputs a vec2 but takes x,y as inputs
    vec2<i32> getParentCoord(i32 x_local, i32 y_local) const {
        return {x_local + mBottomLeft.x, y_local + mBottomLeft.y};
    }

    vec2<i32> getLocalCoord(i32 x_world, i32 y_world) const {
        // clamp to [mBottomLeft, mTopRight]
        i32 x_clamped = fl::clamp(x_world, mBottomLeft.x, mTopRight.x);
        i32 y_clamped = fl::clamp(y_world, mBottomLeft.y, mTopRight.y);
        // convert to local
        return {x_clamped - mBottomLeft.x, y_clamped - mBottomLeft.y};
    }

    // element access via (x,y)
    T &operator()(i32 x, i32 y) { return at(x, y); }

    // Add access like slice[y][x]
    T *operator[](i32 row) {
        i32 parentRow = row + mBottomLeft.y;
        return mData + parentRow * mDataWidth + mBottomLeft.x;
    }

    T &at(i32 x, i32 y) {
        auto parent = getParentCoord(x, y);
        return mData[parent.x + parent.y * mDataWidth];
    }

    const T &at(i32 x, i32 y) const {
        auto parent = getParentCoord(x, y);
        return mData[parent.x + parent.y * mDataWidth];
    }

  private:
    T *mData = nullptr;
    i32 mDataWidth = 0;
    i32 mDataHeight = 0;
    vec2<i32> mBottomLeft;
    vec2<i32> mTopRight;
};

} // namespace fl
