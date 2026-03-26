#pragma once

// allow-include-after-namespace

#include "fl/stl/stdint.h"  // IWYU pragma: keep
#include "fl/stl/int.h"  // IWYU pragma: keep

#include "fl/stl/bit_cast.h"
#include "fl/stl/functional.h"  // IWYU pragma: keep
#include "fl/stl/initializer_list.h"  // IWYU pragma: keep
#include "fl/math/math.h"
#include "fl/stl/cstring.h"
#include "fl/stl/allocator.h"
#include "fl/stl/scoped_ptr.h"  // IWYU pragma: keep
#include "fl/stl/type_traits.h"
#include "fl/stl/new.h"  // IWYU pragma: keep
#include "fl/stl/move.h"
#include "fl/stl/align.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/basic_vector.h"
#include "fl/stl/noexcept.h"

namespace fl {

// Forward declaration for span (defined in fl/stl/span.h)
template <typename T, fl::size Extent> class span;  // IWYU pragma: keep

// Aligned memory block for inlined data structures.
// Must be aligned to at least the alignment of T, but not less than the natural alignment
template <typename T, fl::size N>
struct FL_ALIGNAS(alignof(T) > alignof(fl::uptr) ? alignof(T) : alignof(fl::uptr)) InlinedMemoryBlock {
    // using MemoryType = uinptr_t;
    typedef fl::uptr MemoryType;
    enum {
        kTotalBytes = N * sizeof(T),
        kExtraSize =
            (kTotalBytes % alignof(fl::max_align_t)) ? (alignof(fl::max_align_t) - (kTotalBytes % alignof(fl::max_align_t))) : 0,
        // Fix: calculate total bytes first, then convert to MemoryType units
        kTotalBytesAligned = kTotalBytes + kExtraSize,
        kBlockSize = (kTotalBytesAligned + sizeof(MemoryType) - 1) / sizeof(MemoryType),
    };

    InlinedMemoryBlock() FL_NOEXCEPT {
        fl::memset(mMemoryBlock, 0, sizeof(mMemoryBlock));
#ifdef FASTLED_TESTING
        __data = memory();
#endif
    }

    InlinedMemoryBlock(const InlinedMemoryBlock &other) FL_NOEXCEPT = default;
    InlinedMemoryBlock(InlinedMemoryBlock &&other) FL_NOEXCEPT = default;

    // u32 mRaw[N * sizeof(T)/sizeof(MemoryType) + kExtraSize];
    // align this to the size of MemoryType.
    // u32 mMemoryBlock[kTotalSize] = {0};
    MemoryType mMemoryBlock[kBlockSize];

    T *memory() FL_NOEXCEPT {
        MemoryType *begin = &mMemoryBlock[0];
        fl::uptr shift_up =
            fl::ptr_to_int(begin) & (sizeof(MemoryType) - 1);
        MemoryType *raw = begin + shift_up;
        return fl::bit_cast<T *>(raw);
    }

