#pragma once

#include <stdint.h>
#include <stddef.h>

#include "inplacenew.h"
#include "fl/namespace.h"
#include "fl/scoped_ptr.h"
#include "fl/insert_result.h"

namespace fl {


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

    template<size_t M>
    FixedVector(const T (&values)[M]) : current_size(M) {
        static_assert(M <= N, "Too many elements for FixedVector");
        assign(values, M);
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
        if (index >= current_size) {
            const T* out = nullptr;
            return *out;  // Cause a nullptr dereference
        }
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

    void assign(const_iterator begin, const_iterator end) {
        clear();
        for (const_iterator it = begin; it != end; ++it) {
            push_back(*it);
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
    fl::scoped_array<T> mArray;
    
    size_t mCapacity = 0;
    size_t mSize = 0;

    public:
    typedef T* iterator;
    typedef const T* const_iterator;

    // Constructor
    HeapVector(size_t size = 0, const T& value = T()): mCapacity(size) { 
        mArray.reset(new T[mCapacity]);
        for (size_t i = 0; i < size; ++i) {
            mArray[i] = value;
        }
        mSize = size;
    }
    HeapVector(const HeapVector<T>& other): mSize(other.size()) {
        assign(other.begin(), other.end());
    }
    HeapVector& operator=(const HeapVector<T>& other) { // cppcheck-suppress operatorEqVarError
        if (this != &other) {
            assign(other.begin(), other.end());
        }
        return *this;
    }

    // Destructor
    ~HeapVector() {
        clear();
    }

    void ensure_size(size_t n) {
        if (n > mCapacity) {
            size_t new_capacity = (3*mCapacity) / 2;
            if (new_capacity < n) {
                new_capacity = n;
            }
            fl::scoped_array<T> new_array(new T[new_capacity]);
            for (size_t i = 0; i < mSize; ++i) {
                new_array[i] = mArray[i];
            }
            // mArray = std::move(new_array);
            mArray.reset();
            mArray.reset(new_array.release());
            mCapacity = new_capacity;
        }
    }

    void reserve(size_t n) {
        if (n > mCapacity) {
            ensure_size(n);
        }
    }

    void resize(size_t n) {
        if (mSize == n) {
            return;
        }
        HeapVector<T> temp(n);
        for (size_t i = 0; i < n && i < mSize; ++i) {
            temp.mArray[i] = mArray[i];
        }
        swap(temp);
    }

    void resize(size_t n, const T& value) {
        mArray.reset();
        mArray.reset(new T[n]);
        for (size_t i = 0; i < n; ++i) {
            mArray[i] = value;
        }
        mCapacity = n;
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
        ensure_size(mSize + 1);
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

    void swap(HeapVector<T>& other) {
        T* temp = mArray.release();
        size_t temp_size = mSize;
        size_t temp_capacity = mCapacity;
        T* temp2 = other.mArray.release();
        mArray.reset(temp2);
        other.mArray.reset(temp);
        mSize = other.mSize;
        mCapacity = other.mCapacity;
        other.mSize = temp_size;
        other.mCapacity = temp_capacity;
    }

    void swap(iterator a, iterator b) {
        T temp = *a;
        *a = *b;
        *b = temp;
    }

    bool full() const {
        return mSize >= mCapacity;
    }

    bool insert(iterator pos, const T& value) {
        // TODO: Introduce mMaxSize (and move it from SortedVector to here)
        // push back and swap into place.
        size_t target_idx = pos - begin();
        push_back(value);
        auto last = end() - 1;
        for (size_t curr_idx = last - begin(); curr_idx > target_idx; --curr_idx) {
            auto first = begin() + curr_idx - 1;
            auto second = begin() + curr_idx;
            swap(first, second);
        }
        return true;
    }

    void assign(const T* values, size_t count) {
        assign(values, values + count);
    }

    void assign(const_iterator begin, const_iterator end) {
        clear();
        reserve(end - begin);
        for (const_iterator it = begin; it != end; ++it) {
            push_back(*it);
        }
    }

    T* data() {
        return mArray.get();
    }

    const T* data() const {
        return mArray.get();
    }

    bool operator==(const HeapVector<T>& other) const {
        if (size() != other.size()) {
            return false;
        }
        for (size_t i = 0; i < size(); ++i) {
            if (mArray[i] != other.mArray[i]) {
                return false;
            }
        }
        return true;
    }

    bool operator!=(const HeapVector<T>& other) const {
        return !(*this == other);
    }
};


template <typename T, typename LessThan>
class SortedHeapVector {
private:
    HeapVector<T> mArray;
    LessThan mLess;
    size_t mMaxSize = size_t(-1);

public:
    typedef typename HeapVector<T>::iterator iterator;
    typedef typename HeapVector<T>::const_iterator const_iterator;

    SortedHeapVector(LessThan less=LessThan()): mLess(less) {}

    void setMaxSize(size_t n) {
        if (mMaxSize == n) {
            return;
        }
        mMaxSize = n;
        const bool needs_adjustment = mArray.size() > mMaxSize;
        if (needs_adjustment) {
            mArray.resize(n);
        } else {
            mArray.reserve(n);
        }
    }

    ~SortedHeapVector() {
        mArray.clear();
    }

    void reserve(size_t n) {
        mArray.reserve(n);
    }

    // Insert while maintaining sort order
    bool insert(const T& value, InsertResult* result = nullptr) {
        // Find insertion point using binary search
        iterator pos = lower_bound(value);
        if (pos != end() && !mLess(value, *pos) && !mLess(*pos, value)) {
            // return false; // Already inserted.
            if (result) {
                // *result = kExists;
                *result = InsertResult::kExists;
            }
            
            return false;
        }
        if (mArray.size() >= mMaxSize) {
            // return false;  // Too full
            if (result) {
                *result = InsertResult::kMaxSize;
            }
            return false;
        }
        mArray.insert(pos, value);
        if (result) {
            *result = kInserted;
        }

        return true;
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

    void swap(SortedHeapVector& other) {
        mArray.swap(other.mArray);
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
    bool full() const {
        if (mArray.size() >= mMaxSize) {
            return true;
        }
        return mArray.full();
    }

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


}  // namespace fl
