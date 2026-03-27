#pragma once

/// @file basic_vector.h
/// @brief Type-erased base class for fl::vector<T>.
/// All vector logic lives here, compiled once. vector<T> is a thin wrapper
/// that provides type safety and sets up the element operations table.
/// Follows the same pattern as basic_string / StrN<N>.

#include "fl/stl/int.h"
#include "fl/stl/cstring.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/memory_resource.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// Function pointer table for type-specific element operations.
/// One static instance per type T, shared by all vectors of that T.
/// nullptr means the type is trivially copyable — use memcpy/memmove.
struct vector_element_ops {
    void (*copy_construct)(void* dst, const void* src);
    void (*move_construct)(void* dst, void* src);
    void (*destroy)(void* ptr);
    void (*default_construct)(void* ptr);
    void (*swap_elements)(void* a, void* b);

    /// Move-construct count elements from src to dst (non-overlapping, dst uninitialized).
    void (*uninitialized_move_n)(void* dst, void* src, fl::size count);

    /// Destroy count elements starting at first.
    void (*destroy_n)(void* first, fl::size count);
};

/// Type-erased vector base class. Holds all vector logic.
/// vector<T> inherits this and provides the element type info.
/// VectorN<T, N> inherits vector<T> and provides the inline buffer.
/// This class cannot be constructed directly — only through vector<T>.
class vector_basic {
  public:
    // ======= CAPACITY =======
    fl::size size() const { return mSize; }
    bool empty() const { return mSize == 0; }
    fl::size capacity() const { return mCapacity; }
    bool full() const { return mSize >= mCapacity; }
    fl::size element_size() const { return mElementSize; }

    // ======= RAW DATA ACCESS =======
    void* data_raw() { return mArray; }
    const void* data_raw() const { return mArray; }

    // ======= MEMORY MANAGEMENT =======
    void reserve_impl(fl::size n);
    void shrink_to_fit_impl();

    // ======= ELEMENT OPERATIONS =======
    void push_back_copy_impl(const void* element);
    void push_back_move_impl(void* element);
    void pop_back_impl();
    void clear_impl();

    /// Erase element at index. Shifts subsequent elements left.
    void erase_impl(fl::size index);

    /// Erase range [first_index, first_index + count).
    void erase_range_impl(fl::size first_index, fl::size count);

    /// Insert element at index by copy. Shifts subsequent elements right.
    void insert_copy_impl(fl::size index, const void* element);

    /// Insert element at index by move. Shifts subsequent elements right.
    void insert_move_impl(fl::size index, void* element);

    /// Resize to n elements. New elements are default-constructed (zeroed for trivial).
    void resize_impl(fl::size n);

    /// Resize to n elements. New elements are copy-constructed from value.
    void resize_value_impl(fl::size n, const void* value);

    /// Swap contents with another vector_basic.
    void swap_impl(vector_basic& other);

    // ======= DESTRUCTOR =======
    ~vector_basic() FL_NOEXCEPT;

  protected:
    // ======= CONSTRUCTION (only callable by vector<T>) =======

    /// Heap-only vector (no inline buffer).
    vector_basic(fl::size elementSize, memory_resource* resource,
                 const vector_element_ops* ops) FL_NOEXCEPT
        : mElementSize(elementSize)
        , mResource(resource)
        , mOps(ops)
        , mInlineOffset(0)
        , mInlineCapacity(0) {}

    /// Vector with inline buffer (for VectorN).
    vector_basic(void* inlineBuffer, fl::size inlineCapacity,
                 fl::size elementSize, memory_resource* resource,
                 const vector_element_ops* ops)
        : mElementSize(elementSize)
        , mResource(resource)
        , mOps(ops)
        , mInlineOffset(static_cast<fl::size>(
              static_cast<char*>(inlineBuffer) -
              static_cast<char*>(static_cast<void*>(this))))
        , mInlineCapacity(inlineCapacity) {
        // Start with data in inline buffer
        mArray = inlineBuffer;
        mCapacity = inlineCapacity;
    }

