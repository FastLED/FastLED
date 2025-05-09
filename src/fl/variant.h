#pragma once

#include "fl/inplacenew.h"
#include "fl/template_magic.h"

namespace fl {

// A variant that can hold any of N different types
template <typename... Types> class Variant {
  public:
    // -- Type definitions ---------------------------------------------------
    using Tag = uint8_t;
    static constexpr Tag Empty = 0;

    // Helper to check if a type is in the variant's type list
    template <typename T, typename First, typename... Rest>
    struct contains_type {
        static constexpr bool value =
            is_same<T, First>::value || contains_type<T, Rest...>::value;
    };

    template <typename T, typename Last> struct contains_type<T, Last> {
        static constexpr bool value = is_same<T, Last>::value;
    };

    // -- Constructors/Destructor --------------------------------------------

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
            other.reset();
        }
    }

    ~Variant() { reset(); }

    // -- Assignment operators -----------------------------------------------

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
                other.reset();
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

    // -- Modifiers ----------------------------------------------------------

    template <typename T, typename... Args>
    typename fl::enable_if<contains_type<T, Types...>::value, T &>::type
    emplace(Args &&...args) {
        reset();
        construct<T>(fl::forward<Args>(args)...);
        return *get<T>();
    }

    void reset() noexcept {
        if (!empty()) {
            destroy_current();
            _tag = Empty;
        }
    }

    // -- Observers ----------------------------------------------------------

    Tag tag() const noexcept { return _tag; }
    bool empty() const noexcept { return _tag == Empty; }

    template <typename T> bool equals(const T &value) const {
        if (empty()) {
            return false;
        }
        if (!is<T>()) {
            return false;
        }
        return *ptr<T>() == value;
    }

    template <typename T> bool is() const noexcept {
        return _tag == type_to_tag<T>();
    }

    template <typename T> T *ptr() {
        if (is<T>()) {
            return reinterpret_cast<T *>(&_storage);
        }
        return nullptr;
    }

    template <typename T> const T *ptr() const {
        if (is<T>()) {
            return reinterpret_cast<const T *>(&_storage);
        }
        return nullptr;
    }

    template <typename T> T &get() {
        T *p = ptr<T>();
        if (!p) {
            // Handle error - in a minimal implementation we could just assert
            // but for now we'll use a static T as a fallback
            static T dummy;
            return dummy;
        }
        return *p;
    }

    template <typename T> const T &get() const {
        const T *p = ptr<T>();
        if (!p) {
            static const T dummy;
            return dummy;
        }
        return *p;
    }

    template <typename Visitor> void visit(Visitor &visitor) {
        if (empty())
            return;

        visit_impl<0, Types...>(visitor);
    }

    template <typename Visitor> void visit(Visitor &visitor) const {
        if (empty())
            return;

        visit_impl<0, Types...>(visitor);
    }

  private:
    // -- Type traits helpers ------------------------------------------------

    // Helper to get the maximum size of all types
    template <typename... Ts> struct max_size;

    template <typename T> struct max_size<T> {
        static constexpr size_t value = sizeof(T);
    };

    template <typename First, typename... Rest>
    struct max_size<First, Rest...> {
        static constexpr size_t value = sizeof(First) > max_size<Rest...>::value
                                            ? sizeof(First)
                                            : max_size<Rest...>::value;
    };

    // Helper to get the maximum alignment of all types
    template <typename... Ts> struct max_align;

    template <typename T> struct max_align<T> {
        static constexpr size_t value = alignof(T);
    };

    template <typename First, typename... Rest>
    struct max_align<First, Rest...> {
        static constexpr size_t value = alignof(First) >
                                                max_align<Rest...>::value
                                            ? alignof(First)
                                            : max_align<Rest...>::value;
    };

    // -- Type to tag conversion ---------------------------------------------

    // Convert a type to its corresponding tag value (1-based index)
    template <typename T, typename... Ts> struct type_to_tag_impl;

    template <typename T, typename First, typename... Rest>
    struct type_to_tag_impl<T, First, Rest...> {
        static constexpr Tag value =
            is_same<T, First>::value ? 1
                                     : 1 + type_to_tag_impl<T, Rest...>::value;
    };

    template <typename T, typename Last> struct type_to_tag_impl<T, Last> {
        static constexpr Tag value = is_same<T, Last>::value ? 1 : 0;
    };

    template <typename T> static constexpr Tag type_to_tag() {
        return type_to_tag_impl<T, Types...>::value;
    }

    // -- Implementation details ---------------------------------------------

    // Construct a value of type T in the storage
    template <typename T, typename... Args> void construct(Args &&...args) {
        new (&_storage) T(fl::forward<Args>(args)...);
        _tag = type_to_tag<T>();
    }

    // Destroy the current value
    void destroy_current() { visit_destroy<0, Types...>(); }

    // Helper to copy construct from another variant
    void copy_construct_from(const Variant &other) {
        copy_construct_impl<0, Types...>(other);
    }

    // Helper to move construct from another variant
    void move_construct_from(Variant &other) {
        move_construct_impl<0, Types...>(other);
    }

    // Implementation of copy construction
    template <size_t I, typename First, typename... Rest>
    void copy_construct_impl(const Variant &other) {
        if (other._tag == I + 1) {
            new (&_storage)
                First(*reinterpret_cast<const First *>(&other._storage));
            _tag = I + 1;
        } else if constexpr (sizeof...(Rest) > 0) {
            copy_construct_impl<I + 1, Rest...>(other);
        }
    }

    // Implementation of move construction
    template <size_t I, typename First, typename... Rest>
    void move_construct_impl(Variant &other) {
        if (other._tag == I + 1) {
            new (&_storage)
                First(fl::move(*reinterpret_cast<First *>(&other._storage)));
            _tag = I + 1;
        } else if constexpr (sizeof...(Rest) > 0) {
            move_construct_impl<I + 1, Rest...>(other);
        }
    }

    // Implementation of destruction
    template <size_t I, typename First, typename... Rest> void visit_destroy() {
        if (_tag == I + 1) {
            reinterpret_cast<First *>(&_storage)->~First();
        } else if constexpr (sizeof...(Rest) > 0) {
            visit_destroy<I + 1, Rest...>();
        }
    }

    // Implementation of visitor pattern
    template <size_t I, typename First, typename... Rest, typename Visitor>
    void visit_impl(Visitor &visitor) {
        if (_tag == I + 1) {
            visitor.accept(*reinterpret_cast<First *>(&_storage));
        } else if constexpr (sizeof...(Rest) > 0) {
            visit_impl<I + 1, Rest...>(visitor);
        }
    }

    template <size_t I, typename First, typename... Rest, typename Visitor>
    void visit_impl(Visitor &visitor) const {
        if (_tag == I + 1) {
            visitor.accept(*reinterpret_cast<const First *>(&_storage));
        } else if constexpr (sizeof...(Rest) > 0) {
            visit_impl<I + 1, Rest...>(visitor);
        }
    }

    // -- Data members -------------------------------------------------------

    // Storage for the variant, sized to fit the largest type
    alignas(
        max_align<Types...>::value) char _storage[max_size<Types...>::value];

    // Current type tag (0 = empty, 1+ = index into Types...)
    Tag _tag;
};

} // namespace fl
