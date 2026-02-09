#pragma once

#include "fl/scoped_array.h"
#include "fl/int.h"  // for size
#include "fl/stl/move.h"  // for fl::move

namespace fl {

// TODO:
// ERROR bit to indicate over flow.

// Static version with compile-time capacity
template <typename T, fl::size N>
class StaticCircularBuffer {
  public:
    StaticCircularBuffer() : mHead(0), mTail(0) {}

    StaticCircularBuffer(const StaticCircularBuffer& other) = default;
    StaticCircularBuffer(StaticCircularBuffer&& other) : mHead(0), mTail(0) {
        *this = fl::move(other);
    }

    StaticCircularBuffer& operator=(const StaticCircularBuffer& other) = default;
    StaticCircularBuffer& operator=(StaticCircularBuffer&& other) {
        if (this != &other) {
            // Move each element from other's buffer to this buffer
            for (fl::size i = 0; i < (N + 1); ++i) {
                mBuffer[i] = fl::move(other.mBuffer[i]);
            }
            mHead = other.mHead;
            mTail = other.mTail;
            // Clear the source
            other.clear();
        }
        return *this;
    }

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
        value = fl::move(mBuffer[mTail]);
        mBuffer[mTail] = T();  // Properly destroy the element by assigning default value
        mTail = (mTail + 1) % (N + 1);
        return true;
    }

    T& front() {
        return mBuffer[mTail];
    }

    const T& front() const {
        return mBuffer[mTail];
    }

    fl::size size() const { return (mHead + N + 1 - mTail) % (N + 1); }
    constexpr fl::size capacity() const { return N; }
    bool empty() const { return mHead == mTail; }
    bool full() const { return ((mHead + 1) % (N + 1)) == mTail; }
    void clear() {
        // Properly destroy all elements by assigning default-constructed values
        while (!empty()) {
            T dummy;
            pop(dummy);
        }
    }

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

    // Move constructor
    DynamicCircularBuffer(DynamicCircularBuffer&& other) noexcept
        : mBuffer(fl::move(other.mBuffer)),
          mCapacity(other.mCapacity),
          mHead(other.mHead),
          mTail(other.mTail) {
        // Leave other in valid empty state (mHead == mTail means empty)
        // Keep mCapacity > 0 to avoid divide-by-zero in size()
        other.mHead = 0;
        other.mTail = 0;
    }

    // Move assignment operator
    DynamicCircularBuffer& operator=(DynamicCircularBuffer&& other) noexcept {
        if (this != &other) {
            // Move the scoped_array (will steal heap pointer efficiently)
            mBuffer = fl::move(other.mBuffer);
            mCapacity = other.mCapacity;
            mHead = other.mHead;
            mTail = other.mTail;

            // Leave other in valid empty state (mHead == mTail means empty)
            // Keep mCapacity > 0 to avoid divide-by-zero in size()
            other.mHead = 0;
            other.mTail = 0;
        }
        return *this;
    }

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
            *dst = fl::move(mBuffer[mTail]);
        }
        mBuffer[mTail] = T();  // Properly destroy the element by assigning default value
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
            *dst = fl::move(mBuffer[mHead]);
        }
        mBuffer[mHead] = T();  // Properly destroy the element by assigning default value
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

    void clear() {
        // Properly destroy all elements by assigning default-constructed values
        while (!empty()) {
            T dummy;
            pop_front(&dummy);
        }
    }

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