    // Deleted copy/move — vector<T> handles these
    vector_basic(const vector_basic&) FL_NOEXCEPT = delete;
    vector_basic(vector_basic&&) FL_NOEXCEPT = delete;
    vector_basic& operator=(const vector_basic&) FL_NOEXCEPT = delete;
    vector_basic& operator=(vector_basic&&) FL_NOEXCEPT = delete;

    // ======= HELPERS FOR DERIVED CLASSES =======

    /// Copy all elements from another vector_basic.
    void copy_from(const vector_basic& other) FL_NOEXCEPT;

    /// Move-steal contents from another vector_basic.
    void move_from(vector_basic& other) FL_NOEXCEPT;

    /// Move-assign from another vector_basic (clears this first).
    void move_assign(vector_basic& other) FL_NOEXCEPT;

    // ======= DATA MEMBERS =======
    void* mArray = nullptr;
    fl::size mSize = 0;
    fl::size mCapacity = 0;
    fl::size mElementSize;
    memory_resource* mResource;
    const vector_element_ops* mOps;  // nullptr → trivially copyable

    // Inline buffer support (offset trick from basic_string).
    // Store offset from `this` to the inline buffer, not a raw pointer.
    // This survives trivial relocation (bitwise copy by containers) because
    // the offset is relative to `this` which updates with the object.
    fl::size mInlineOffset;
    fl::size mInlineCapacity;  // 0 = no inline buffer

    // ======= HELPER METHODS =======

    /// Compute inline buffer pointer from offset.
    void* inlineBufferPtr() {
        return static_cast<char*>(static_cast<void*>(this)) + mInlineOffset;
    }
    const void* inlineBufferPtr() const {
        return static_cast<const char*>(static_cast<const void*>(this)) + mInlineOffset;
    }

    /// Is data currently in the inline buffer?
    bool isInline() const {
        return mInlineCapacity > 0 && mArray == inlineBufferPtr();
    }

    /// Does this vector have an inline buffer at all?
    bool hasInlineBuffer() const {
        return mInlineCapacity > 0;
    }

    /// Pointer to element at index (byte arithmetic).
    void* element_ptr(fl::size index) {
        return static_cast<char*>(mArray) + index * mElementSize;
    }
    const void* element_ptr(fl::size index) const {
        return static_cast<const char*>(mArray) + index * mElementSize;
    }

  private:
    /// Ensure capacity for at least n elements. Grows if needed.
    void ensure_capacity(fl::size n);

    /// Grow to new_capacity. Moves existing elements.
    void grow_to(fl::size new_capacity);

    // ======= TRIVIAL ELEMENT HELPERS (memcpy/memmove) =======
    void trivial_copy(void* dst, const void* src, fl::size count) const;
    void trivial_move_left(void* dst, const void* src, fl::size count) const;
    void trivial_default_construct(void* ptr, fl::size count) const;
    void trivial_swap(void* a, void* b) const;
};

// ======= OPS TABLE HELPERS =======

