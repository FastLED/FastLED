#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "fl/functional.h"
#include "fl/insert_result.h"
#include "fl/math_macros.h"
#include "fl/namespace.h"
#include "fl/scoped_ptr.h"
#include "fl/type_traits.h"
#include "inplacenew.h"

namespace fl {

// Aligned memory block for inlined data structures.
template <typename T, size_t N> struct InlinedMemoryBlock {
    // using MemoryType = uinptr_t;
    typedef uintptr_t MemoryType;
    enum {
        kTotalBytes = N * sizeof(T),
        kAlign = sizeof(MemoryType),
        kExtraSize =
            (kTotalBytes % kAlign) ? (kAlign - (kTotalBytes % kAlign)) : 0,
        kBlockSize = kTotalBytes / sizeof(MemoryType) + kExtraSize,
    };

    InlinedMemoryBlock() {
        memset(mMemoryBlock, 0, sizeof(mMemoryBlock));
#ifdef FASTLED_TESTING
        __data = memory();
#endif
    }

    InlinedMemoryBlock(const InlinedMemoryBlock &other) = default;
    InlinedMemoryBlock(InlinedMemoryBlock &&other) = default;

    // uint32_t mRaw[N * sizeof(T)/sizeof(MemoryType) + kExtraSize];
    // align this to the size of MemoryType.
    // uint32_t mMemoryBlock[kTotalSize] = {0};
    MemoryType mMemoryBlock[kBlockSize];

    T *memory() {
        MemoryType *begin = &mMemoryBlock[0];
        uintptr_t shift_up =
            reinterpret_cast<uintptr_t>(begin) & (sizeof(MemoryType) - 1);
        MemoryType *raw = begin + shift_up;
        return reinterpret_cast<T *>(raw);
    }

    const T *memory() const {
        const MemoryType *begin = &mMemoryBlock[0];
        const uintptr_t shift_up =
            reinterpret_cast<uintptr_t>(begin) & (sizeof(MemoryType) - 1);
        const MemoryType *raw = begin + shift_up;
        return reinterpret_cast<const T *>(raw);
    }

#ifdef FASTLED_TESTING
    T *__data = nullptr;
#endif
};

// A fixed sized vector. The user is responsible for making sure that the
// inserts do not exceed the capacity of the vector, otherwise they will fail.
// Because of this limitation, this vector is not a drop in replacement for
// std::vector. However it used for vector_inlined<T, N> which allows spill over
// to a heap vector when size > N.
template <typename T, size_t N> class FixedVector {
  private:
    InlinedMemoryBlock<T, N> mMemoryBlock;

    T *memory() { return mMemoryBlock.memory(); }

    const T *memory() const { return mMemoryBlock.memory(); }

  public:
    typedef T *iterator;
    typedef const T *const_iterator;
    // Constructor
    constexpr FixedVector() : current_size(0) {}

    FixedVector(const T (&values)[N]) : current_size(N) {
        assign_array(values, N);
    }

    FixedVector(FixedVector &&other) {
        fl::swap(*this, other);
        other.clear();
    }

    FixedVector(const FixedVector &other) : current_size(other.current_size) {
        assign_array(other.memory(), other.current_size);
    }

    template <size_t M> FixedVector(const T (&values)[M]) : current_size(M) {
        static_assert(M <= N, "Too many elements for FixedVector");
        assign_array(values, M);
    }

    FixedVector &operator=(const FixedVector &other) {
        if (this != &other) {
            assign_array(other.memory(), other.current_size);
        }
        return *this;
    }

    // Destructor
    ~FixedVector() { clear(); }

    // Array subscript operator
    T &operator[](size_t index) { return memory()[index]; }

    // Const array subscript operator
    const T &operator[](size_t index) const {
        if (index >= current_size) {
            const T *out = nullptr;
            return *out; // Cause a nullptr dereference
        }
        return memory()[index];
    }

    void resize(size_t n) {
        while (current_size < n) {
            push_back(T());
        }
        while (current_size > n) {
            pop_back();
        }
    }

    // Get the current size of the vector
    constexpr size_t size() const { return current_size; }

    constexpr bool empty() const { return current_size == 0; }

    // Get the capacity of the vector
    constexpr size_t capacity() const { return N; }

    // Add an element to the end of the vector
    void push_back(const T &value) {
        if (current_size < N) {
            void *mem = &memory()[current_size];
            new (mem) T(value);
            ++current_size;
        }
    }

    void reserve(size_t n) {
        if (n > N) {
            // This is a no-op for fixed size vectors
            return;
        }
    }

    void assign_array(const T *values, size_t count) {
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
            memory()[current_size].~T();
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
                new (p)
                    T(*(p + 1)); // Use copy constructor instead of std::move
                (p + 1)->~T();
            }
            --current_size;
        }
        return pos;
    }

    iterator erase(const T &value) {
        iterator it = find(value);
        if (it != end()) {
            erase(it);
        }
        return it;
    }

    iterator find(const T &value) {
        for (iterator it = begin(); it != end(); ++it) {
            if (*it == value) {
                return it;
            }
        }
        return end();
    }

    template <typename Predicate> iterator find_if(Predicate pred) {
        for (iterator it = begin(); it != end(); ++it) {
            if (pred(*it)) {
                return it;
            }
        }
        return end();
    }

    bool insert(iterator pos, const T &value) {
        if (current_size < N) {
            // shift all elements to the right
            // for (iterator p = end(); p != pos; --p) {
            //     new (p) T(*(p - 1)); // Use copy constructor instead of
            //     std::move (p - 1)->~T();
            // }
            // new (pos) T(value);
            // shift all element from pos to end to the right
            for (iterator p = end(); p != pos; --p) {
                // LOOKS LIKE THERE ARE BUGS AROUND THIS INSERT FUNCTION.
                // I'VE TRIED TO UPGRADE THE CODE TO USE TEMPORARIES BUT
                // IT SEEMS TO NOT WORK. IT COULD POSSIBLY DO WITH ALIGNMENT
                // OF THE DATA. THIS IMPL HAS ISSUES WITH THE NEW PLACE
                // OPERATION.
                T temp = *(p - 1);
                (p)->~T(); // Destroy the current element
                // Clear the memory
                void *vp = static_cast<void *>(p);
                memset(vp, 0, sizeof(T));
                new (p) T(temp); // Use copy constructor instead of std::move
                //(p - 1)->~T();
            }
            ++current_size;
            // now insert the new value
            new (pos) T(value);
            return true;
        }
        return false;
    }

    const_iterator find(const T &value) const {
        for (const_iterator it = begin(); it != end(); ++it) {
            if (*it == value) {
                return it;
            }
        }
        return end();
    }

    iterator data() { return begin(); }

    const_iterator data() const { return begin(); }

    bool has(const T &value) const { return find(value) != end(); }

    // Access to first and last elements
    T &front() { return memory()[0]; }

    const T &front() const { return memory()[0]; }

    T &back() { return memory()[current_size - 1]; }

    const T &back() const { return memory()[current_size - 1]; }

    // Iterator support
    iterator begin() { return &memory()[0]; }
    const_iterator begin() const { return &memory()[0]; }
    iterator end() { return &memory()[current_size]; }
    const_iterator end() const { return &memory()[current_size]; }

    void swap(FixedVector<T, N> &other) {
        if (this != &other) {
            const int max_size = MAX(current_size, other.current_size);
            for (int i = 0; i < max_size; ++i) {
                fl::swap(memory()[i], other.memory()[i]);
            }
            // swap the sizes
            size_t temp_size = current_size;
            current_size = other.current_size;
            other.current_size = temp_size;
        }
    }

  private:
    size_t current_size = 0;
};

