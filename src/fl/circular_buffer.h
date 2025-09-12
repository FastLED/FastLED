#pragma once

#include "fl/math_macros.h"
#include "fl/namespace.h"
#include "fl/scoped_array.h"
#include "fl/stdint.h" // For standard integer types

namespace fl {

// TODO:
// ERROR bit to indicate over flow.

// Static version with compile-time capacity
template <typename T, fl::size N>
class StaticCircularBuffer {
  public:
    StaticCircularBuffer() : mHead(0), mTail(0) {}

    void push(const T &value) {
        if (full()) {
            mTail = (mTail + 1) % (N + 1); // Overwrite the oldest element
        }
        mBuffer[mHead] = value;
        mHead = (mHead + 1) % (N + 1);
    }

    bool pop(T &value) {
        if (empty()) {
            return false;
        }
        value = mBuffer[mTail];
        mTail = (mTail + 1) % (N + 1);
        return true;
    }

    fl::size size() const { return (mHead + N + 1 - mTail) % (N + 1); }
    constexpr fl::size capacity() const { return N; }
    bool empty() const { return mHead == mTail; }
    bool full() const { return ((mHead + 1) % (N + 1)) == mTail; }
    void clear() { mHead = mTail = 0; }

  private:
    T mBuffer[N + 1]; // Extra space for distinguishing full/empty
    fl::size mHead;
    fl::size mTail;
};

// Dynamic version with runtime capacity (existing implementation)
template <typename T> class DynamicCircularBuffer {
  public:
    DynamicCircularBuffer(fl::size capacity)
        : mCapacity(capacity + 1), mHead(0),
          mTail(0) { // Extra space for distinguishing full/empty
        mBuffer.reset(new T[mCapacity]);
    }

    DynamicCircularBuffer(const DynamicCircularBuffer &) = delete;
    DynamicCircularBuffer &operator=(const DynamicCircularBuffer &) = delete;

    bool push_back(const T &value) {
        if (full()) {
            mTail = increment(mTail); // Overwrite the oldest element
        }
        mBuffer[mHead] = value;
        mHead = increment(mHead);
        return true;
    }

    bool pop_front(T *dst = nullptr) {
        if (empty()) {
            return false;
        }
        if (dst) {
            *dst = mBuffer[mTail];
        }
        mTail = increment(mTail);
        return true;
    }

    bool push_front(const T &value) {
        if (full()) {
            mHead = decrement(mHead); // Overwrite the oldest element
        }
        mTail = decrement(mTail);
        mBuffer[mTail] = value;
        return true;
    }

    bool pop_back(T *dst = nullptr) {
        if (empty()) {
            return false;
        }
        mHead = decrement(mHead);
        if (dst) {
            *dst = mBuffer[mHead];
        }
        return true;
    }

    T &front() { return mBuffer[mTail]; }

    const T &front() const { return mBuffer[mTail]; }

    T &back() { return mBuffer[(mHead + mCapacity - 1) % mCapacity]; }

    const T &back() const {
        return mBuffer[(mHead + mCapacity - 1) % mCapacity];
    }

    T &operator[](fl::size index) { return mBuffer[(mTail + index) % mCapacity]; }

    const T &operator[](fl::size index) const {
        return mBuffer[(mTail + index) % mCapacity];
    }

    fl::size size() const { return (mHead + mCapacity - mTail) % mCapacity; }

    fl::size capacity() const { return mCapacity - 1; }

    bool empty() const { return mHead == mTail; }

    bool full() const { return increment(mHead) == mTail; }

    void clear() { mHead = mTail = 0; }

  private:
    fl::size increment(fl::size index) const { return (index + 1) % mCapacity; }

    fl::size decrement(fl::size index) const {
        return (index + mCapacity - 1) % mCapacity;
    }

    fl::scoped_array<T> mBuffer;
    fl::size mCapacity;
    fl::size mHead;
    fl::size mTail;
};

// For backward compatibility, keep the old name for the dynamic version
template <typename T>
using CircularBuffer = DynamicCircularBuffer<T>;

} // namespace fl
