#pragma once

#include <stdint.h>
#include "namespace.h"
#include "crgb.h"
#include "ptr.h"

FASTLED_NAMESPACE_BEGIN

template <typename T>
class CircularBuffer {
public:
    CircularBuffer(size_t capacity, T* buffer = nullptr) 
        : mCapacity(capacity), mSize(0), mHead(0), mTail(0) {
        if (buffer) {
            mBuffer.reset(buffer);
        } else {
            mBuffer.reset(new T[capacity]);
        }
    }

    // Deleted copy constructor and assignment operator to prevent copying
    CircularBuffer(const CircularBuffer&) = delete;
    CircularBuffer& operator=(const CircularBuffer&) = delete;

    void push_back(const T& value) {
        mBuffer[mHead] = value;
        mHead = (mHead + 1) % mCapacity;
        if (mSize < mCapacity) {
            ++mSize;
        } else {
            mTail = (mTail + 1) % mCapacity;
        }
    }

    T pop_front() {
        if (empty()) {
            return T(); // Return default-constructed T if empty
        }
        T value = mBuffer[mTail];
        mTail = (mTail + 1) % mCapacity;
        --mSize;
        return value;
    }

    T& front() {
        return mBuffer[mTail];
    }

    const T& front() const {
        return mBuffer[mTail];
    }

    T& back() {
        return mBuffer[(mHead - 1 + mCapacity) % mCapacity];
    }

    const T& back() const {
        return mBuffer[(mHead - 1 + mCapacity) % mCapacity];
    }

    T& operator[](size_t index) {
        return mBuffer[(mTail + index) % mCapacity];
    }

    const T& operator[](size_t index) const {
        return mBuffer[(mTail + index) % mCapacity];
    }

    size_t size() const {
        return mSize;
    }

    size_t capacity() const {
        return mCapacity;
    }

    bool empty() const {
        return mSize == 0;
    }

    bool full() const {
        return mSize == mCapacity;
    }

    void clear() {
        mHead = mTail = mSize = 0;
    }

private:
    scoped_array<T> mBuffer;
    size_t mCapacity;
    size_t mSize;
    size_t mHead;
    size_t mTail;
};

FASTLED_NAMESPACE_END

