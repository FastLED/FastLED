#pragma once

#include <stdint.h>

template<typename T>
class Slice {
public:
    Slice(T* data, size_t size) : mData(data), mSize(size) {}
    Slice() : mData(nullptr), mSize(0) {}
    Slice(const Slice& other) = default;
    Slice& operator=(const Slice& other) = default;

    T& operator[](size_t index) {
        // No bounds checking in embedded environment
        return mData[index];
    }

    const T& operator[](size_t index) const {
        // No bounds checking in embedded environment
        return mData[index];
    }

    T* begin() const {
        return mData;
    }

    T* end() const {
        return mData + mSize;
    }

    size_t length() const {
        return mSize;
    }

    const T* data() const {
        return mData;
    }

    T* data() {
        return mData;
    }

    size_t size() const {
        return mSize;
    }

    Slice<T> slice(size_t start, size_t end) const {
        // No bounds checking in embedded environment
        return Slice<T>(mData + start, end - start);
    }

    Slice<T> slice(size_t start) const {
        // No bounds checking in embedded environment
        return Slice<T>(mData + start, mSize - start);
    }

private:
    T* mData;
    size_t mSize;
};
