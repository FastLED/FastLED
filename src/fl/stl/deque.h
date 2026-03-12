#pragma once

#include "fl/stl/stdint.h"

#include "fl/stl/allocator.h"
#include "fl/stl/move.h"
#include "fl/stl/iterator.h"

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
    // Iterator implementation (RandomAccessIterator)
    class iterator {
    public:
        typedef T value_type;
        typedef T& reference;
        typedef T* pointer;
        typedef fl::size difference_type;
        typedef fl::random_access_iterator_tag iterator_category;

    private:
        deque* mDeque;
        fl::size mIndex;

        friend class deque;

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

        iterator& operator+=(fl::size n) {
            mIndex += n;
            return *this;
        }

        iterator operator+(fl::size n) const {
            iterator temp = *this;
            return temp += n;
        }

        iterator& operator-=(fl::size n) {
            mIndex -= n;
            return *this;
        }

        iterator operator-(fl::size n) const {
            iterator temp = *this;
            return temp -= n;
        }

        fl::size operator-(const iterator& other) const {
            return mIndex - other.mIndex;
        }

        T& operator[](fl::size n) const {
            return (*mDeque)[mIndex + n];
        }

        bool operator==(const iterator& other) const {
            return mDeque == other.mDeque && mIndex == other.mIndex;
        }

        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }

        bool operator<(const iterator& other) const {
            return mIndex < other.mIndex;
        }

        bool operator<=(const iterator& other) const {
            return mIndex <= other.mIndex;
        }

        bool operator>(const iterator& other) const {
            return mIndex > other.mIndex;
        }

        bool operator>=(const iterator& other) const {
            return mIndex >= other.mIndex;
        }
    };

    class const_iterator {
    public:
        typedef T value_type;
        typedef const T& reference;
        typedef const T* pointer;
        typedef fl::size difference_type;
        typedef fl::random_access_iterator_tag iterator_category;

    private:
        const deque* mDeque;
        fl::size mIndex;

        friend class deque;

    public:
        const_iterator(const deque* dq, fl::size index) : mDeque(dq), mIndex(index) {}

        // Implicit conversion from iterator to const_iterator
        const_iterator(const iterator& it) : mDeque(it.mDeque), mIndex(it.mIndex) {}

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

        const_iterator& operator+=(fl::size n) {
            mIndex += n;
            return *this;
        }

        const_iterator operator+(fl::size n) const {
            const_iterator temp = *this;
            return temp += n;
        }

        const_iterator& operator-=(fl::size n) {
            mIndex -= n;
            return *this;
        }

        const_iterator operator-(fl::size n) const {
            const_iterator temp = *this;
            return temp -= n;
        }

        fl::size operator-(const const_iterator& other) const {
            return mIndex - other.mIndex;
        }

        const T& operator[](fl::size n) const {
            return (*mDeque)[mIndex + n];
        }

        bool operator==(const const_iterator& other) const {
            return mDeque == other.mDeque && mIndex == other.mIndex;
        }

        bool operator!=(const const_iterator& other) const {
            return !(*this == other);
        }

        bool operator<(const const_iterator& other) const {
            return mIndex < other.mIndex;
        }

        bool operator<=(const const_iterator& other) const {
            return mIndex <= other.mIndex;
        }

        bool operator>(const const_iterator& other) const {
            return mIndex > other.mIndex;
        }

        bool operator>=(const const_iterator& other) const {
            return mIndex >= other.mIndex;
        }
    };

    typedef Allocator allocator_type;
    typedef fl::reverse_iterator<iterator> reverse_iterator;
    typedef fl::reverse_iterator<const_iterator> const_reverse_iterator;

    // Constructors
    deque() : mData(nullptr), mCapacity(0), mSize(0), mFront(0) {}

    explicit deque(const Allocator& alloc) : mData(nullptr), mCapacity(0), mSize(0), mFront(0), mAlloc(alloc) {}

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

    // Reverse iterators
    reverse_iterator rbegin() {
        return reverse_iterator(end());
    }

    const_reverse_iterator rbegin() const {
        return const_reverse_iterator(end());
    }

    reverse_iterator rend() {
        return reverse_iterator(begin());
    }

    const_reverse_iterator rend() const {
        return const_reverse_iterator(begin());
    }

    // Explicit const iterator accessors
    const_iterator cbegin() const {
        return const_iterator(this, 0);
    }

    const_iterator cend() const {
        return const_iterator(this, mSize);
    }

    const_reverse_iterator crbegin() const {
        return const_reverse_iterator(cend());
    }

    const_reverse_iterator crend() const {
        return const_reverse_iterator(cbegin());
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

    fl::size max_size() const {
        // Return a reasonable max size (limited by size_t or memory)
        return static_cast<fl::size>(-1) / sizeof(T);
    }

    void reserve(fl::size new_capacity) {
        if (new_capacity > mCapacity) {
            ensure_capacity(new_capacity);
        }
    }

    void shrink_to_fit() {
        if (mSize < mCapacity) {
            if (mSize == 0) {
                if (mData) {
                    mAlloc.deallocate(mData, mCapacity);
                }
                mData = nullptr;
                mCapacity = 0;
                mFront = 0;
            } else {
                // Reallocate to exact size
                T* new_data = mAlloc.allocate(mSize);
                if (!new_data) {
                    return; // Allocation failed
                }

                // Copy elements to new buffer
                for (fl::size i = 0; i < mSize; ++i) {
                    fl::size old_idx = (mFront + i) % mCapacity;
                    mAlloc.construct(&new_data[i], fl::move(mData[old_idx]));
                    mAlloc.destroy(&mData[old_idx]);
                }

                if (mData) {
                    mAlloc.deallocate(mData, mCapacity);
                }

                mData = new_data;
                mCapacity = mSize;
                mFront = 0;
            }
        }
    }

    allocator_type get_allocator() const {
        return mAlloc;
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

    // Insert operations
    iterator insert(const_iterator pos, const T& value) {
        fl::size index = pos.mIndex;
        ensure_capacity(mSize + 1);

        // Shift elements from pos to end one position to the right
        for (fl::size i = mSize; i > index; --i) {
            fl::size from_idx = get_index(i - 1);
            fl::size to_idx = get_index(i);
            mAlloc.construct(&mData[to_idx], fl::move(mData[from_idx]));
            mAlloc.destroy(&mData[from_idx]);
        }

        // Insert new element
        fl::size insert_idx = get_index(index);
        mAlloc.construct(&mData[insert_idx], value);
        ++mSize;

        return iterator(this, index);
    }

    iterator insert(const_iterator pos, T&& value) {
        fl::size index = pos.mIndex;
        ensure_capacity(mSize + 1);

        // Shift elements from pos to end one position to the right
        for (fl::size i = mSize; i > index; --i) {
            fl::size from_idx = get_index(i - 1);
            fl::size to_idx = get_index(i);
            mAlloc.construct(&mData[to_idx], fl::move(mData[from_idx]));
            mAlloc.destroy(&mData[from_idx]);
        }

        // Insert new element
        fl::size insert_idx = get_index(index);
        mAlloc.construct(&mData[insert_idx], fl::move(value));
        ++mSize;

        return iterator(this, index);
    }

    iterator insert(const_iterator pos, fl::size count, const T& value) {
        fl::size index = pos.mIndex;
        ensure_capacity(mSize + count);

        // Shift elements from pos to end 'count' positions to the right
        for (fl::size i = mSize + count - 1; i >= index + count; --i) {
            fl::size from_idx = get_index(i - count);
            fl::size to_idx = get_index(i);
            mAlloc.construct(&mData[to_idx], fl::move(mData[from_idx]));
            mAlloc.destroy(&mData[from_idx]);
        }

        // Insert new elements
        for (fl::size i = 0; i < count; ++i) {
            fl::size insert_idx = get_index(index + i);
            mAlloc.construct(&mData[insert_idx], value);
        }
        mSize += count;

        return iterator(this, index);
    }

    // Erase operations
    iterator erase(const_iterator pos) {
        if (pos == end()) return end();

        fl::size index = pos.mIndex;

        // Destroy element at pos
        fl::size erase_idx = get_index(index);
        mAlloc.destroy(&mData[erase_idx]);

        // Shift elements from pos+1 to end one position to the left
        for (fl::size i = index; i < mSize - 1; ++i) {
            fl::size from_idx = get_index(i + 1);
            fl::size to_idx = get_index(i);
            mAlloc.construct(&mData[to_idx], fl::move(mData[from_idx]));
            mAlloc.destroy(&mData[from_idx]);
        }

        --mSize;
        return iterator(this, index);
    }

    iterator erase(const_iterator first, const_iterator last) {
        if (first == last) return iterator(this, first.mIndex);

        fl::size start_idx = first.mIndex;
        fl::size count = last.mIndex - first.mIndex;

        // Destroy elements in range
        for (fl::size i = 0; i < count; ++i) {
            fl::size destroy_idx = get_index(start_idx + i);
            mAlloc.destroy(&mData[destroy_idx]);
        }

        // Shift remaining elements left
        for (fl::size i = start_idx; i < mSize - count; ++i) {
            fl::size from_idx = get_index(i + count);
            fl::size to_idx = get_index(i);
            mAlloc.construct(&mData[to_idx], fl::move(mData[from_idx]));
            mAlloc.destroy(&mData[from_idx]);
        }

        mSize -= count;
        return iterator(this, start_idx);
    }

    // Emplace operations
    template<typename... Args>
    iterator emplace(const_iterator pos, Args&&... args) {
        fl::size index = pos.mIndex;
        ensure_capacity(mSize + 1);

        // Shift elements from pos to end one position to the right
        for (fl::size i = mSize; i > index; --i) {
            fl::size from_idx = get_index(i - 1);
            fl::size to_idx = get_index(i);
            mAlloc.construct(&mData[to_idx], fl::move(mData[from_idx]));
            mAlloc.destroy(&mData[from_idx]);
        }

        // Construct new element in place
        fl::size emplace_idx = get_index(index);
        mAlloc.construct(&mData[emplace_idx], fl::forward<Args>(args)...);
        ++mSize;

        return iterator(this, index);
    }

    template<typename... Args>
    T& emplace_back(Args&&... args) {
        ensure_capacity(mSize + 1);
        fl::size back_index = get_index(mSize);
        mAlloc.construct(&mData[back_index], fl::forward<Args>(args)...);
        ++mSize;
        return mData[back_index];
    }

    template<typename... Args>
    T& emplace_front(Args&&... args) {
        ensure_capacity(mSize + 1);
        mFront = (mFront - 1 + mCapacity) % mCapacity;
        mAlloc.construct(&mData[mFront], fl::forward<Args>(args)...);
        ++mSize;
        return mData[mFront];
    }

    // Assign operations
    void assign(fl::size count, const T& value) {
        clear();
        ensure_capacity(count);
        for (fl::size i = 0; i < count; ++i) {
            push_back(value);
        }
    }

    // Comparison operators
    bool operator==(const deque& other) const {
        if (mSize != other.mSize) {
            return false;
        }
        for (fl::size i = 0; i < mSize; ++i) {
            if ((*this)[i] != other[i]) {
                return false;
            }
        }
        return true;
    }

    bool operator!=(const deque& other) const {
        return !(*this == other);
    }

    bool operator<(const deque& other) const {
        fl::size min_size = mSize < other.mSize ? mSize : other.mSize;
        for (fl::size i = 0; i < min_size; ++i) {
            if ((*this)[i] < other[i]) {
                return true;
            }
            if ((*this)[i] > other[i]) {
                return false;
            }
        }
        return mSize < other.mSize;
    }

    bool operator<=(const deque& other) const {
        return *this < other || *this == other;
    }

    bool operator>(const deque& other) const {
        return other < *this;
    }

    bool operator>=(const deque& other) const {
        return *this > other || *this == other;
    }
};

// Convenience typedef for the most common use case
typedef deque<int> deque_int;
typedef deque<float> deque_float;
typedef deque<double> deque_double;

template <typename T, typename Allocator>
void fl_swap(deque<T, Allocator>& lhs, deque<T, Allocator>& rhs) {
    lhs.swap(rhs);
}

} // namespace fl