namespace detail {

// SFINAE: detect if T is default-constructible
template <typename T, typename = void>
struct has_default_ctor : fl::false_type {};

template <typename T>
struct has_default_ctor<T, decltype(void(T()))> : fl::true_type {};

// SFINAE: detect if T is copy-constructible
template <typename T, typename = void>
struct has_copy_ctor : fl::false_type {};

template <typename T>
struct has_copy_ctor<T, decltype(void(T(fl::declval<const T&>())))> : fl::true_type {};

// Default construct: available when T has default ctor
template <typename T>
typename fl::enable_if<has_default_ctor<T>::value, void(*)(void*)>::type
get_default_construct_fn() {
    return [](void* ptr) { new (ptr) T(); };
}

// Default construct: nullptr when T has no default ctor
template <typename T>
typename fl::enable_if<!has_default_ctor<T>::value, void(*)(void*)>::type
get_default_construct_fn() {
    return nullptr;
}

// Copy construct: available when T has copy ctor
template <typename T>
typename fl::enable_if<has_copy_ctor<T>::value, void(*)(void*, const void*)>::type
get_copy_construct_fn() {
    return [](void* dst, const void* src) {
        new (dst) T(*static_cast<const T*>(src));
    };
}

// Copy construct: nullptr when T is not copyable
template <typename T>
typename fl::enable_if<!has_copy_ctor<T>::value, void(*)(void*, const void*)>::type
get_copy_construct_fn() {
    return nullptr;
}

// SFINAE: detect if T is move-constructible
template <typename T, typename = void>
struct has_move_ctor : fl::false_type {};

template <typename T>
struct has_move_ctor<T, decltype(void(T(fl::declval<T&&>())))> : fl::true_type {};

// SFINAE: detect if T is swappable (needs move-constructible + move-assignable)
template <typename T, typename = void>
struct is_swappable : fl::false_type {};

template <typename T>
struct is_swappable<T, decltype(void(
    fl::declval<T&>() = fl::declval<T&&>()
))> : has_move_ctor<T> {};

// Move construct: available
template <typename T>
typename fl::enable_if<has_move_ctor<T>::value, void(*)(void*, void*)>::type
get_move_construct_fn() {
    return [](void* dst, void* src) {
        new (dst) T(static_cast<T&&>(*static_cast<T*>(src)));
    };
}

template <typename T>
typename fl::enable_if<!has_move_ctor<T>::value, void(*)(void*, void*)>::type
get_move_construct_fn() {
    return nullptr;
}

// Swap: available when swappable
template <typename T>
typename fl::enable_if<is_swappable<T>::value, void(*)(void*, void*)>::type
get_swap_fn() {
    return [](void* a, void* b) {
        T& ta = *static_cast<T*>(a);
        T& tb = *static_cast<T*>(b);
        T tmp(static_cast<T&&>(ta));
        ta = static_cast<T&&>(tb);
        tb = static_cast<T&&>(tmp);
    };
}

template <typename T>
typename fl::enable_if<!is_swappable<T>::value, void(*)(void*, void*)>::type
get_swap_fn() {
    return nullptr;
}

// Uninitialized move N: available when move-constructible
template <typename T>
typename fl::enable_if<has_move_ctor<T>::value, void(*)(void*, void*, fl::size)>::type
get_uninitialized_move_n_fn() {
    return [](void* dst, void* src, fl::size count) {
        T* d = static_cast<T*>(dst);
        T* s = static_cast<T*>(src);
        for (fl::size i = 0; i < count; ++i) {
            new (&d[i]) T(static_cast<T&&>(s[i]));
        }
    };
}

template <typename T>
typename fl::enable_if<!has_move_ctor<T>::value, void(*)(void*, void*, fl::size)>::type
get_uninitialized_move_n_fn() {
    return nullptr;
}

} // namespace detail

// ======= OPS TABLE GENERATOR =======

/// Generate a static ops table for type T.
/// Returns nullptr for trivially copyable types (fast path).
template <typename T>
const vector_element_ops* vector_element_ops_for() FL_NOEXCEPT {
    // Trivially copyable types use the fast path (mOps == nullptr)
    if (fl::is_trivially_copyable<T>::value) {
        return nullptr;
    }

    static const vector_element_ops ops = {
        detail::get_copy_construct_fn<T>(),       // copy_construct
        detail::get_move_construct_fn<T>(),       // move_construct
        [](void* ptr) { static_cast<T*>(ptr)->~T(); }, // destroy
        detail::get_default_construct_fn<T>(),    // default_construct
        detail::get_swap_fn<T>(),                 // swap_elements
        detail::get_uninitialized_move_n_fn<T>(), // uninitialized_move_n
        // destroy_n
        [](void* first, fl::size count) {
            T* p = static_cast<T*>(first);
            for (fl::size i = 0; i < count; ++i) {
                p[i].~T();
            }
        }
    };
    return &ops;
}

} // namespace fl
