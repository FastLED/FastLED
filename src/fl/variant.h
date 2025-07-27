#pragma once

#include "fl/inplacenew.h"  // for fl::move, fl::forward, in‐place new
#include "fl/type_traits.h" // for fl::enable_if, fl::is_same, etc.
#include "fl/bit_cast.h"    // for safe type-punning

namespace fl {

// A variant that can hold any of N different types
template <typename... Types> 
class alignas(max_align<Types...>::value) Variant {
  public:
    using Tag = u8;
    static constexpr Tag Empty = 0;

    // –– ctors/dtors/assign as before …

    Variant() noexcept : _tag(Empty) {}

    template <typename T, typename = typename fl::enable_if<
                              contains_type<T, Types...>::value>::type>
    Variant(const T &value) : _tag(Empty) {
        construct<T>(value);
    }

    template <typename T, typename = typename fl::enable_if<
                              contains_type<T, Types...>::value>::type>
    Variant(T &&value) : _tag(Empty) {
        construct<T>(fl::move(value));
    }

    Variant(const Variant &other) : _tag(Empty) {
        if (!other.empty()) {
            copy_construct_from(other);
        }
    }

    Variant(Variant &&other) noexcept : _tag(Empty) {
        if (!other.empty()) {
            move_construct_from(other);
            // After moving, mark other as empty to prevent destructor calls on moved-from objects
            other._tag = Empty;
        }
    }

    ~Variant() { reset(); }

    Variant &operator=(const Variant &other) {
        if (this != &other) {
            reset();
            if (!other.empty()) {
                copy_construct_from(other);
            }
        }
        return *this;
    }

