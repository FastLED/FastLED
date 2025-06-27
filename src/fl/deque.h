#pragma once

#include <stddef.h>
#include "fl/stdint.h"
#include <string.h>

#include "fl/allocator.h"
#include "fl/initializer_list.h"
#include "fl/inplacenew.h"
#include "fl/move.h"
#include "fl/namespace.h"
#include "fl/type_traits.h"

namespace fl {

template <typename T, typename Allocator = fl::allocator<T>>
class deque {
private:
    T* mData = nullptr;
    size_t mCapacity = 0;
    size_t mSize = 0;
    size_t mFront = 0;  // Index of the front element
    Allocator mAlloc;

    static const size_t kInitialCapacity = 8;

    void ensure_capacity(size_t min_capacity) {
        if (mCapacity >= min_capacity) {
            return;
        }

        size_t new_capacity = mCapacity == 0 ? kInitialCapacity : mCapacity * 2;
        while (new_capacity < min_capacity) {
            new_capacity *= 2;
        }

        T* new_data = mAlloc.allocate(new_capacity);
        if (!new_data) {
            return; // Allocation failed
        }

        // Copy existing elements to new buffer in linear order
        for (size_t i = 0; i < mSize; ++i) {
            size_t old_idx = (mFront + i) % mCapacity;
            mAlloc.construct(&new_data[i], mData[old_idx]);
            mAlloc.destroy(&mData[old_idx]);
        }

        if (mData) {
            mAlloc.deallocate(mData, mCapacity);
        }

        mData = new_data;
        mCapacity = new_capacity;
        mFront = 0; // Reset front to 0 after reallocation
    }

    size_t get_index(size_t logical_index) const {
        return (mFront + logical_index) % mCapacity;
    }

public:
    // Iterator implementation
    class iterator {
    private:
        deque* mDeque;
        size_t mIndex;

    public:
        iterator(deque* dq, size_t index) : mDeque(dq), mIndex(index) {}

        T& operator*() const {
            return (*mDeque)[mIndex];
        }

        T* operator->() const {
            return &(*mDeque)[mIndex];
        }

        iterator& operator++() {
            ++mIndex;
            return *this;
        }

        iterator operator++(int) {
            iterator temp = *this;
            ++mIndex;
            return temp;
        }

        iterator& operator--() {
            --mIndex;
            return *this;
        }

        iterator operator--(int) {
            iterator temp = *this;
            --mIndex;
            return temp;
        }

        bool operator==(const iterator& other) const {
            return mDeque == other.mDeque && mIndex == other.mIndex;
        }

        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }
    };

    class const_iterator {
    private:
        const deque* mDeque;
        size_t mIndex;

    public:
        const_iterator(const deque* dq, size_t index) : mDeque(dq), mIndex(index) {}

        const T& operator*() const {
            return (*mDeque)[mIndex];
        }

        const T* operator->() const {
            return &(*mDeque)[mIndex];
        }

        const_iterator& operator++() {
            ++mIndex;
            return *this;
        }

        const_iterator operator++(int) {
            const_iterator temp = *this;
            ++mIndex;
            return temp;
        }

        const_iterator& operator--() {
            --mIndex;
            return *this;
        }

        const_iterator operator--(int) {
            const_iterator temp = *this;
            --mIndex;
            return temp;
        }

        bool operator==(const const_iterator& other) const {
            return mDeque == other.mDeque && mIndex == other.mIndex;
        }

        bool operator!=(const const_iterator& other) const {
            return !(*this == other);
        }
    };

    // Constructors
    deque() : mData(nullptr), mCapacity(0), mSize(0), mFront(0) {}

    explicit deque(size_t count, const T& value = T()) : deque() {
        resize(count, value);
    }

    deque(const deque& other) : deque() {
        *this = other;
    }

    deque(deque&& other) : deque() {
        *this = fl::move(other);
    }

    deque(fl::initializer_list<T> init) : deque() {
        for (const auto& value : init) {
            push_back(value);
        }
    }

    // Destructor
    ~deque() {
        clear();
        if (mData) {
            mAlloc.deallocate(mData, mCapacity);
        }
    }

