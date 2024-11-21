#pragma once

#include <stdint.h>
#include <stddef.h>

#include "inplacenew.h"
#include "namespace.h"
#include "scoped_ptr.h"

FASTLED_NAMESPACE_BEGIN


// A fixed sized vector. The user is responsible for making sure that the
// inserts do not exceed the capacity of the vector, otherwise they will fail.
// Because of this limitation, this vector is not a drop in replacement for
// std::vector.
template<typename T, size_t N>
class FixedVector {
private:
    union {
        char mRaw[N * sizeof(T)];
        T mData[N];
    };
    size_t current_size = 0;

public:
    typedef T* iterator;
    typedef const T* const_iterator;
    // Constructor
    constexpr FixedVector() : current_size(0) {}

    FixedVector(const T (&values)[N]) : current_size(N) {
        assign(values, N);
    }

    // Destructor
    ~FixedVector() {
        clear();
    }

    // Array subscript operator
    T& operator[](size_t index) {
        return mData[index];
    }

    // Const array subscript operator
    const T& operator[](size_t index) const {
        return mData[index];
    }

    // Get the current size of the vector
    constexpr size_t size() const {
        return current_size;
    }

    constexpr bool empty() const {
        return current_size == 0;
    }

    // Get the capacity of the vector
    constexpr size_t capacity() const {
        return N;
    }

    // Add an element to the end of the vector
    void push_back(const T& value) {
        if (current_size < N) {
            void* mem = &mData[current_size];
            new (mem) T(value);
            ++current_size;
        }
    }

    void assign(const T* values, size_t count) {
        clear();
        for (size_t i = 0; i < count; ++i) {
            push_back(values[i]);
        }
    }

    // Remove the last element from the vector
    void pop_back() {
        if (current_size > 0) {
            --current_size;
            mData[current_size].~T();
        }
    }

    // Clear the vector
    void clear() {
        while (current_size > 0) {
            pop_back();
        }
    }

    // Erase the element at the given iterator position
    iterator erase(iterator pos) {
        if (pos != end()) {
            pos->~T();
            // shift all elements to the left
            for (iterator p = pos; p != end() - 1; ++p) {
                new (p) T(*(p + 1)); // Use copy constructor instead of std::move
                (p + 1)->~T();
            }
            --current_size;
        }
        return pos;
    }

    iterator erase(const T& value) {
        iterator it = find(value);
        if (it != end()) {
            erase(it);
        }
        return it;
    }

    iterator find(const T& value) {
        for (iterator it = begin(); it != end(); ++it) {
            if (*it == value) {
                return it;
            }
        }
        return end();
    }

    template<typename Predicate>
    iterator find_if(Predicate pred) {
        for (iterator it = begin(); it != end(); ++it) {
            if (pred(*it)) {
                return it;
            }
        }
        return end();
    }

    bool insert(iterator pos, const T& value) {
        if (current_size < N) {
            // shift all elements to the right
            for (iterator p = end(); p != pos; --p) {
                new (p) T(*(p - 1)); // Use copy constructor instead of std::move
                (p - 1)->~T();
            }
            new (pos) T(value);
            ++current_size;
            return true;
        }
        return false;
    }

    const_iterator find(const T& value) const {
        for (const_iterator it = begin(); it != end(); ++it) {
            if (*it == value) {
                return it;
            }
        }
        return end();
    }

    iterator data() {
        return begin();
    }

    const_iterator data() const {
        return begin();
    }

    bool has(const T& value) const {
        return find(value) != end();
    }

    // Access to first and last elements
    T& front() {
        return mData[0];
    }

    const T& front() const {
        return mData[0];
    }

    T& back() {
        return mData[current_size - 1];
    }

    const T& back() const {
        return mData[current_size - 1];
    }

    // Iterator support
    iterator begin() { return &mData[0]; }
    const_iterator begin() const { return &mData[0]; }
    iterator end() { return &mData[current_size]; }
    const_iterator end() const { return &mData[current_size]; }
};


template<typename T>
class HeapVector {
private:
    scoped_array<T> mArray;
    
    size_t mCapacity = 0;
    size_t mSize = 0;

    public:
    typedef T* iterator;
    typedef const T* const_iterator;

    // Constructor
    HeapVector(size_t capacity) : mCapacity(capacity) {
        mArray.reset(new T[mCapacity]);
    }

    // Destructor
    ~HeapVector() {
        clear();
    }

    // Array access operators
    T& operator[](size_t index) {
        return mArray[index];
    }

    const T& operator[](size_t index) const {
        return mArray[index];
    }

    // Capacity and size methods
    size_t size() const {
        return mSize;
    }

    bool empty() const {
        return mSize == 0;
    }

    size_t capacity() const {
        return mCapacity;
    }

    // Element addition/removal
    void push_back(const T& value) {
        if (mSize < mCapacity) {
            mArray[mSize] = value;
            ++mSize;
        }
    }

    void pop_back() {
        if (mSize > 0) {
            --mSize;
            mArray[mSize] = T();
        }
    }

    void clear() {
        while (mSize > 0) {
            pop_back();
        }
    }

