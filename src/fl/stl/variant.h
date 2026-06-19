#pragma once

#include "fl/stl/cstddef.h"       // for size_t
#include "fl/stl/cstring.h"        // for fl::memcpy (trivial copy/move thunks)
#include "fl/stl/int.h"               // for u8
#include "fl/stl/type_traits.h" // for fl::enable_if, fl::is_same, etc.
#include "fl/stl/bit_cast.h"    // for safe type-punning
#include "fl/stl/move.h"          // for move
#include "fl/stl/new.h"          // for placement new operator (must be before namespace fl)  // IWYU pragma: keep
#include "fl/stl/align.h"        // for FL_ALIGN_AS_T macro (GCC 4.8.3 workaround)
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"

namespace fl {

namespace detail {

// =============================================================================
// variant lifecycle thunks (FastLED #3249 / #3244 Tier 3G)
// =============================================================================
//
// Each (T x op) pair in `fl::variant<Ts...>` previously emitted its own
// per-instantiation thunk -- on the json_value 10-alternative variant the
// audit catalogued ~245 thunks totalling ~6.7 KB on LPC845.
//
// The collapse:
//   1. Thunks now live at namespace scope (`fl::detail`) so two distinct
//      `variant<Ts...>` instantiations that share a type `T` also share
//      `typed_destroy_thunk<T>`, `typed_copy_thunk<T>`, `typed_move_thunk<T>`.
//      Previously each variant's outer template params re-mangled the symbol.
//   2. Trivially-copyable T's route through a shared `noop_destroy` and
//      per-size `trivial_copy_n<sizeof(T)>` / `trivial_move_n<sizeof(T)>`
//      thunks. All `bool`, `i64`, `float`, `nullptr_t`, etc. alternatives
//      collapse to one no-op + a handful of memcpy bodies keyed on size,
//      regardless of how many variant types ride on them.
//
// `is_trivially_copyable<T>` is used as the gate for both the destroy AND
// copy/move collapses: per the C++ standard, trivially-copyable implies
// trivially-destructible, so this is strictly correct (and a touch more
// conservative than the issue body's `is_trivially_destructible<T>` --
// we don't have that trait in `fl/stl/type_traits.h` and the
// conservative variant captures every json_value primitive alternative).
//
// `visit_fn<T, Visitor>` stays per-instantiation -- it encodes T-specific
// dispatch into the visitor's `operator()(T const&)` and the collapse
// would change behavior. See `variant<Ts...>::visit_fn` below.

using variant_destroy_fn = void (*)(void* /*storage*/);
using variant_copy_fn    = void (*)(void* /*dst*/, const void* /*src*/);
using variant_move_fn    = void (*)(void* /*dst*/, void* /*src*/);

// -- per-T thunks (used when T is not trivially-copyable) ---------------------

template <typename T>
void typed_destroy_thunk(void* storage) FL_NOEXCEPT {
    fl::bit_cast_ptr<T>(storage)->~T();
}

template <typename T>
void typed_copy_thunk(void* dst, const void* src) FL_NOEXCEPT {
    const T* source = fl::bit_cast_ptr<const T>(src);
    new (dst) T(*source);
}

template <typename T>
void typed_move_thunk(void* dst, void* src) FL_NOEXCEPT {
    T* source = fl::bit_cast_ptr<T>(src);
    new (dst) T(fl::move(*source));
}

// -- shared thunks for trivially-copyable types -------------------------------

inline void variant_noop_destroy(void* /*storage*/) FL_NOEXCEPT {}

template <fl::size N>
void variant_trivial_copy_n(void* dst, const void* src) FL_NOEXCEPT {
    fl::memcpy(dst, src, N);
}

template <fl::size N>
void variant_trivial_move_n(void* dst, void* src) FL_NOEXCEPT {
    fl::memcpy(dst, src, N);
}

// -- per-T compile-time pickers (one symbol per Sig, populates the variant's
// static dispatch table at startup) ------------------------------------------

template <typename T>
constexpr variant_destroy_fn pick_variant_destroy_fn() FL_NOEXCEPT {
    return is_trivially_copyable<T>::value
        ? &variant_noop_destroy
        : &typed_destroy_thunk<T>;
}

template <typename T>
constexpr variant_copy_fn pick_variant_copy_fn() FL_NOEXCEPT {
    return is_trivially_copyable<T>::value
        ? &variant_trivial_copy_n<sizeof(T)>
        : &typed_copy_thunk<T>;
}

template <typename T>
constexpr variant_move_fn pick_variant_move_fn() FL_NOEXCEPT {
    return is_trivially_copyable<T>::value
        ? &variant_trivial_move_n<sizeof(T)>
        : &typed_move_thunk<T>;
}

} // namespace detail

// A variant that can hold any of N different types
template <typename... Types>
class FL_ALIGN_AS_T(max_align<Types...>::value) variant {
  public:
    using Tag = u8;
    static constexpr Tag Empty = 0;

    // –– ctors/dtors/assign as before …

    variant() FL_NOEXCEPT : _tag(Empty) {}