    // Assignment operators
    deque& operator=(const deque& other) {
        if (this != &other) {
            clear();
            for (size_t i = 0; i < other.size(); ++i) {
                push_back(other[i]);
            }
        }
        return *this;
    }

    deque& operator=(deque&& other) {
        if (this != &other) {
            clear();
            if (mData) {
                mAlloc.deallocate(mData, mCapacity);
            }

            mData = other.mData;
            mCapacity = other.mCapacity;
            mSize = other.mSize;
            mFront = other.mFront;
            mAlloc = other.mAlloc;

            other.mData = nullptr;
            other.mCapacity = 0;
            other.mSize = 0;
            other.mFront = 0;
        }
        return *this;
    }

    // Element access
    T& operator[](size_t index) {
        return mData[get_index(index)];
    }

    const T& operator[](size_t index) const {
        return mData[get_index(index)];
    }

    T& at(size_t index) {
        if (index >= mSize) {
            // Handle bounds error - in embedded context, we'll just return the first element
            // In a real implementation, this might throw an exception
            return mData[mFront];
        }
        return mData[get_index(index)];
    }

    const T& at(size_t index) const {
        if (index >= mSize) {
            // Handle bounds error - in embedded context, we'll just return the first element
            return mData[mFront];
        }
        return mData[get_index(index)];
    }

    T& front() {
        return mData[mFront];
    }

    const T& front() const {
        return mData[mFront];
    }

    T& back() {
        return mData[get_index(mSize - 1)];
    }

    const T& back() const {
        return mData[get_index(mSize - 1)];
    }

    // Iterators
    iterator begin() {
        return iterator(this, 0);
    }

    const_iterator begin() const {
        return const_iterator(this, 0);
    }

    iterator end() {
        return iterator(this, mSize);
    }

    const_iterator end() const {
        return const_iterator(this, mSize);
    }

    // Capacity
    bool empty() const {
        return mSize == 0;
    }

    size_t size() const {
        return mSize;
    }

    size_t capacity() const {
        return mCapacity;
    }

    // Modifiers
    void clear() {
        while (!empty()) {
            pop_back();
        }
    }

    void push_back(const T& value) {
        ensure_capacity(mSize + 1);
        size_t back_index = get_index(mSize);
        mAlloc.construct(&mData[back_index], value);
        ++mSize;
    }

    void push_front(const T& value) {
        ensure_capacity(mSize + 1);
        mFront = (mFront - 1 + mCapacity) % mCapacity;
        mAlloc.construct(&mData[mFront], value);
        ++mSize;
    }

    void pop_back() {
        if (mSize > 0) {
            size_t back_index = get_index(mSize - 1);
            mAlloc.destroy(&mData[back_index]);
            --mSize;
        }
    }

    void pop_front() {
        if (mSize > 0) {
            mAlloc.destroy(&mData[mFront]);
            mFront = (mFront + 1) % mCapacity;
            --mSize;
        }
    }

    void resize(size_t new_size) {
        resize(new_size, T());
    }

    void resize(size_t new_size, const T& value) {
        if (new_size > mSize) {
            // Add elements
            ensure_capacity(new_size);
            while (mSize < new_size) {
                push_back(value);
            }
        } else if (new_size < mSize) {
            // Remove elements
            while (mSize > new_size) {
                pop_back();
            }
        }
        // If new_size == mSize, do nothing
    }

    void swap(deque& other) {
        if (this != &other) {
            T* temp_data = mData;
            size_t temp_capacity = mCapacity;
            size_t temp_size = mSize;
            size_t temp_front = mFront;
            Allocator temp_alloc = mAlloc;

            mData = other.mData;
            mCapacity = other.mCapacity;
            mSize = other.mSize;
            mFront = other.mFront;
            mAlloc = other.mAlloc;

            other.mData = temp_data;
            other.mCapacity = temp_capacity;
            other.mSize = temp_size;
            other.mFront = temp_front;
            other.mAlloc = temp_alloc;
        }
    }
};

// Convenience typedef for the most common use case
typedef deque<int> deque_int;
typedef deque<float> deque_float;
typedef deque<double> deque_double;

} // namespace fl