    // Iterator methods
    iterator begin() { return &mArray[0]; }
    const_iterator begin() const { return &mArray[0]; }
    iterator end() { return &mArray[mSize]; }
    const_iterator end() const { return &mArray[mSize]; }

    // Element access
    T& front() {
        return mArray[0];
    }

    const T& front() const {
        return mArray[0];
    }

    T& back() {
        return mArray[mSize - 1];
    }

    const T& back() const {
        return mArray[mSize - 1];
    }

    // Search and modification
    iterator find(const T& value) {
        for (iterator it = begin(); it != end(); ++it) {
            if (*it == value) {
                return it;
            }
        }
        return end();
    }

    const_iterator find(const T& value) const {
        for (const_iterator it = begin(); it != end(); ++it) {
            if (*it == value) {
                return it;
            }
        }
        return end();
    }

    template<typename Predicate>
    iterator find_if(Predicate pred) {
        for (iterator it = begin(); it != end(); ++it) {
            if (pred(*it)) {
                return it;
            }
        }
        return end();
    }

    bool has(const T& value) const {
        return find(value) != end();
    }

    bool erase(iterator pos, T* out_value = nullptr) {
        if (pos == end() || empty()) {
            return false;
        }
        if (out_value) {
            *out_value = *pos;
        }
        while (pos != end() - 1) {
            *pos = *(pos + 1);
            ++pos;
        }
        back() = T();
        --mSize;
        return true;
    }

    void erase(const T& value) {
        iterator it = find(value);
        if (it != end()) {
            erase(it);
        }
    }

    void swap(iterator a, iterator b) {
        T temp = *a;
        *a = *b;
        *b = temp;
    }

    bool full() const {
        return mSize == mCapacity;
    }

    bool insert(iterator pos, const T& value) {
        if (full()) {
            return false;
        }
        // push back and swap into place.
        push_back(value);
        auto curr = end() - 1;
        while (curr != pos) {
            swap(curr, (curr - 1));
            --curr;
        }
        return true;
    }

    void assign(const T* values, size_t count) {
        clear();
        for (size_t i = 0; i < count && i < mCapacity; ++i) {
            push_back(values[i]);
        }
    }

    T* data() {
        return mArray.get();
    }

    const T* data() const {
        return mArray.get();
    }
};


template <typename T, typename LessThan>
class SortedHeapVector {
private:
    HeapVector<T> mArray;
    LessThan mLess;

public:
    typedef typename HeapVector<T>::iterator iterator;
    typedef typename HeapVector<T>::const_iterator const_iterator;

    SortedHeapVector(size_t capacity, LessThan less=LessThan()) : mArray(capacity), mLess(less) {}

    ~SortedHeapVector() {
        mArray.clear();
    }

    // Insert while maintaining sort order
    bool insert(const T& value) {
        if (mArray.size() >= mArray.capacity()) {
            return false;
        }

        // Find insertion point using binary search
        iterator pos = lower_bound(value);
        return mArray.insert(pos, value);
    }

    // Find the first position where we should insert value to maintain sort order
    iterator lower_bound(const T& value) {
        iterator first = mArray.begin();
        iterator last = mArray.end();
        
        while (first != last) {
            iterator mid = first + (last - first) / 2;
            
            if (mLess(*mid, value)) {
                first = mid + 1;
            } else {
                last = mid;
            }
        }
        return first;
    }

    const_iterator lower_bound(const T& value) const {
        return const_cast<SortedHeapVector*>(this)->lower_bound(value);
    }

    // Lookup operations
    iterator find(const T& value) {
        iterator pos = lower_bound(value);
        if (pos != end() && !mLess(value, *pos) && !mLess(*pos, value)) {
            return pos;
        }
        return end();
    }

    const_iterator find(const T& value) const {
        return const_cast<SortedHeapVector*>(this)->find(value);
    }

    bool has(const T& value) const {
        return find(value) != end();
    }

    // Removal operations
    bool erase(const T& value) {
        iterator it = find(value);
        if (it != end()) {
            return mArray.erase(it);
        }
        return false;
    }

    bool erase(iterator pos) {
        return mArray.erase(pos);
    }

    // Basic container operations
    size_t size() const { return mArray.size(); }
    bool empty() const { return mArray.empty(); }
    size_t capacity() const { return mArray.capacity(); }
    void clear() { mArray.clear(); }
    bool full() const { return mArray.full(); }

    // Element access
    T& operator[](size_t index) { return mArray[index]; }
    const T& operator[](size_t index) const { return mArray[index]; }
    
    T& front() { return mArray.front(); }
    const T& front() const { return mArray.front(); }
    
    T& back() { return mArray.back(); }
    const T& back() const { return mArray.back(); }

    // Iterators
    iterator begin() { return mArray.begin(); }
    const_iterator begin() const { return mArray.begin(); }
    iterator end() { return mArray.end(); }
    const_iterator end() const { return mArray.end(); }

    // Raw data access
    T* data() { return mArray.data(); }
    const T* data() const { return mArray.data(); }
};


FASTLED_NAMESPACE_END