    template <typename T, typename = typename fl::enable_if<
                              contains_type<T, Types...>::value>::type>
    variant(const T &value) FL_NOEXCEPT : _tag(Empty) {
        construct<T>(value);
    }

    template <typename T, typename = typename fl::enable_if<
                              contains_type<T, Types...>::value>::type>
    variant(T &&value) FL_NOEXCEPT : _tag(Empty) {
        construct<T>(fl::move(value));
    }

    variant(const variant &other) FL_NOEXCEPT : _tag(Empty) {
        if (!other.empty()) {
            copy_construct_from(other);
        }
    }

    variant(variant &&other) FL_NOEXCEPT : _tag(Empty) {
        if (!other.empty()) {
            move_construct_from(other);
            // After moving, mark other as empty to prevent destructor calls on moved-from objects
            other._tag = Empty;
        }
    }

    ~variant() FL_NOEXCEPT { reset(); }

    variant &operator=(const variant &other) FL_NOEXCEPT {
        if (this != &other) {
            reset();
            if (!other.empty()) {
                copy_construct_from(other);
            }
        }
        return *this;
    }

    variant &operator=(variant &&other) FL_NOEXCEPT {
        if (this != &other) {
            reset();
            if (!other.empty()) {
                move_construct_from(other);
                // After moving, mark other as empty to prevent destructor calls on moved-from objects
                other._tag = Empty;
            }
        }
        return *this;
    }

    template <typename T, typename = typename fl::enable_if<
                              contains_type<T, Types...>::value>::type>
    variant &operator=(const T &value) FL_NOEXCEPT {
        // Copy first to handle self-referential values (e.g. assigning
        // from a reference into this variant's own storage)
        T value_copy(value);
        reset();
        construct<T>(fl::move(value_copy));
        return *this;
    }

    template <typename T, typename = typename fl::enable_if<
                              contains_type<T, Types...>::value>::type>
    variant &operator=(T &&value) FL_NOEXCEPT {
        reset();
        construct<T>(fl::move(value));
        return *this;
    }

    // –– modifiers, observers, ptr/get, etc. unchanged …

    template <typename T, typename... Args>
    typename fl::enable_if<contains_type<T, Types...>::value, T &>::type
    emplace(Args &&...args) FL_NOEXCEPT {
        reset();
        construct<T>(fl::forward<Args>(args)...);
        return *ptr<T>();
    }

    void reset() FL_NOEXCEPT {
        if (!empty()) {
            destroy_current();
            _tag = Empty;
        }
    }

    Tag tag() const FL_NOEXCEPT { return _tag; }
    bool empty() const FL_NOEXCEPT { return _tag == Empty; }

    template <typename T> bool is() const FL_NOEXCEPT {
        return _tag == type_to_tag<T>();
    }

    template <typename T> T *ptr() FL_NOEXCEPT {
        if (!is<T>()) return nullptr;
        // Use bit_cast_ptr for safe type-punning on properly aligned storage
        // The storage is guaranteed to be properly aligned by alignas(max_align<Types...>::value)
        return fl::bit_cast_ptr<T>(&_storage[0]);
    }

    template <typename T> const T *ptr() const FL_NOEXCEPT {
        if (!is<T>()) return nullptr;
        // Use bit_cast_ptr for safe type-punning on properly aligned storage
        // The storage is guaranteed to be properly aligned by alignas(max_align<Types...>::value)
        return fl::bit_cast_ptr<const T>(&_storage[0]);
    }

    /// @brief Get a reference to the stored value of type T
    /// @tparam T The type to retrieve
    /// @return Reference to the stored value
    /// @note Asserts if the variant doesn't contain type T. Use is<T>() to check first.
    /// @warning Will crash if called with wrong type - this is intentional for fast failure
    template <typename T> T &get() FL_NOEXCEPT {
        // Dereference ptr() directly - will crash with null pointer access if wrong type
        // This provides fast failure semantics similar to std::variant
        return *ptr<T>();
    }

    /// @brief Get a const reference to the stored value of type T
    /// @tparam T The type to retrieve  
    /// @return Const reference to the stored value
    /// @note Asserts if the variant doesn't contain type T. Use is<T>() to check first.
    /// @warning Will crash if called with wrong type - this is intentional for fast failure
    template <typename T> const T &get() const FL_NOEXCEPT {
        // Dereference ptr() directly - will crash with null pointer access if wrong type
        // This provides fast failure semantics similar to std::variant
        return *ptr<T>();
    }

    template <typename T> bool equals(const T &other) const FL_NOEXCEPT {
        if (auto p = ptr<T>()) {
            return *p == other;
        }
        return false;
    }

