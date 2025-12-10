#pragma once

#include "ftl/stdint.h"
#include "fl/int.h"
#include "ftl/move.h"
#include "ftl/allocator.h"
#include "ftl/type_traits.h"
#include "ftl/initializer_list.h"
#include "ftl/new.h"
#include "fl/align.h"
#include "fl/unused.h"

// Note: This file is included inside namespace fl in vector.h
// Do not wrap in namespace fl {} here

// Forward declaration for span (defined in fl/slice.h)
template <typename T, fl::size Extent> class span;

template <typename T, typename Allocator = fl::allocator<T>> 
class FL_ALIGN HeapVector {
  private:
    T* mArray = nullptr;
    fl::size mCapacity = 0;
    fl::size mSize = 0;
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

    // Default constructor
    HeapVector() : mArray(nullptr),mCapacity(0), mSize(0) {
    }
    
    // Constructor with size and value
    HeapVector(fl::size size, const T &value = T()) : mCapacity(size), mSize(size) {
        if (size > 0) {
            // mArray.reset(size);
            mArray = mAlloc.allocate(size);
            for (fl::size i = 0; i < size; ++i) {
                mAlloc.construct(&mArray[i], value);
            }
        }
    }
    HeapVector(const HeapVector<T, Allocator> &other) {
        reserve(other.size());
        assign(other.begin(), other.end());
    }
    // Move constructor - directly transfer ownership to maintain invariants
    HeapVector(HeapVector<T, Allocator> &&other) noexcept
        : mArray(other.mArray)
        , mCapacity(other.mCapacity)
        , mSize(other.mSize)
        , mAlloc(fl::move(other.mAlloc)) {
        // Leave other in valid empty state (maintains invariant)
        other.mArray = nullptr;
        other.mSize = 0;
        other.mCapacity = 0;
    }
    HeapVector &operator=(
        const HeapVector<T, Allocator> &other) { // cppcheck-suppress operatorEqVarError
        if (this != &other) {
            mAlloc = other.mAlloc;
            assign(other.begin(), other.end());
        }
        return *this;
    }

    // Move assignment operator - explicitly defined to maintain invariants
    HeapVector &operator=(HeapVector<T, Allocator> &&other) noexcept {
        if (this != &other) {
            // Clean up our own resources first
            clear();
            if (mArray) {
                mAlloc.deallocate(mArray, mCapacity);
            }

            // Transfer ownership from other
            mArray = other.mArray;
            mSize = other.mSize;
            mCapacity = other.mCapacity;
            mAlloc = fl::move(other.mAlloc);

            // Leave other in valid empty state (maintains invariant)
            other.mArray = nullptr;
            other.mSize = 0;
            other.mCapacity = 0;
        }
        return *this;
    }

    template <fl::size N> HeapVector(T (&values)[N]) {
        T *begin = &values[0];
        T *end = &values[N];
        assign(begin, end);
    }

    // emplace back
    template <typename... Args>
    void emplace_back(Args&&... args) {
        push_back(T(fl::forward<Args>(args)...));
    }

    // Initializer list constructor (C++11 and later) - uses fl::initializer_list
    HeapVector(fl::initializer_list<T> init) {
        reserve(init.size());
        for (const auto& value : init) {
            push_back(value);
        }
    }

    // Iterator-based constructor
    template <typename InputIterator>
    HeapVector(InputIterator first, InputIterator last) {
        assign(first, last);
    }

    // Implicit copy constructor from span
    // Note: dynamic_extent is defined as fl::size(-1) in fl/slice.h
    HeapVector(fl::span<const T, fl::size(-1)> s) {
        reserve(s.size());
        assign(s.begin(), s.end());
    }

    // Destructor
    ~HeapVector() {
        clear();  // Destroys all elements and sets mSize = 0
        if (mArray) {
            mAlloc.deallocate(mArray, mCapacity);
            mArray = nullptr;
        }
    }