template <typename T, typename Allocator = fl::allocator<T>> class HeapVector {
  private:
    T* mArray = nullptr;
    size_t mCapacity = 0;
    size_t mSize = 0;
    Allocator mAlloc;

  public:
    typedef T *iterator;
    typedef const T *const_iterator;

    struct reverse_iterator {
        iterator it;
        reverse_iterator(iterator i) : it(i) {}
        T &operator*() { return *(it - 1); }
        reverse_iterator &operator++() {
            --it;
            return *this;
        }
        bool operator!=(const reverse_iterator &other) const {
            return it != other.it;
        }
    };

    // Constructor
    HeapVector(size_t size = 0, const T &value = T()) : mCapacity(size), mSize(size) {
        if (size > 0) {
            // mArray.reset(size);
            mArray = mAlloc.allocate(size);
            for (size_t i = 0; i < size; ++i) {
                mAlloc.construct(&mArray[i], value);
            }
        }
    }
    HeapVector(const HeapVector<T> &other) {
        reserve(other.size());
        assign(other.begin(), other.end());
    }
    HeapVector(HeapVector<T> &&other) {
        this->swap(other);
        other.clear();
    }
    HeapVector &operator=(
        const HeapVector<T> &other) { // cppcheck-suppress operatorEqVarError
        if (this != &other) {
            assign(other.begin(), other.end());
        }
        return *this;
    }

    template <size_t N> HeapVector(T (&values)[N]) {
        T *begin = &values[0];
        T *end = &values[N];
        assign(begin, end);
    }

    // Destructor
    ~HeapVector() { 
        clear();
        if (mArray) {
            for (size_t i = 0; i < mSize; ++i) {
                mAlloc.destroy(&mArray[i]);
            }
            mAlloc.deallocate(mArray, mCapacity);
            mArray = nullptr;
        }
    }

    void ensure_size(size_t n) {
        if (n > mCapacity) {
            size_t new_capacity = (3 * mCapacity) / 2;
            if (new_capacity < n) {
                new_capacity = n;
            }
            
            T* new_array = mAlloc.allocate(new_capacity);
            
            // Copy existing elements to new array
            for (size_t i = 0; i < mSize; ++i) {
                mAlloc.construct(&new_array[i], mArray[i]);
            }
            
            // Clean up old array
            if (mArray) {
                for (size_t i = 0; i < mSize; ++i) {
                    mAlloc.destroy(&mArray[i]);
                }
                mAlloc.deallocate(mArray, mCapacity);
            }
            
            mArray = new_array;
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
        if (n > mCapacity) {
            ensure_size(n);
        }
        while (mSize < n) {
            push_back(T());
        }
        while (mSize > n) {
            pop_back();
        }
    }

    void resize(size_t n, const T &value) {
        if (n > mCapacity) {
            // Need to allocate more space
            T* new_array = mAlloc.allocate(n);
            
            // Copy existing elements
            for (size_t i = 0; i < mSize; ++i) {
                mAlloc.construct(&new_array[i], mArray[i]);
            }
            
            // Initialize new elements with value
            for (size_t i = mSize; i < n; ++i) {
                mAlloc.construct(&new_array[i], value);
            }
            
            // Clean up old array
            if (mArray) {
                for (size_t i = 0; i < mSize; ++i) {
                    mAlloc.destroy(&mArray[i]);
                }
                mAlloc.deallocate(mArray, mCapacity);
            }
            
            mArray = new_array;
            mCapacity = n;
            mSize = n;
        } else if (n > mSize) {
            // Just need to add more elements
            for (size_t i = mSize; i < n; ++i) {
                mAlloc.construct(&mArray[i], value);
            }
            mSize = n;
        } else if (n < mSize) {
            // Need to remove elements
            for (size_t i = n; i < mSize; ++i) {
                mAlloc.destroy(&mArray[i]);
            }
            mSize = n;
        }
    }

    template <typename InputIt,
              typename = fl::enable_if_t<!fl::is_integral<InputIt>::value>>
    void assign(InputIt begin, InputIt end) {
        clear();
        reserve(end - begin);
        for (InputIt it = begin; it != end; ++it) {
            push_back(*it);
        }
    }

    void assign(size_t new_cap, const T &value) {
        clear();
        reserve(new_cap);
        while (size() < new_cap) {
            push_back(value);
        }
    }

    // Array access operators
    T &operator[](size_t index) { return mArray[index]; }

    const T &operator[](size_t index) const { return mArray[index]; }

    // Capacity and size methods
    size_t size() const { return mSize; }

    bool empty() const { return mSize == 0; }

    size_t capacity() const { return mCapacity; }

    // Element addition/removal
    void push_back(const T &value) {
        ensure_size(mSize + 1);
        if (mSize < mCapacity) {
            mAlloc.construct(&mArray[mSize], value);
            ++mSize;
        }
    }

    void pop_back() {
        if (mSize > 0) {
            --mSize;
            mAlloc.destroy(&mArray[mSize]);
        }
    }

    void clear() {
        for (size_t i = 0; i < mSize; ++i) {
            mAlloc.destroy(&mArray[i]);
        }
        mSize = 0;
    }

    // Iterator methods
    iterator begin() { return &mArray[0]; }
    const_iterator begin() const { return &mArray[0]; }
    iterator end() { return &mArray[mSize]; }
    const_iterator end() const { return &mArray[mSize]; }

    reverse_iterator rbegin() { return reverse_iterator(end()); }

    reverse_iterator rend() { return reverse_iterator(begin()); }

    // Element access
    T &front() { return mArray[0]; }

    const T &front() const { return mArray[0]; }

    T &back() { return mArray[mSize - 1]; }

    const T &back() const { return mArray[mSize - 1]; }

    // Search and modification
    iterator find(const T &value) {
        for (iterator it = begin(); it != end(); ++it) {
            if (*it == value) {
                return it;
            }
        }
        return end();
    }

    const_iterator find(const T &value) const {
        for (const_iterator it = begin(); it != end(); ++it) {
            if (*it == value) {
                return it;
            }
        }
        return end();
    }

    template <typename Predicate> iterator find_if(Predicate pred) {
        for (iterator it = begin(); it != end(); ++it) {
            if (pred(*it)) {
                return it;
            }
        }
        return end();
    }

    bool has(const T &value) const { return find(value) != end(); }

    bool erase(iterator pos, T *out_value = nullptr) {
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

    void erase(const T &value) {
        iterator it = find(value);
        if (it != end()) {
            erase(it);
        }
    }

    void swap(HeapVector<T> &other) {
        fl::swap(mArray, other.mArray);
        fl::swap(mSize, other.mSize);
        fl::swap(mCapacity, other.mCapacity);
        fl::swap(mAlloc, other.mAlloc);
    }

    void swap(iterator a, iterator b) {
        T temp = *a;
        *a = *b;
        *b = temp;
    }

    bool full() const { return mSize >= mCapacity; }

    bool insert(iterator pos, const T &value) {
        // TODO: Introduce mMaxSize (and move it from SortedVector to here)
        // push back and swap into place.
        size_t target_idx = pos - begin();
        push_back(value);
        auto last = end() - 1;
        for (size_t curr_idx = last - begin(); curr_idx > target_idx;
             --curr_idx) {
            auto first = begin() + curr_idx - 1;
            auto second = begin() + curr_idx;
            swap(first, second);
        }
        return true;
    }

    // void assign(const T* values, size_t count) {
    //     clear();
    //     if (!mArray) {
    //         mArray.reset(new T[count]);
    //     }
    //     mCapacity = count;
    //     assign(values, values + count);
    // }

    // void assign(size_t new_cap, const T &value) {
    //     clear();
    //     reserve(new_cap);
    //     while (size() < new_cap) {
    //         push_back(value);
    //     }
    // }

    // 2) the iterator‐range overload, only enabled when InputIt is *not*
    // integral
    // template <typename InputIt>
    // void assign(InputIt begin, InputIt end) {
    //     clear();
    //     auto n = static_cast<std::size_t>(end - begin);
    //     reserve(n);
    //     for (InputIt it = begin; it != end; ++it) {
    //         push_back(*it);
    //     }
    // }

    T *data() { return mArray; }

    const T *data() const { return mArray; }

    bool operator==(const HeapVector<T> &other) const {
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

    bool operator!=(const HeapVector<T> &other) const {
        return !(*this == other);
    }
};

template <typename T, typename LessThan = fl::less<T>> class SortedHeapVector {
  private:
    HeapVector<T> mArray;
    LessThan mLess;
    size_t mMaxSize = size_t(-1);

  public:
    typedef typename HeapVector<T>::iterator iterator;
    typedef typename HeapVector<T>::const_iterator const_iterator;

    SortedHeapVector(LessThan less = LessThan()) : mLess(less) {}

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

    ~SortedHeapVector() { mArray.clear(); }

    void reserve(size_t n) { mArray.reserve(n); }

    // Insert while maintaining sort order
    bool insert(const T &value, InsertResult *result = nullptr) {
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

    // Find the first position where we should insert value to maintain sort
    // order
    iterator lower_bound(const T &value) {
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

    const_iterator lower_bound(const T &value) const {
        return const_cast<SortedHeapVector *>(this)->lower_bound(value);
    }

    // Lookup operations
    iterator find(const T &value) {
        iterator pos = lower_bound(value);
        if (pos != end() && !mLess(value, *pos) && !mLess(*pos, value)) {
            return pos;
        }
        return end();
    }

    void swap(SortedHeapVector &other) { mArray.swap(other.mArray); }

    const_iterator find(const T &value) const {
        return const_cast<SortedHeapVector *>(this)->find(value);
    }

    bool has(const T &value) const { return find(value) != end(); }

    // Removal operations
    bool erase(const T &value) {
        iterator it = find(value);
        if (it != end()) {
            return mArray.erase(it);
        }
        return false;
    }

    bool erase(iterator pos) { return mArray.erase(pos); }

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
    T &operator[](size_t index) { return mArray[index]; }
    const T &operator[](size_t index) const { return mArray[index]; }

    T &front() { return mArray.front(); }
    const T &front() const { return mArray.front(); }

    T &back() { return mArray.back(); }
    const T &back() const { return mArray.back(); }

    // Iterators
    iterator begin() { return mArray.begin(); }
    const_iterator begin() const { return mArray.begin(); }
    iterator end() { return mArray.end(); }
    const_iterator end() const { return mArray.end(); }

    // Raw data access
    T *data() { return mArray.data(); }
    const T *data() const { return mArray.data(); }
};

template <typename T, size_t INLINED_SIZE> class InlinedVector {
  public:
    using iterator = typename FixedVector<T, INLINED_SIZE>::iterator;
    using const_iterator =
        typename FixedVector<T, INLINED_SIZE>::const_iterator;

    InlinedVector() = default;
    InlinedVector(const InlinedVector &other) {
        if (other.mUsingHeap) {
            mHeap = other.mHeap;
            mUsingHeap = true;
        } else {
            mFixed = other.mFixed;
            mUsingHeap = false;
        }
    }
    InlinedVector(InlinedVector &&other) {
        // swap(*this, other);
        fl::swap(*this, other);
        other.clear();
    }
    InlinedVector(size_t size) : mUsingHeap(false) {
        if (size > INLINED_SIZE) {
            mHeap.resize(size);
            mUsingHeap = true;
        } else {
            mFixed.resize(size);
        }
    }

    InlinedVector &operator=(const InlinedVector &other) {
        if (this != &other) {
            assign(other.begin(), other.end());
        }
        return *this;
    }

    InlinedVector &operator=(InlinedVector &&other) {
        this->clear();
        if (this != &other) {
            if (other.mUsingHeap) {
                mHeap.swap(other.mHeap);
                mUsingHeap = true;
            } else {
                mFixed.swap(other.mFixed);
                mUsingHeap = false;
            }
            other.clear();
        }
        return *this;
    }

    void reserve(size_t size) {
        if (size > INLINED_SIZE) {
            if (mUsingHeap) {
                mHeap.reserve(size);
            } else {
                mHeap.reserve(size);
                for (auto &v : mFixed) {
                    mHeap.push_back(v);
                }
                mFixed.clear();
                mUsingHeap = true;
            }
        } else {
            if (mUsingHeap) {
                mFixed.reserve(size);
                for (auto &v : mHeap) {
                    mFixed.push_back(v);
                }
                mHeap.clear();
                mUsingHeap = false;
            } else {
                mFixed.reserve(size);
            }
        }
    }

    void resize(size_t size) {
        if (size > INLINED_SIZE) {
            if (mUsingHeap) {
                mHeap.resize(size);
            } else {
                mHeap.resize(size);
                for (auto &v : mFixed) {
                    mHeap.push_back(v);
                }
                mFixed.clear();
                mUsingHeap = true;
            }
        } else {
            if (mUsingHeap) {
                mFixed.resize(size);
                for (auto &v : mHeap) {
                    mFixed.push_back(v);
                }
                mHeap.clear();
                mUsingHeap = false;
            } else {
                mFixed.resize(size);
            }
        }
    }

    // Get current size
    size_t size() const { return mUsingHeap ? mHeap.size() : mFixed.size(); }
    bool empty() const { return size() == 0; }
    T *data() { return mUsingHeap ? mHeap.data() : mFixed.data(); }
    const T *data() const { return mUsingHeap ? mHeap.data() : mFixed.data(); }

    void assign(size_t new_cap, const T &value) {
        clear();
        if (INLINED_SIZE > new_cap) {
            // mFixed.assign(value);
            while (size() < new_cap) {
                mFixed.push_back(value);
            }
            return;
        }
        // mHeap.assign(value);
        mHeap.reserve(new_cap);
        mUsingHeap = true;
        while (size() < new_cap) {
            mHeap.push_back(value);
        }
    }

    template <typename InputIt,
              typename = fl::enable_if_t<!fl::is_integral<InputIt>::value>>
    void assign(InputIt begin, InputIt end) {
        clear();
        if (uint32_t(end - begin) <= INLINED_SIZE) {
            mFixed.assign(begin, end);
            return;
        }
        mHeap.assign(begin, end);
        mUsingHeap = true;
    }

    // void assign(const_iterator begin, const_iterator end) {
    //     clear();
    //     if (end - begin <= INLINED_SIZE) {
    //         mFixed.assign(begin, end);
    //         return;
    //     }
    //     mHeap.assign(begin, end);
    //     mUsingHeap = true;
    // }

    // Element access
    T &operator[](size_t idx) { return mUsingHeap ? mHeap[idx] : mFixed[idx]; }
    const T &operator[](size_t idx) const {
        return mUsingHeap ? mHeap[idx] : mFixed[idx];
    }

    bool full() const { return INLINED_SIZE == size(); }

    // Add an element
    void push_back(const T &value) {
        if (!mUsingHeap) {
            if (mFixed.size() < INLINED_SIZE) {
                mFixed.push_back(value);
                return;
            }
            // overflow: move inline data into heap
            mHeap.reserve(INLINED_SIZE * 2);
            for (auto &v : mFixed) {
                mHeap.push_back(v);
            }
            mFixed.clear();
            mUsingHeap = true;
        }
        mHeap.push_back(value);
    }

    // Remove last element
    void pop_back() {
        if (mUsingHeap) {
            mHeap.pop_back();
        } else {
            mFixed.pop_back();
        }
    }

    // Clear all elements
    void clear() {
        if (mUsingHeap) {
            mHeap.clear();
            // mUsingHeap = false;
        } else {
            mFixed.clear();
        }
    }

    template <typename Predicate> iterator find_if(Predicate pred) {
        for (iterator it = begin(); it != end(); ++it) {
            if (pred(*it)) {
                return it;
            }
        }
        return end();
    }

    void erase(iterator pos) {
        if (mUsingHeap) {
            mHeap.erase(pos);
        } else {
            mFixed.erase(pos);
        }
    }

    bool insert(iterator pos, const T &value) {
        if (mUsingHeap) {
            // return insert(pos, value);
            return mHeap.insert(pos, value);
        }

        if (mFixed.size() < INLINED_SIZE) {
            return mFixed.insert(pos, value);
        }

        // size_t diff = pos - mFixed.begin();
        // make safe for data that grows down
        size_t idx = mFixed.end() - pos;

        // overflow: move inline data into heap
        mHeap.reserve(INLINED_SIZE * 2);
        for (auto &v : mFixed) {
            mHeap.push_back(v);
        }
        mFixed.clear();
        return mHeap.insert(mHeap.begin() + idx, value);
    }

    // Iterators
    iterator begin() { return mUsingHeap ? mHeap.begin() : mFixed.begin(); }
    iterator end() { return mUsingHeap ? mHeap.end() : mFixed.end(); }
    const_iterator begin() const {
        return mUsingHeap ? mHeap.begin() : mFixed.begin();
    }
    const_iterator end() const {
        return mUsingHeap ? mHeap.end() : mFixed.end();
    }

    // back, front
    T &front() { return mUsingHeap ? mHeap.front() : mFixed.front(); }
    const T &front() const {
        return mUsingHeap ? mHeap.front() : mFixed.front();
    }

    T &back() { return mUsingHeap ? mHeap.back() : mFixed.back(); }
    const T &back() const { return mUsingHeap ? mHeap.back() : mFixed.back(); }

    void swap(InlinedVector &other) {
        if (this != &other) {
            fl::swap(mUsingHeap, other.mUsingHeap);
            fl::swap(mFixed, other.mFixed);
            fl::swap(mHeap, other.mHeap);
        }
    }

  private:
    bool mUsingHeap = false;
    FixedVector<T, INLINED_SIZE> mFixed;
    HeapVector<T> mHeap;
};

template <typename T, typename Allocator = fl::allocator<T>> using vector = HeapVector<T, Allocator>;

template <typename T, size_t INLINED_SIZE>
using vector_fixed = FixedVector<T, INLINED_SIZE>;

template <typename T, size_t INLINED_SIZE = 64>
using vector_inlined = InlinedVector<T, INLINED_SIZE>;

} // namespace fl