    Variant &operator=(Variant &&other) noexcept {
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
    Variant &operator=(const T &value) {
        reset();
        construct<T>(value);
        return *this;
    }

    template <typename T, typename = typename fl::enable_if<
                              contains_type<T, Types...>::value>::type>
    Variant &operator=(T &&value) {
        reset();
        construct<T>(fl::move(value));
        return *this;
    }

    // –– modifiers, observers, ptr/get, etc. unchanged …

    template <typename T, typename... Args>
    typename fl::enable_if<contains_type<T, Types...>::value, T &>::type
    emplace(Args &&...args) {
        reset();
        construct<T>(fl::forward<Args>(args)...);
        return ptr<T>();
    }

    void reset() noexcept {
        if (!empty()) {
            destroy_current();
            _tag = Empty;
        }
    }

    Tag tag() const noexcept { return _tag; }
    bool empty() const noexcept { return _tag == Empty; }

    template <typename T> bool is() const noexcept {
        return _tag == type_to_tag<T>();
    }

    template <typename T> T *ptr() {
        if (!is<T>()) return nullptr;
        // Use bit_cast_ptr for safe type-punning on properly aligned storage
        // The storage is guaranteed to be properly aligned by alignas(max_align<Types...>::value)
        return fl::bit_cast_ptr<T>(&_storage[0]);
    }

    template <typename T> const T *ptr() const {
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
    template <typename T> T &get() {
        // Dereference ptr() directly - will crash with null pointer access if wrong type
        // This provides fast failure semantics similar to std::variant
        return *ptr<T>();
    }

    /// @brief Get a const reference to the stored value of type T
    /// @tparam T The type to retrieve  
    /// @return Const reference to the stored value
    /// @note Asserts if the variant doesn't contain type T. Use is<T>() to check first.
    /// @warning Will crash if called with wrong type - this is intentional for fast failure
    template <typename T> const T &get() const {
        // Dereference ptr() directly - will crash with null pointer access if wrong type
        // This provides fast failure semantics similar to std::variant
        return *ptr<T>();
    }

    template <typename T> bool equals(const T &other) const {
        if (auto p = ptr<T>()) {
            return *p == other;
        }
        return false;
    }

    // –– visitor using O(1) function‐pointer table
    template <typename Visitor> void visit(Visitor &visitor) {
        if (_tag == Empty)
            return;

        // Fn is "a pointer to function taking (void* storage, Visitor&)"
        using Fn = void (*)(void *, Visitor &);

        // Build a constexpr array of one thunk per type in Types...
        // Each thunk casts the storage back to the right T* and calls
        // visitor.accept
        static constexpr Fn table[] = {
            &Variant::template visit_fn<Types, Visitor>...};

        // _tag is 1-based, so dispatch in O(1) via one indirect call:
        // Check bounds to prevent out-of-bounds access
        size_t index = _tag - 1;
        if (index < sizeof...(Types)) {
            table[index](&_storage, visitor);
        }
    }

    template <typename Visitor> void visit(Visitor &visitor) const {
        if (_tag == Empty)
            return;

        // Fn is "a pointer to function taking (const void* storage, Visitor&)"
        using Fn = void (*)(const void *, Visitor &);

        // Build a constexpr array of one thunk per type in Types...
        static constexpr Fn table[] = {
            &Variant::template visit_fn_const<Types, Visitor>...};

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
    static void visit_fn(void *storage, Visitor &v) {
        // Use bit_cast_ptr for safe type-punning on properly aligned storage
        // The storage is guaranteed to be properly aligned by alignas(max_align<Types...>::value)
        T* typed_ptr = fl::bit_cast_ptr<T>(storage);
        v.accept(*typed_ptr);
    }

    template <typename T, typename Visitor>
    static void visit_fn_const(const void *storage, Visitor &v) {
        // Use bit_cast_ptr for safe type-punning on properly aligned storage
        // The storage is guaranteed to be properly aligned by alignas(max_align<Types...>::value)
        const T* typed_ptr = fl::bit_cast_ptr<const T>(storage);
        v.accept(*typed_ptr);
    }

    // –– destroy via table
    void destroy_current() noexcept {
        using Fn = void (*)(void *);
        static constexpr Fn table[] = {&Variant::template destroy_fn<Types>...};
        if (_tag != Empty) {
            table[_tag - 1](&_storage);
        }
    }

    template <typename T> static void destroy_fn(void *storage) {
        // Use bit_cast_ptr for safe type-punning on properly aligned storage
        // The storage is guaranteed to be properly aligned by alignas(max_align<Types...>::value)
        T* typed_ptr = fl::bit_cast_ptr<T>(storage);
        typed_ptr->~T();
    }

    // –– copy‐construct via table
    void copy_construct_from(const Variant &other) {
        using Fn = void (*)(void *, const Variant &);
        static constexpr Fn table[] = {&Variant::template copy_fn<Types>...};
        table[other._tag - 1](&_storage, other);
        _tag = other._tag;
    }

    template <typename T>
    static void copy_fn(void *storage, const Variant &other) {
        // Use bit_cast_ptr for safe type-punning on properly aligned storage
        // The storage is guaranteed to be properly aligned by alignas(max_align<Types...>::value)
        const T* source_ptr = fl::bit_cast_ptr<const T>(&other._storage[0]);
        new (storage) T(*source_ptr);
    }

    // –– move‐construct via table
    void move_construct_from(Variant &other) noexcept {
        using Fn = void (*)(void *, Variant &);
        static constexpr Fn table[] = {&Variant::template move_fn<Types>...};
        table[other._tag - 1](&_storage, other);
        _tag = other._tag;
        other.reset();
    }

    template <typename T> static void move_fn(void *storage, Variant &other) {
        // Use bit_cast_ptr for safe type-punning on properly aligned storage
        // The storage is guaranteed to be properly aligned by alignas(max_align<Types...>::value)
        T* source_ptr = fl::bit_cast_ptr<T>(&other._storage[0]);
        new (storage) T(fl::move(*source_ptr));
    }

    // –– everything below here (type_traits, construct<T>, type_to_tag,
    // storage)
    //    stays exactly as you wrote it:

    // … max_size, max_align, contains_type, type_to_tag_impl, etc. …

    // Helper to map a type to its tag value
    template <typename T> static constexpr Tag type_to_tag() {
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

    template <typename T, typename... Args> void construct(Args &&...args) {
        new (&_storage) T(fl::forward<Args>(args)...);
        _tag = type_to_tag<T>();
    }

    alignas(
        max_align<Types...>::value) char _storage[max_size<Types...>::value];

    Tag _tag;
};

} // namespace fl