    void ensure_size(fl::size n) {
        if (n > mCapacity) {
            fl::size new_capacity = (3 * mCapacity) / 2;
            if (new_capacity < n) {
                new_capacity = n;
            }
            ensure_size_impl(n, new_capacity);
        }
    }

private:
    // SFINAE helper: Try reallocate() path if allocator supports it
    template <typename A = Allocator>
    typename fl::enable_if<fl::allocator_traits<A>::has_reallocate_v, bool>::type
    try_reallocate(fl::size new_capacity) {
        T* reallocated = mAlloc.reallocate(mArray, mCapacity, new_capacity);
        if (reallocated) {
            mArray = reallocated;
            mCapacity = new_capacity;
            return true;  // Success!
        }
        return false;  // Not supported or failed
    }

    // Fallback: reallocate() not supported
    template <typename A = Allocator>
    typename fl::enable_if<!fl::allocator_traits<A>::has_reallocate_v, bool>::type
    try_reallocate(fl::size new_capacity) {
        FASTLED_UNUSED(new_capacity);
        return false;
    }

    // SFINAE helper: Try allocate_at_least() path if allocator supports it
    template <typename A = Allocator>
    typename fl::enable_if<fl::allocator_traits<A>::has_allocate_at_least_v, T*>::type
    allocate_new(fl::size new_capacity, fl::size& actual_capacity) {
        auto result = mAlloc.allocate_at_least(new_capacity);
        actual_capacity = result.count;
        return result.ptr;
    }

    // Fallback: Use standard allocate()
    template <typename A = Allocator>
    typename fl::enable_if<!fl::allocator_traits<A>::has_allocate_at_least_v, T*>::type
    allocate_new(fl::size new_capacity, fl::size& actual_capacity) {
        actual_capacity = new_capacity;
        return mAlloc.allocate(new_capacity);
    }

    // Implementation: Try optimized paths, fall back to standard if needed
    void ensure_size_impl(fl::size n, fl::size new_capacity) {
        FASTLED_UNUSED(n);
        // PATH 1: Try optimized reallocate() if allocator supports it
        // This avoids copying for POD types and moves memory in-place
        if (try_reallocate<Allocator>(new_capacity)) {
            return;  // Success! No need for copy
        }

        // PATH 2: Try allocate_at_least() or standard allocate()
        // Allocator can return MORE than requested to reduce future reallocations
        fl::size actual_capacity = new_capacity;
        T* new_array = allocate_new<Allocator>(new_capacity, actual_capacity);

        if (!new_array) {
            return;  // Allocation failed
        }

        // Move existing elements to new array
        for (fl::size i = 0; i < mSize; ++i) {
            mAlloc.construct(&new_array[i], fl::move(mArray[i]));
        }

        // Clean up old array
        if (mArray) {
            for (fl::size i = 0; i < mSize; ++i) {
                mAlloc.destroy(&mArray[i]);
            }
            mAlloc.deallocate(mArray, mCapacity);
        }

        mArray = new_array;
        mCapacity = actual_capacity;  // Use actual allocated count
    }

public:

    void reserve(fl::size n) {
        if (n > mCapacity) {
            ensure_size(n);
        }
    }

