#pragma once

#include <stddef.h>   // For size_t
#include <stdint.h>   // For standard integer types
#include "fl/namespace.h"
#include "fl/ptr.h"      // Assuming this provides `scoped_array` or similar
#include "fl/math_macros.h"

namespace fl {

template <typename T>
class CircularBuffer {
public:
    CircularBuffer(size_t capacity)
        : mCapacity(capacity + 1), mHead(0), mTail(0) { // Extra space for distinguishing full/empty
        mBuffer.reset(new T[mCapacity]);
    }

    CircularBuffer(const CircularBuffer&) = delete;
    CircularBuffer& operator=(const CircularBuffer&) = delete;

    bool push_back(const T& value) {
        if (full()) {
            mTail = increment(mTail); // Overwrite the oldest element
        }
        mBuffer[mHead] = value;
        mHead = increment(mHead);
        return true;
    }

    bool pop_front(T* dst = nullptr) {
        if (empty()) {
            return false;
        }
        if (dst) {
            *dst = mBuffer[mTail];
        }
        mTail = increment(mTail);
        return true;
    }

    bool push_front(const T& value) {
        if (full()) {
            mHead = decrement(mHead); // Overwrite the oldest element
        }
        mTail = decrement(mTail);
        mBuffer[mTail] = value;
        return true;
    }

    bool pop_back(T* dst = nullptr) {
        if (empty()) {
            return false;
        }
        mHead = decrement(mHead);
        if (dst) {
            *dst = mBuffer[mHead];
        }
        return true;
    }

    T& front() {
        return mBuffer[mTail];
    }

    const T& front() const {
        return mBuffer[mTail];
    }

    T& back() {
        return mBuffer[(mHead + mCapacity - 1) % mCapacity];
    }

    const T& back() const {
        return mBuffer[(mHead + mCapacity - 1) % mCapacity];
    }

    T& operator[](size_t index) {
        return mBuffer[(mTail + index) % mCapacity];
    }

    const T& operator[](size_t index) const {
        return mBuffer[(mTail + index) % mCapacity];
    }

    size_t size() const {
        return (mHead + mCapacity - mTail) % mCapacity;
    }

    size_t capacity() const {
        return mCapacity - 1;
    }

    bool empty() const {
        return mHead == mTail;
    }

    bool full() const {
        return increment(mHead) == mTail;
    }

    void clear() {
        mHead = mTail = 0;
    }

private:
    size_t increment(size_t index) const {
        return (index + 1) % mCapacity;
    }

    size_t decrement(size_t index) const {
        return (index + mCapacity - 1) % mCapacity;
    }

    fl::scoped_array<T> mBuffer;
    size_t mCapacity;
    size_t mHead;
    size_t mTail;
};


}
