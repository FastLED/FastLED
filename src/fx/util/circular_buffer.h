#pragma once

#include <stddef.h>   // For size_t
#include <stdint.h>   // For standard integer types
#include "namespace.h"
#include "ptr.h"      // Assuming this provides `scoped_array` or similar

FASTLED_NAMESPACE_BEGIN

template <typename T>
class CircularBuffer {
public:
    // Constructor
    CircularBuffer(size_t capacity, T* buffer = nullptr) 
        : mCapacity(capacity), mSize(0), mHead(0), mTail(0) {
        // Ensure capacity is greater than zero
        if (capacity == 0) {
            // Handle zero capacity appropriately (e.g., set to 1 or return)
            capacity = 1;
            mCapacity = capacity;
        }

        if (buffer) {
            // Assume ownership of the provided buffer
            mBuffer.reset(buffer);
        } else {
            mBuffer.reset(new T[capacity]);
        }
    }

    // Deleted copy constructor and assignment operator to prevent copying
    CircularBuffer(const CircularBuffer&) = delete;
    CircularBuffer& operator=(const CircularBuffer&) = delete;

    // Push value to the back of the buffer
    void push_back(const T& value) {
        mBuffer[mHead] = value;
        mHead = increment(mHead);
        if (mSize < mCapacity) {
            ++mSize;
        } else {
            // Overwrite the oldest data
            mTail = increment(mTail);
        }
    }

    // Pop value from the front of the buffer
    T pop_front() {
        if (empty()) {
            // Handle underflow appropriately (e.g., return default value)
            return T();
        }
        T value = mBuffer[mTail];
        mTail = increment(mTail);
        --mSize;
        return value;
    }

    // Access the front element
    T& front() {
        return mBuffer[mTail];
    }

    const T& front() const {
        return mBuffer[mTail];
    }

    // Access the back element
    T& back() {
        return mBuffer[(mHead + mCapacity - 1) % mCapacity];
    }

    const T& back() const {
        return mBuffer[(mHead + mCapacity - 1) % mCapacity];
    }

    // Random access operator
    T& operator[](size_t index) {
        // No bounds checking to avoid overhead
        return mBuffer[(mTail + index) % mCapacity];
    }

    const T& operator[](size_t index) const {
        return mBuffer[(mTail + index) % mCapacity];
    }

    // Get the current size
    size_t size() const {
        return mSize;
    }

    // Get the capacity
    size_t capacity() const {
        return mCapacity;
    }

    // Check if the buffer is empty
    bool empty() const {
        return mSize == 0;
    }

    // Check if the buffer is full
    bool full() const {
        return mSize == mCapacity;
    }

    // Clear the buffer
    void clear() {
        mHead = mTail = mSize = 0;
    }

private:
    // Helper function to increment an index with wrap-around
    size_t increment(size_t index) const {
        return (index + 1) % mCapacity;
    }

    scoped_array<T> mBuffer;  // Assuming `scoped_array` is defined in "ptr.h"
    size_t mCapacity;
    size_t mSize;
    size_t mHead;
    size_t mTail;
};

FASTLED_NAMESPACE_END