    const T *memory() const FL_NOEXCEPT {
        const MemoryType *begin = &mMemoryBlock[0];
        const fl::uptr shift_up =
            fl::ptr_to_int(begin) & (sizeof(MemoryType) - 1);
        const MemoryType *raw = begin + shift_up;
        return fl::bit_cast<const T *>(raw);
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
template <typename T, fl::size N>
class FL_ALIGN FixedVector {
  private:
    InlinedMemoryBlock<T, N> mMemoryBlock;

    T *memory() FL_NOEXCEPT { return mMemoryBlock.memory(); }

    const T *memory() const FL_NOEXCEPT { return mMemoryBlock.memory(); }

  public:
    typedef T value_type;
    typedef T *iterator;
    typedef const T *const_iterator;
    // Constructor
    constexpr FixedVector() FL_NOEXCEPT : current_size(0) {}

    FixedVector(const T (&values)[N]) FL_NOEXCEPT : current_size(N) {
        assign_array(values, N);
    }

    FixedVector(FixedVector &&other) FL_NOEXCEPT : current_size(0) {
        fl::swap(*this, other);
        other.clear();
    }

    FixedVector(const FixedVector &other) FL_NOEXCEPT : current_size(other.current_size) {
        assign_array(other.memory(), other.current_size);
    }

    template <fl::size M> FixedVector(T (&values)[M]) FL_NOEXCEPT : current_size(0) {
        static_assert(M <= N, "Too many elements for FixedVector");
        assign_array(values, M);
    }

    // Initializer list constructor (C++11 and later) - uses fl::initializer_list
    FixedVector(fl::initializer_list<T> init) FL_NOEXCEPT : current_size(0) {
        if (init.size() > N) {
            // Only assign the first N elements if the list is too long
            auto it = init.begin();
            for (fl::size i = 0; i < N && it != init.end(); ++i, ++it) {
                push_back(*it);
            }
        } else {
            for (const auto& value : init) {
                push_back(value);
            }
        }
    }

    // Implicit copy constructor from span (dynamic extent)
    // Only copies up to N elements from the span
    FixedVector(fl::span<const T, fl::size(-1)> s) FL_NOEXCEPT : current_size(0) {
        fl::size count = s.size() < N ? s.size() : N;
        for (fl::size i = 0; i < count; ++i) {
            push_back(s[i]);
        }
    }

    FixedVector &operator=(const FixedVector &other) FL_NOEXCEPT { // cppcheck-suppress operatorEqVarError
        if (this != &other) {
            assign_array(other.memory(), other.current_size);
        }
        return *this;
    }

    FixedVector &operator=(FixedVector &&other) FL_NOEXCEPT { // cppcheck-suppress operatorEqVarError
        if (this != &other) {
            fl::swap(*this, other);
            other.clear();
        }
        return *this;
    }

    // Destructor
    ~FixedVector() FL_NOEXCEPT { clear(); }

    // Array subscript operator
    T &operator[](fl::size index) FL_NOEXCEPT { return memory()[index]; }

    // Const array subscript operator
    const T &operator[](fl::size index) const FL_NOEXCEPT {
        if (index >= current_size) {
            const T *out = nullptr;
            return *out; // Cause a nullptr dereference
        }
        return memory()[index];
    }

    void resize(fl::size n) FL_NOEXCEPT {
        while (current_size < n) {
            push_back(T());
        }
        while (current_size > n) {
            pop_back();
        }
    }

    // Get the current size of the vector
    constexpr fl::size size() const FL_NOEXCEPT { return current_size; }

    constexpr bool empty() const FL_NOEXCEPT { return current_size == 0; }

    // Get the capacity of the vector
    constexpr fl::size capacity() const FL_NOEXCEPT { return N; }

    // Add an element to the end of the vector
    void push_back(const T &value) FL_NOEXCEPT {
        if (current_size < N) {
            void *mem = &memory()[current_size];
            new (mem) T(value);
            ++current_size;
        }
    }

    // Move version of push_back
    void push_back(T &&value) FL_NOEXCEPT {
        if (current_size < N) {
            void *mem = &memory()[current_size];
            new (mem) T(fl::move(value));
            ++current_size;
        }
    }

    // Emplace back - construct in place
    template<typename... Args>
    void emplace_back(Args&&... args) FL_NOEXCEPT {
        if (current_size < N) {
            void *mem = &memory()[current_size];
            new (mem) T(fl::forward<Args>(args)...);
            ++current_size;
        }
    }

    void reserve(fl::size n) FL_NOEXCEPT {
        if (n > N) {
            // This is a no-op for fixed size vectors
            return;
        }
    }

    void assign_array(const T *values, fl::size count) FL_NOEXCEPT {
        clear();
        for (fl::size i = 0; i < count; ++i) {
            push_back(values[i]);
        }
    }

    void assign(const_iterator begin, const_iterator end) FL_NOEXCEPT {
        clear();
        for (const_iterator it = begin; it != end; ++it) {
            push_back(*it);
        }
    }

    // Remove the last element from the vector
    void pop_back() FL_NOEXCEPT {
        if (current_size > 0) {
            --current_size;
            memory()[current_size].~T();
        }
    }

    // Clear the vector
    void clear() FL_NOEXCEPT {
        while (current_size > 0) {
            pop_back();
        }
    }

    // Erase the element at the given iterator position
    iterator erase(iterator pos) FL_NOEXCEPT {
        if (pos != end()) {
            pos->~T();
            // shift all elements to the left
            for (iterator p = pos; p != end() - 1; ++p) {
                new (p) T(fl::move(*(p + 1))); // Use move constructor
                (p + 1)->~T();
            }
            --current_size;
        }
        return pos;
    }

    iterator erase(const T &value) FL_NOEXCEPT {
        iterator it = find(value);
        if (it != end()) {
            erase(it);
        }
        return it;
    }

    iterator find(const T &value) FL_NOEXCEPT {
        for (iterator it = begin(); it != end(); ++it) {
            if (*it == value) {
                return it;
            }
        }
        return end();
    }

    template <typename Predicate> iterator find_if(Predicate pred) FL_NOEXCEPT {
        for (iterator it = begin(); it != end(); ++it) {
            if (pred(*it)) {
                return it;
            }
        }
        return end();
    }

    bool insert(iterator pos, const T &value) FL_NOEXCEPT {
        if (current_size >= N) {
            return false;
        }

        // Construct a new element at end() position using placement new
        new (end()) T();
        ++current_size;

        // Shift elements from [pos, end-1) to the right by one position
        for (iterator p = end() - 1; p > pos; --p) {
            *p = fl::move(*(p - 1));
        }

        // Assign the new value to the insertion position
        *pos = value;
        return true;
    }

    // Move version of insert
    bool insert(iterator pos, T &&value) FL_NOEXCEPT {
        if (current_size >= N) {
            return false;
        }

        // Construct a new element at end() position using placement new
        new (end()) T();
        ++current_size;

        // Shift elements from [pos, end-1) to the right by one position
        for (iterator p = end() - 1; p > pos; --p) {
            *p = fl::move(*(p - 1));
        }

        // Move-assign the new value to the insertion position
        *pos = fl::move(value);
        return true;
    }

    const_iterator find(const T &value) const FL_NOEXCEPT {
        for (const_iterator it = begin(); it != end(); ++it) {
            if (*it == value) {
                return it;
            }
        }
        return end();
    }

    iterator data() FL_NOEXCEPT { return begin(); }

    const_iterator data() const FL_NOEXCEPT { return begin(); }

    bool has(const T &value) const FL_NOEXCEPT { return find(value) != end(); }

    // Access to first and last elements
    T &front() FL_NOEXCEPT { return memory()[0]; }

    const T &front() const FL_NOEXCEPT { return memory()[0]; }

    T &back() FL_NOEXCEPT { return memory()[current_size - 1]; }

    const T &back() const FL_NOEXCEPT { return memory()[current_size - 1]; }

    // Reverse iterator types (same as vector)
    struct reverse_iterator {
        iterator it;
        reverse_iterator(iterator i) FL_NOEXCEPT : it(i) {}
        T &operator*() FL_NOEXCEPT { return *(it - 1); }
        T *operator->() { return (it - 1); }
        reverse_iterator &operator++() FL_NOEXCEPT {
            --it;
            return *this;
        }
        bool operator==(const reverse_iterator &other) const FL_NOEXCEPT {
            return it == other.it;
        }
        bool operator!=(const reverse_iterator &other) const FL_NOEXCEPT {
            return it != other.it;
        }
    };

    struct const_reverse_iterator {
        const_iterator it;
        const_reverse_iterator(const_iterator i) FL_NOEXCEPT : it(i) {}
        const T &operator*() const FL_NOEXCEPT { return *(it - 1); }
        const T *operator->() const { return (it - 1); }
        const_reverse_iterator &operator++() FL_NOEXCEPT {
            --it;
            return *this;
        }
        bool operator==(const const_reverse_iterator &other) const FL_NOEXCEPT {
            return it == other.it;
        }
        bool operator!=(const const_reverse_iterator &other) const FL_NOEXCEPT {
            return it != other.it;
        }
    };

    // Iterator support
    iterator begin() FL_NOEXCEPT { return &memory()[0]; }
    const_iterator begin() const FL_NOEXCEPT { return &memory()[0]; }
    iterator end() FL_NOEXCEPT { return &memory()[current_size]; }
    const_iterator end() const FL_NOEXCEPT { return &memory()[current_size]; }

    // Reverse iterator support
    reverse_iterator rbegin() FL_NOEXCEPT { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const FL_NOEXCEPT { return const_reverse_iterator(end()); }
    reverse_iterator rend() FL_NOEXCEPT { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const FL_NOEXCEPT { return const_reverse_iterator(begin()); }

    // Capacity management (no-op for fixed-size vector)
    void shrink_to_fit() FL_NOEXCEPT {
        // No-op for fixed-size vectors
    }

    void swap(FixedVector<T, N> &other) FL_NOEXCEPT {
        if (this == &other) return;
        fl::size min_sz = fl::min(current_size, other.current_size);
        fl::size max_sz = fl::max(current_size, other.current_size);
        // Swap elements that exist in both vectors
        for (fl::size i = 0; i < min_sz; ++i) {
            fl::swap(memory()[i], other.memory()[i]);
        }
        // Move remaining elements from the larger to the smaller
        if (current_size > other.current_size) {
            for (fl::size i = min_sz; i < max_sz; ++i) {
                T* src = memory() + i;
                T* dst = other.memory() + i;
                new (dst) T(fl::move(*src));
                src->~T();
            }
        } else if (other.current_size > current_size) {
            for (fl::size i = min_sz; i < max_sz; ++i) {
                T* src = other.memory() + i;
                T* dst = memory() + i;
                new (dst) T(fl::move(*src));
                src->~T();
            }
        }
        fl::size temp_size = current_size;
        current_size = other.current_size;
        other.current_size = temp_size;
    }

  private:
    fl::size current_size = 0;
};

///////////////////////////////////////////////////////////////////////////////
// vector<T> — Thin template wrapper around vector_basic.
// All memory management and element operations are in vector_basic (compiled once).
// This class provides type safety, typed iterators, and sets up the ops table.
///////////////////////////////////////////////////////////////////////////////

template <typename T>
class FL_ALIGN vector : public vector_basic {
  public:
    typedef T value_type;
    typedef T *iterator;
    typedef const T *const_iterator;

    struct reverse_iterator {
        iterator it;
        reverse_iterator(iterator i) FL_NOEXCEPT : it(i) {}
        T &operator*() FL_NOEXCEPT { return *(it - 1); }
        T *operator->() { return (it - 1); }
        reverse_iterator &operator++() FL_NOEXCEPT {
            --it;
            return *this;
        }
        bool operator==(const reverse_iterator &other) const FL_NOEXCEPT {
            return it == other.it;
        }
        bool operator!=(const reverse_iterator &other) const FL_NOEXCEPT {
            return it != other.it;
        }
    };

    struct const_reverse_iterator {
        const_iterator it;
        const_reverse_iterator(const_iterator i) FL_NOEXCEPT : it(i) {}
        const T &operator*() const FL_NOEXCEPT { return *(it - 1); }
        const T *operator->() const { return (it - 1); }
        const_reverse_iterator &operator++() FL_NOEXCEPT {
            --it;
            return *this;
        }
        bool operator==(const const_reverse_iterator &other) const FL_NOEXCEPT {
            return it == other.it;
        }
        bool operator!=(const const_reverse_iterator &other) const FL_NOEXCEPT {
            return it != other.it;
        }
    };

    // ======= CONSTRUCTORS =======

    // Default constructor
    vector()
        : vector_basic(sizeof(T), default_memory_resource(),
                        vector_element_ops_for<T>()) {}

    // Constructor with memory resource
    explicit vector(memory_resource* resource)
        : vector_basic(sizeof(T), resource, vector_element_ops_for<T>()) {}

    // Constructor with size and value
    vector(fl::size count, const T &value = T())
        : vector_basic(sizeof(T), default_memory_resource(),
                        vector_element_ops_for<T>()) {
        resize_value_impl(count, &value);
    }

    // Constructor with size, value, and memory resource
    vector(fl::size count, const T &value, memory_resource* resource)
        : vector_basic(sizeof(T), resource, vector_element_ops_for<T>()) {
        resize_value_impl(count, &value);
    }

    // Copy constructor
    vector(const vector &other)
        : vector_basic(sizeof(T), other.mResource,
                        vector_element_ops_for<T>()) {
        copy_from(other);
    }

    // Move constructor
    vector(vector &&other) FL_NOEXCEPT
        : vector_basic(sizeof(T), other.mResource,
                        vector_element_ops_for<T>()) {
        move_from(other);
    }

    // Array constructor
    template <fl::size N> vector(T (&values)[N]) FL_NOEXCEPT
        : vector_basic(sizeof(T), default_memory_resource(),
                        vector_element_ops_for<T>()) {
        reserve_impl(N);
        for (fl::size i = 0; i < N; ++i) {
            push_back(values[i]);
        }
    }

    // Initializer list constructor
    vector(fl::initializer_list<T> init) FL_NOEXCEPT
        : vector_basic(sizeof(T), default_memory_resource(),
                        vector_element_ops_for<T>()) {
        reserve_impl(init.size());
        for (const auto& value : init) {
            push_back(value);
        }
    }

    // Iterator-based constructor (SFINAE: exclude integral types to avoid
    // ambiguity with vector(size, value) when called as vector(int, int))
    template <typename InputIterator,
              typename = fl::enable_if_t<!fl::is_integral<InputIterator>::value>>
    vector(InputIterator first, InputIterator last) FL_NOEXCEPT
        : vector_basic(sizeof(T), default_memory_resource(),
                        vector_element_ops_for<T>()) {
        for (auto it = first; it != last; ++it) {
            push_back(*it);
        }
    }

    // Span constructor
    vector(span<const T, fl::size(-1)> s) FL_NOEXCEPT
        : vector_basic(sizeof(T), default_memory_resource(),
                        vector_element_ops_for<T>()) {
        reserve_impl(s.size());
        for (fl::size i = 0; i < s.size(); ++i) {
            push_back(s[i]);
        }
    }

    // ======= ASSIGNMENT =======

    vector &operator=(const vector &other) {
        if (this != &other) {
            copy_from(other);
        }
        return *this;
    }

    vector &operator=(vector &&other) FL_NOEXCEPT {
        if (this != &other) {
            move_assign(other);
        }
        return *this;
    }

    // ======= DESTRUCTOR =======
    // Base class ~vector_basic() handles cleanup via mOps

    // ======= CAPACITY =======
    // size(), empty(), capacity(), full() inherited from vector_basic

    void reserve(fl::size n) FL_NOEXCEPT { reserve_impl(n); }

    void resize(fl::size n) FL_NOEXCEPT { resize_impl(n); }

    void resize(fl::size n, const T &value) FL_NOEXCEPT { resize_value_impl(n, &value); }

    void shrink_to_fit() FL_NOEXCEPT { shrink_to_fit_impl(); }

    void ensure_size(fl::size n) FL_NOEXCEPT { reserve_impl(n); }

    memory_resource* get_resource() const FL_NOEXCEPT { return mResource; }

    // ======= ELEMENT ACCESS =======

    T &operator[](fl::size index) FL_NOEXCEPT {
        return static_cast<T*>(mArray)[index];
    }

    const T &operator[](fl::size index) const FL_NOEXCEPT {
        return static_cast<const T*>(mArray)[index];
    }

    T &front() FL_NOEXCEPT { return static_cast<T*>(mArray)[0]; }
    const T &front() const FL_NOEXCEPT { return static_cast<const T*>(mArray)[0]; }

    T &back() FL_NOEXCEPT { return static_cast<T*>(mArray)[mSize - 1]; }
    const T &back() const FL_NOEXCEPT { return static_cast<const T*>(mArray)[mSize - 1]; }

    T *data() FL_NOEXCEPT { return static_cast<T*>(mArray); }
    const T *data() const FL_NOEXCEPT { return static_cast<const T*>(mArray); }

    // ======= MODIFIERS =======

    void push_back(const T &value) FL_NOEXCEPT { push_back_copy_impl(&value); }
    void push_back(T &&value) FL_NOEXCEPT { push_back_move_impl(&value); }

    template <typename... Args>
    void emplace_back(Args&&... args) FL_NOEXCEPT {
        T tmp(fl::forward<Args>(args)...);
        push_back_move_impl(&tmp);
    }

    void pop_back() FL_NOEXCEPT { pop_back_impl(); }
    void clear() FL_NOEXCEPT { clear_impl(); }

    template <typename InputIt,
              typename = fl::enable_if_t<!fl::is_integral<InputIt>::value>>
    void assign(InputIt first, InputIt last) FL_NOEXCEPT {
        clear();
        for (auto it = first; it != last; ++it) {
            push_back(*it);
        }
    }

    void assign(fl::size new_cap, const T &value) FL_NOEXCEPT {
        clear();
        reserve_impl(new_cap);
        for (fl::size i = 0; i < new_cap; ++i) {
            push_back(value);
        }
    }

    // ======= ITERATORS =======

    iterator begin() FL_NOEXCEPT {
        return mArray ? static_cast<T*>(mArray) : nullptr;
    }
    const_iterator begin() const FL_NOEXCEPT {
        return mArray ? static_cast<const T*>(mArray) : nullptr;
    }
    iterator end() FL_NOEXCEPT {
        return mArray ? static_cast<T*>(mArray) + mSize : nullptr;
    }
    const_iterator end() const FL_NOEXCEPT {
        return mArray ? static_cast<const T*>(mArray) + mSize : nullptr;
    }

    reverse_iterator rbegin() FL_NOEXCEPT { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const FL_NOEXCEPT { return const_reverse_iterator(end()); }
    reverse_iterator rend() FL_NOEXCEPT { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const FL_NOEXCEPT { return const_reverse_iterator(begin()); }

    const_iterator cbegin() const FL_NOEXCEPT {
        return mArray ? static_cast<const T*>(mArray) : nullptr;
    }
    const_iterator cend() const FL_NOEXCEPT {
        return mArray ? static_cast<const T*>(mArray) + mSize : nullptr;
    }

    // ======= SEARCH =======

    iterator find(const T &value) FL_NOEXCEPT {
        for (iterator it = begin(); it != end(); ++it) {
            if (*it == value) return it;
        }
        return end();
    }

    const_iterator find(const T &value) const FL_NOEXCEPT {
        for (const_iterator it = begin(); it != end(); ++it) {
            if (*it == value) return it;
        }
        return end();
    }

    template <typename Predicate> iterator find_if(Predicate pred) FL_NOEXCEPT {
        for (iterator it = begin(); it != end(); ++it) {
            if (pred(*it)) return it;
        }
        return end();
    }

    bool has(const T &value) const FL_NOEXCEPT { return find(value) != end(); }

    // ======= ERASE =======

    // Standard STL-compatible erase that returns iterator to next element
    iterator erase(iterator pos) FL_NOEXCEPT {
        if (pos == end() || empty()) return end();
        fl::size index = pos - begin();
        erase_impl(index);
        return begin() + index;
    }

    // Extended erase with optional output parameter
    bool erase(iterator pos, T *out_value) FL_NOEXCEPT {
        if (pos == end() || empty()) return false;
        if (out_value) {
            *out_value = fl::move(*pos);
        }
        erase(pos);
        return true;
    }

    void erase(const T &value) FL_NOEXCEPT {
        iterator it = find(value);
        if (it != end()) {
            erase(it);
        }
    }

    // Range erase: remove count elements starting at first
    void erase_range(iterator first, fl::size count) FL_NOEXCEPT {
        if (count == 0 || first >= end()) return;
        fl::size index = first - begin();
        if (index + count > mSize) count = mSize - index;
        erase_range_impl(index, count);
    }

    // ======= INSERT =======

    bool insert(iterator pos, const T &value) FL_NOEXCEPT {
        fl::size index = pos - begin();
        fl::size old_size = mSize;
        insert_copy_impl(index, &value);
        return mSize > old_size;
    }

    bool insert(iterator pos, T &&value) FL_NOEXCEPT {
        fl::size index = pos - begin();
        fl::size old_size = mSize;
        insert_move_impl(index, &value);
        return mSize > old_size;
    }

    // Range insert: insert elements from [first, last) before pos
    template <typename InputIt>
    iterator insert(iterator pos, InputIt first, InputIt last) FL_NOEXCEPT {
        fl::size target_idx = pos - begin();
        fl::size count = 0;
        for (InputIt it = first; it != last; ++it) {
            push_back(*it);
            ++count;
        }
        if (count == 0) {
            return begin() + target_idx;
        }
        // Rotate new elements into place via bubble swaps
        fl::size src_start = mSize - count;
        for (fl::size i = 0; i < count; ++i) {
            for (fl::size j = src_start + i; j > target_idx + i; --j) {
                fl::swap(static_cast<T*>(mArray)[j - 1],
                         static_cast<T*>(mArray)[j]);
            }
        }
        return begin() + target_idx;
    }

    // ======= SWAP =======

    void swap(vector &other) FL_NOEXCEPT { swap_impl(other); }
    void swap(vector &&other) FL_NOEXCEPT { swap_impl(other); }

    void swap(iterator a, iterator b) FL_NOEXCEPT {
        fl::swap(*a, *b);
    }

    // ======= COMPARISON =======

    bool operator==(const vector &other) const FL_NOEXCEPT {
        if (size() != other.size()) return false;
        const T* a = static_cast<const T*>(mArray);
        const T* b = static_cast<const T*>(other.mArray);
        for (fl::size i = 0; i < size(); ++i) {
            if (a[i] != b[i]) return false;
        }
        return true;
    }

    bool operator!=(const vector &other) const FL_NOEXCEPT { return !(*this == other); }

    bool operator<(const vector &other) const FL_NOEXCEPT {
        fl::size min_size = mSize < other.mSize ? mSize : other.mSize;
        const T* a = static_cast<const T*>(mArray);
        const T* b = static_cast<const T*>(other.mArray);
        for (fl::size i = 0; i < min_size; ++i) {
            if (a[i] < b[i]) return true;
            if (a[i] > b[i]) return false;
        }
        return mSize < other.mSize;
    }

    bool operator<=(const vector &other) const FL_NOEXCEPT {
        return *this < other || *this == other;
    }

    bool operator>(const vector &other) const FL_NOEXCEPT { return other < *this; }

    bool operator>=(const vector &other) const FL_NOEXCEPT {
        return *this > other || *this == other;
    }

  protected:
    // For VectorN — constructor with inline buffer
    vector(void* inlineBuffer, fl::size inlineCapacity)
        : vector_basic(inlineBuffer, inlineCapacity, sizeof(T),
                        default_memory_resource(), vector_element_ops_for<T>()) {}

    vector(void* inlineBuffer, fl::size inlineCapacity, memory_resource* resource)
        : vector_basic(inlineBuffer, inlineCapacity, sizeof(T),
                        resource, vector_element_ops_for<T>()) {}
};

///////////////////////////////////////////////////////////////////////////////
// VectorN<T, N> — Vector with inline buffer (replaces InlinedVector).
// Like StrN<N> for basic_string: provides the inline buffer storage.
// When size <= N, data lives inline. When size > N, spills to heap.
// Uses the offset trick from basic_string for trivial relocatability.
///////////////////////////////////////////////////////////////////////////////

template <typename T, fl::size INLINED_SIZE>
class FL_ALIGN VectorN : public vector<T> {
  private:
    // Raw aligned storage — address is valid even before initialization,
    // same pattern as StrN<N>::mInlineBuffer in basic_string.
    FL_ALIGNAS(alignof(T) > alignof(fl::uptr) ? alignof(T) : alignof(fl::uptr))
    char mInlineBuffer[INLINED_SIZE * sizeof(T)] = {};

    T* inline_memory() { return fl::bit_cast<T*>(mInlineBuffer); }

  public:
    using typename vector<T>::iterator;
    using typename vector<T>::const_iterator;
    using typename vector<T>::value_type;

    VectorN()
        : vector<T>(mInlineBuffer, INLINED_SIZE) {}

    explicit VectorN(memory_resource* resource)
        : vector<T>(mInlineBuffer, INLINED_SIZE, resource) {}

    VectorN(fl::size count, const T& value = T())
        : vector<T>(mInlineBuffer, INLINED_SIZE) {
        this->resize_value_impl(count, &value);
    }

    VectorN(const VectorN& other)
        : vector<T>(mInlineBuffer, INLINED_SIZE) {
        this->copy_from(other);
    }

    template <fl::size M>
    VectorN(const VectorN<T, M>& other)
        : vector<T>(mInlineBuffer, INLINED_SIZE) {
        this->copy_from(other);
    }

    VectorN(const vector<T>& other)
        : vector<T>(mInlineBuffer, INLINED_SIZE) {
        this->copy_from(other);
    }

    VectorN(VectorN&& other) FL_NOEXCEPT
        : vector<T>(mInlineBuffer, INLINED_SIZE) {
        this->move_from(other);
    }

    VectorN(fl::initializer_list<T> init)
        : vector<T>(mInlineBuffer, INLINED_SIZE) {
        this->reserve_impl(init.size());
        for (const auto& value : init) {
            this->push_back(value);
        }
    }

    VectorN& operator=(const VectorN& other) {
        if (this != &other) {
            this->copy_from(other);
        }
        return *this;
    }

    VectorN& operator=(VectorN&& other) FL_NOEXCEPT {
        if (this != &other) {
            this->move_assign(other);
        }
        return *this;
    }
};

///////////////////////////////////////////////////////////////////////////////
// vector_psram<T> — Vector that allocates from PSRAM by default.
///////////////////////////////////////////////////////////////////////////////

template <typename T>
class vector_psram : public vector<T> {
  public:
    vector_psram()
        : vector<T>(psram_memory_resource()) {}

    vector_psram(fl::size count, const T& value = T())
        : vector<T>(count, value, psram_memory_resource()) {}

    vector_psram(fl::initializer_list<T> init)
        : vector<T>(psram_memory_resource()) {
        this->reserve_impl(init.size());
        for (const auto& value : init) {
            this->push_back(value);
        }
    }

    // Iterator-range constructor
    template <typename InputIterator,
              typename = fl::enable_if_t<!fl::is_integral<InputIterator>::value>>
    vector_psram(InputIterator first, InputIterator last)
        : vector<T>(psram_memory_resource()) {
        for (auto it = first; it != last; ++it) {
            this->push_back(*it);
        }
    }

    vector_psram(const vector_psram& other)
        : vector<T>(psram_memory_resource()) {
        this->copy_from(other);
    }

    vector_psram(vector_psram&& other) FL_NOEXCEPT
        : vector<T>(psram_memory_resource()) {
        this->move_from(other);
    }

    vector_psram& operator=(const vector_psram& other) {
        if (this != &other) {
            this->copy_from(other);
        }
        return *this;
    }

    vector_psram& operator=(vector_psram&& other) FL_NOEXCEPT {
        if (this != &other) {
            this->move_assign(other);
        }
        return *this;
    }
};

///////////////////////////////////////////////////////////////////////////////
// SortedHeapVector — sorted wrapper around vector<T>
///////////////////////////////////////////////////////////////////////////////

template <typename T, typename LessThan = fl::less<T>>
class FL_ALIGN SortedHeapVector {
  private:
    vector<T> mArray;
    LessThan mLess;
    fl::size mMaxSize = fl::size(-1);

  public:
    enum insert_result { inserted = 0, exists = 1, at_capacity = 2 };

    typedef T value_type;
    typedef typename vector<T>::iterator iterator;
    typedef typename vector<T>::const_iterator const_iterator;
    typedef typename vector<T>::reverse_iterator reverse_iterator;
    typedef typename vector<T>::const_reverse_iterator const_reverse_iterator;

    SortedHeapVector(LessThan less = LessThan()) FL_NOEXCEPT : mLess(less) {}

    // Copy constructor
    SortedHeapVector(const SortedHeapVector& other) = default;

    // Copy assignment
    SortedHeapVector& operator=(const SortedHeapVector& other) = default;

    // Move constructor
    SortedHeapVector(SortedHeapVector&& other) FL_NOEXCEPT
        : mArray(fl::move(other.mArray))
        , mLess(fl::move(other.mLess))
        , mMaxSize(other.mMaxSize) {
        other.mMaxSize = fl::size(-1);
    }

    // Move assignment operator
    SortedHeapVector& operator=(SortedHeapVector&& other) FL_NOEXCEPT {
        if (this != &other) {
            mArray = fl::move(other.mArray);
            mLess = fl::move(other.mLess);
            mMaxSize = other.mMaxSize;
            other.mMaxSize = fl::size(-1);
        }
        return *this;
    }

    void setMaxSize(fl::size n) FL_NOEXCEPT {
        if (mMaxSize == n) return;
        mMaxSize = n;
        const bool needs_adjustment = mArray.size() > mMaxSize;
        if (needs_adjustment) {
            mArray.resize(n);
        } else {
            mArray.reserve(n);
        }
    }

    ~SortedHeapVector() FL_NOEXCEPT { mArray.clear(); }

    void reserve(fl::size n) FL_NOEXCEPT { mArray.reserve(n); }

    // Insert while maintaining sort order
    bool insert(const T &value, insert_result *result = nullptr) FL_NOEXCEPT {
        iterator pos = lower_bound(value);
        if (pos != end() && !mLess(value, *pos) && !mLess(*pos, value)) {
            if (result) *result = exists;
            return false;
        }
        if (mArray.size() >= mMaxSize) {
            if (result) *result = at_capacity;
            return false;
        }
        mArray.insert(pos, value);
        if (result) *result = inserted;
        return true;
    }

    iterator lower_bound(const T &value) FL_NOEXCEPT {
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

    const_iterator lower_bound(const T &value) const FL_NOEXCEPT {
        return const_cast<SortedHeapVector *>(this)->lower_bound(value);
    }

    iterator find(const T &value) FL_NOEXCEPT {
        iterator pos = lower_bound(value);
        if (pos != end() && !mLess(value, *pos) && !mLess(*pos, value)) {
            return pos;
        }
        return end();
    }

    void swap(SortedHeapVector &other) FL_NOEXCEPT { mArray.swap(other.mArray); }

    const_iterator find(const T &value) const FL_NOEXCEPT {
        return const_cast<SortedHeapVector *>(this)->find(value);
    }

    bool has(const T &value) const FL_NOEXCEPT { return find(value) != end(); }

    bool erase(const T &value) FL_NOEXCEPT {
        iterator it = find(value);
        if (it != end()) {
            mArray.erase(it);
            return true;
        }
        return false;
    }

    iterator erase(iterator pos) FL_NOEXCEPT { return mArray.erase(pos); }

    fl::size size() const FL_NOEXCEPT { return mArray.size(); }
    bool empty() const FL_NOEXCEPT { return mArray.empty(); }
    fl::size capacity() const FL_NOEXCEPT { return mArray.capacity(); }

    void clear() FL_NOEXCEPT { mArray.clear(); }
    bool full() const FL_NOEXCEPT {
        if (mArray.size() >= mMaxSize) return true;
        return mArray.full();
    }

    T &operator[](fl::size index) FL_NOEXCEPT { return mArray[index]; }
    const T &operator[](fl::size index) const FL_NOEXCEPT { return mArray[index]; }

    T &front() FL_NOEXCEPT { return mArray.front(); }
    const T &front() const FL_NOEXCEPT { return mArray.front(); }

    T &back() FL_NOEXCEPT { return mArray.back(); }
    const T &back() const FL_NOEXCEPT { return mArray.back(); }

    iterator begin() FL_NOEXCEPT { return mArray.begin(); }
    const_iterator begin() const FL_NOEXCEPT { return mArray.begin(); }
    iterator end() FL_NOEXCEPT { return mArray.end(); }
    const_iterator end() const FL_NOEXCEPT { return mArray.end(); }

    reverse_iterator rbegin() FL_NOEXCEPT { return mArray.rbegin(); }
    const_reverse_iterator rbegin() const FL_NOEXCEPT { return mArray.rbegin(); }
    reverse_iterator rend() FL_NOEXCEPT { return mArray.rend(); }
    const_reverse_iterator rend() const FL_NOEXCEPT { return mArray.rend(); }

    // Raw data access
    T *data() FL_NOEXCEPT { return mArray.data(); }
    const T *data() const FL_NOEXCEPT { return mArray.data(); }
};

///////////////////////////////////////////////////////////////////////////////
// Type aliases
///////////////////////////////////////////////////////////////////////////////

template <typename T, fl::size INLINED_SIZE>
using vector_fixed = FixedVector<T, INLINED_SIZE>;

template <typename T, fl::size INLINED_SIZE = 64>
using vector_inlined = VectorN<T, INLINED_SIZE>;

// Backward compat: InlinedVector is now VectorN
template <typename T, fl::size INLINED_SIZE>
using InlinedVector = VectorN<T, INLINED_SIZE>;

} // namespace fl