    // –– visitor using O(1) function‐pointer table
    template <typename Visitor> void visit(Visitor &visitor) FL_NOEXCEPT {
        if (_tag == Empty)
            return;

        // Fn is "a pointer to function taking (void* storage, Visitor&)"
        using Fn = void (*)(void *, Visitor &);

        // Build a constexpr array of one thunk per type in Types...
        // Each thunk casts the storage back to the right T* and calls
        // visitor.accept
        static constexpr Fn table[] = {
            &variant::template visit_fn<Types, Visitor>...};

        // _tag is 1-based, so dispatch in O(1) via one indirect call:
        // Check bounds to prevent out-of-bounds access
        size_t index = _tag - 1;
        if (index < sizeof...(Types)) {
            table[index](&_storage, visitor);
        }
    }

    template <typename Visitor> void visit(Visitor &visitor) const FL_NOEXCEPT {
        if (_tag == Empty)
            return;

        // Fn is "a pointer to function taking (const void* storage, Visitor&)"
        using Fn = void (*)(const void *, Visitor &);

        // Build a constexpr array of one thunk per type in Types...
        static constexpr Fn table[] = {
            &variant::template visit_fn_const<Types, Visitor>...};

        // _tag is 1-based, so dispatch in O(1) via one indirect call:
        // Check bounds to prevent out-of-bounds access
        size_t index = _tag - 1;
        if (index < sizeof...(Types)) {
            table[index](&_storage, visitor);
        }
    }

  private:
    // –– helper for the visit table
    template <typename T, typename Visitor>
    static void visit_fn(void *storage, Visitor &v) FL_NOEXCEPT {
        // Use bit_cast_ptr for safe type-punning on properly aligned storage
        // The storage is guaranteed to be properly aligned by alignas(max_align<Types...>::value)
        T* typed_ptr = fl::bit_cast_ptr<T>(storage);
        v.accept(*typed_ptr);
    }

    template <typename T, typename Visitor>
    static void visit_fn_const(const void *storage, Visitor &v) FL_NOEXCEPT {
        // Use bit_cast_ptr for safe type-punning on properly aligned storage
        // The storage is guaranteed to be properly aligned by alignas(max_align<Types...>::value)
        const T* typed_ptr = fl::bit_cast_ptr<const T>(storage);
        v.accept(*typed_ptr);
    }

    // –– destroy via table -- dispatches through fl::detail::pick_variant_destroy_fn,
    //    which collapses trivially-copyable T's to the shared `variant_noop_destroy`
    //    (one symbol across every variant<...> in the link). See #3249 header.
    void destroy_current() FL_NOEXCEPT {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING(array-bounds)
        static constexpr detail::variant_destroy_fn table[] = {
            detail::pick_variant_destroy_fn<Types>()...
        };
        if (_tag != Empty) {
            table[_tag - 1](&_storage);
        }
        FL_DISABLE_WARNING_POP
    }

    // –– copy‐construct via table -- collapses trivially-copyable T's to the
    //    shared per-size `variant_trivial_copy_n<sizeof(T)>` thunk. See #3249.
    void copy_construct_from(const variant &other) FL_NOEXCEPT {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING(array-bounds)
        static constexpr detail::variant_copy_fn table[] = {
            detail::pick_variant_copy_fn<Types>()...
        };
        table[other._tag - 1](&_storage, &other._storage[0]);
        FL_DISABLE_WARNING_POP
        _tag = other._tag;
    }

    // –– move‐construct via table -- collapses trivially-copyable T's to the
    //    shared per-size `variant_trivial_move_n<sizeof(T)>` thunk. See #3249.
    void move_construct_from(variant &other) FL_NOEXCEPT {
        FL_DISABLE_WARNING_PUSH
        FL_DISABLE_WARNING(array-bounds)
        static constexpr detail::variant_move_fn table[] = {
            detail::pick_variant_move_fn<Types>()...
        };
        table[other._tag - 1](&_storage, &other._storage[0]);
        FL_DISABLE_WARNING_POP
        _tag = other._tag;
        other.reset();
    }

    // –– everything below here (type_traits, construct<T>, type_to_tag,
    // storage)
    //    stays exactly as you wrote it:

    // … max_size, max_align, contains_type, type_to_tag_impl, etc. …

    // Helper to map a type to its tag value
    template <typename T> static constexpr Tag type_to_tag() FL_NOEXCEPT {
        return type_to_tag_impl<T, Types...>::value;
    }

    // Implementation details for type_to_tag
    template <typename T, typename... Ts> struct type_to_tag_impl;

    template <typename T> struct type_to_tag_impl<T> {
        static constexpr Tag value = 0; // Not found
    };

    template <typename T, typename U, typename... Rest>
    struct type_to_tag_impl<T, U, Rest...> {
        static constexpr Tag value =
            fl::is_same<T, U>::value
                ? 1
                : (type_to_tag_impl<T, Rest...>::value == 0
                       ? 0
                       : type_to_tag_impl<T, Rest...>::value + 1);
    };

    template <typename T, typename... Args> void construct(Args &&...args) FL_NOEXCEPT {
        new (&_storage) T(fl::forward<Args>(args)...);
        _tag = type_to_tag<T>();
    }

    FL_ALIGN_AS_T(max_align<Types...>::value) char _storage[max_size<Types...>::value];

    Tag _tag;
};

} // namespace fl
