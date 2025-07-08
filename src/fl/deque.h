#pragma once


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
    fl::size mCapacity = 0;
    fl::size mSize = 0;
    fl::size mFront = 0;  // Index of the front element
    Allocator mAlloc;

    static const fl::size kInitialCapacity = 8;

    void ensure_capacity(fl::size min_capacity) {
        if (mCapacity >= min_capacity) {
            return;
        }

        fl::size new_capacity = mCapacity == 0 ? kInitialCapacity : mCapacity * 2;
        while (new_capacity < min_capacity) {
            new_capacity *= 2;
        }

        T* new_data = mAlloc.allocate(new_capacity);
        if (!new_data) {
            return; // Allocation failed
        }

        // Copy existing elements to new buffer in linear order
        for (fl::size i = 0; i < mSize; ++i) {
            fl::size old_idx = (mFront + i) % mCapacity;
            mAlloc.construct(&new_data[i], fl::move(mData[old_idx]));
            mAlloc.destroy(&mData[old_idx]);
        }

        if (mData) {
            mAlloc.deallocate(mData, mCapacity);
        }

        mData = new_data;
        mCapacity = new_capacity;
        mFront = 0; // Reset front to 0 after reallocation
    }

    fl::size get_index(fl::size logical_index) const {
        return (mFront + logical_index) % mCapacity;
    }

public:
    // Iterator implementation
    class iterator {
    private:
        deque* mDeque;
        fl::size mIndex;

    public:
        iterator(deque* dq, fl::size index) : mDeque(dq), mIndex(index) {}

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
        fl::size mIndex;

    public:
        const_iterator(const deque* dq, fl::size index) : mDeque(dq), mIndex(index) {}

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

    explicit deque(fl::size count, const T& value = T()) : deque() {
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
            for (fl::size i = 0; i < other.size(); ++i) {
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
    T& operator[](fl::size index) {
        return mData[get_index(index)];
    }

    const T& operator[](fl::size index) const {
        return mData[get_index(index)];
    }

    T& at(fl::size index) {
        if (index >= mSize) {
            // Handle bounds error - in embedded context, we'll just return the first element
            // In a real implementation, this might throw an exception
            return mData[mFront];
        }
        return mData[get_index(index)];
    }

    const T& at(fl::size index) const {
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

    fl::size size() const {
        return mSize;
    }

    fl::size capacity() const {
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
        fl::size back_index = get_index(mSize);
        mAlloc.construct(&mData[back_index], value);
        ++mSize;
    }

    void push_back(T&& value) {
        ensure_capacity(mSize + 1);
        fl::size back_index = get_index(mSize);
        mAlloc.construct(&mData[back_index], fl::move(value));
        ++mSize;
    }

    void push_front(const T& value) {
        ensure_capacity(mSize + 1);
        mFront = (mFront - 1 + mCapacity) % mCapacity;
        mAlloc.construct(&mData[mFront], value);
        ++mSize;
    }

    void push_front(T&& value) {
        ensure_capacity(mSize + 1);
        mFront = (mFront - 1 + mCapacity) % mCapacity;
        mAlloc.construct(&mData[mFront], fl::move(value));
        ++mSize;
    }

    void pop_back() {
        if (mSize > 0) {
            fl::size back_index = get_index(mSize - 1);
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

    void resize(fl::size new_size) {
        resize(new_size, T());
    }

    void resize(fl::size new_size, const T& value) {
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
            fl::size temp_capacity = mCapacity;
            fl::size temp_size = mSize;
            fl::size temp_front = mFront;
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