    void resize(fl::size n) {
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

    void resize(fl::size n, const T &value) {
        if (n > mCapacity) {
            // Need to allocate more space
            T* new_array = mAlloc.allocate(n);
            
            // Move existing elements
            for (fl::size i = 0; i < mSize; ++i) {
                mAlloc.construct(&new_array[i], fl::move(mArray[i]));
            }
            
            // Initialize new elements with value
            for (fl::size i = mSize; i < n; ++i) {
                mAlloc.construct(&new_array[i], value);
            }
            
            // Clean up old array
            if (mArray) {
                for (fl::size i = 0; i < mSize; ++i) {
                    mAlloc.destroy(&mArray[i]);
                }
                mAlloc.deallocate(mArray, mCapacity);
            }
            
            mArray = new_array;
            mCapacity = n;
            mSize = n;
        } else if (n > mSize) {
            // Just need to add more elements
            for (fl::size i = mSize; i < n; ++i) {
                mAlloc.construct(&mArray[i], value);
            }
            mSize = n;
        } else if (n < mSize) {
            // Need to remove elements
            for (fl::size i = n; i < mSize; ++i) {
                mAlloc.destroy(&mArray[i]);
            }
            mSize = n;
        }
    }

    template <typename InputIt,
              typename = fl::enable_if_t<!fl::is_integral<InputIt>::value>>
    void assign(InputIt begin, InputIt end) {
        clear();
        fl::size n = static_cast<fl::size>(end - begin);
        reserve(n);
        for (InputIt it = begin; it != end; ++it) {
            push_back(*it);
        }
    }

    void assign(fl::size new_cap, const T &value) {
        clear();
        reserve(new_cap);
        while (size() < new_cap) {
            push_back(value);
        }
    }

    // Array access operators
    T &operator[](fl::size index) { return mArray[index]; }

    const T &operator[](fl::size index) const { return mArray[index]; }

    // Capacity and size methods
    fl::size size() const { return mSize; }

    bool empty() const { return mSize == 0; }

    fl::size capacity() const { return mCapacity; }

    // Element addition/removal
    void push_back(const T &value) {
        ensure_size(mSize + 1);
        if (mSize < mCapacity) {
            mAlloc.construct(&mArray[mSize], value);
            ++mSize;
        }
    }

    // Move version of push_back
    void push_back(T &&value) {
        ensure_size(mSize + 1);
        if (mSize < mCapacity) {
            mAlloc.construct(&mArray[mSize], fl::move(value));
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
        if (mArray) {  // Add null check before accessing array
            for (fl::size i = 0; i < mSize; ++i) {
                mAlloc.destroy(&mArray[i]);
            }
        }
        mSize = 0;
    }

    // Iterator methods
    iterator begin() { 
        return mArray ? &mArray[0] : nullptr; 
    }
    const_iterator begin() const { 
        return mArray ? &mArray[0] : nullptr; 
    }
    iterator end() { 
        return mArray ? &mArray[mSize] : nullptr; 
    }
    const_iterator end() const { 
        return mArray ? &mArray[mSize] : nullptr; 
    }

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
            *out_value = fl::move(*pos);
        }
        while (pos != end() - 1) {
            *pos = fl::move(*(pos + 1));
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

    void swap(HeapVector<T> &&other) {
        fl::swap(mArray, other.mArray);
        fl::swap(mSize, other.mSize);
        fl::swap(mCapacity, other.mCapacity);
        fl::swap(mAlloc, other.mAlloc);
    }

    void swap(iterator a, iterator b) {
        fl::swap(*a, *b);
    }

    bool full() const { return mSize >= mCapacity; }

    bool insert(iterator pos, const T &value) {
        // TODO: Introduce mMaxSize (and move it from SortedVector to here)
        // push back and swap into place.
        fl::size target_idx = pos - begin();
        push_back(value);
        auto last = end() - 1;
        for (fl::size curr_idx = last - begin(); curr_idx > target_idx;
             --curr_idx) {
            auto first = begin() + curr_idx - 1;
            auto second = begin() + curr_idx;
            swap(first, second);
        }
        return true;
    }

    // Move version of insert
    bool insert(iterator pos, T &&value) {
        // push back and swap into place.
        fl::size target_idx = pos - begin();
        push_back(fl::move(value));
        auto last = end() - 1;
        for (fl::size curr_idx = last - begin(); curr_idx > target_idx;
             --curr_idx) {
            auto first = begin() + curr_idx - 1;
            auto second = begin() + curr_idx;
            swap(first, second);
        }
        return true;
    }

    // 2) the iterator‐range overload, only enabled when InputIt is *not*
    // integral
    // template <typename InputIt>
    // void assign(InputIt begin, InputIt end) {
    //     clear();
    //     auto n = static_cast<std::fl::size>(end - begin);
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
        for (fl::size i = 0; i < size(); ++i) {
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
