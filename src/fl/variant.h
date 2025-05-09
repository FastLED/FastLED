#pragma once

#include "fl/inplacenew.h"
#include "fl/template_magic.h"

namespace fl {

template <typename T, typename U> class Variant;

// std like compatibility.
template <typename T, typename U> using variant = Variant<T, U>;

/// A simple variant that can be Empty, hold a T, or hold a U.
template <typename T, typename U> class Variant {
  public:
    enum class Tag : uint8_t { Empty, IsT, IsU };

    // -- ctors/dtor ---------------------------------------------------------

    Variant() noexcept : _tag(Tag::Empty) {}
    Variant(const T &t) : _tag(Tag::IsT) { new (&_storage.t) T(t); }
    Variant(const U &u) : _tag(Tag::IsU) { new (&_storage.u) U(u); }
    Variant(const Variant &other) : _tag(Tag::Empty) {
        switch (other._tag) {
        case Tag::IsT:
            new (&_storage.t) T(other._storage.t);
            _tag = Tag::IsT;
            break;
        case Tag::IsU:
            new (&_storage.u) U(other._storage.u);
            _tag = Tag::IsU;
            break;
        case Tag::Empty:
            break;
        }
    }

    template <typename TT> bool holdsTypeOf() {
        if (is_same<T, TT>::value) {
            return true;
        } else if (is_same<U, TT>::value) {
            return true;
        } else {
            return false;
        }
    }

    Variant(T &&t) : _tag(Tag::IsT) { new (&_storage.t) T(fl::move(t)); }
    Variant(U &&u) : _tag(Tag::IsU) { new (&_storage.u) U(fl::move(u)); }
    Variant(Variant &&other) noexcept : _tag(Tag::Empty) {
        switch (other._tag) {
        case Tag::IsT:
            new (&_storage.t) T(fl::move(other._storage.t));
            _tag = Tag::IsT;
            break;
        case Tag::IsU:
            new (&_storage.u) U(fl::move(other._storage.u));
            _tag = Tag::IsU;
            break;
        case Tag::Empty:
            break;
        }
        other.reset();
    }

    Variant &operator=(Variant &&other) noexcept {
        if (this != &other) {
            reset();
            switch (other._tag) {
            case Tag::IsT:
                new (&_storage.t) T(fl::move(other._storage.t));
                _tag = Tag::IsT;
                break;
            case Tag::IsU:
                new (&_storage.u) U(fl::move(other._storage.u));
                _tag = Tag::IsU;
                break;
            case Tag::Empty:
                _tag = Tag::Empty;
                break;
            }
            other.reset();
        }
        return *this;
    }

    /// Emplace a T in place.
    template <typename... Args> void emplaceT(Args &&...args) {
        reset();
        new (&_storage.t) T(fl::forward<Args>(args)...);
        _tag = Tag::IsT;
    }

    /// Emplace a U in place.
    template <typename... Args> void emplaceU(Args &&...args) {
        reset();
        new (&_storage.u) U(fl::forward<Args>(args)...);
        _tag = Tag::IsU;
    }

    ~Variant() { reset(); }

    // -- assignment ---------------------------------------------------------

    Variant &operator=(const Variant &other) {
        if (this != &other) {
            reset();
            switch (other._tag) {
            case Tag::IsT:
                new (&_storage.t) T(other._storage.t);
                _tag = Tag::IsT;
                break;
            case Tag::IsU:
                new (&_storage.u) U(other._storage.u);
                _tag = Tag::IsU;
                break;
            case Tag::Empty:
                _tag = Tag::Empty;
                break;
            }
        }
        return *this;
    }

    // -- modifiers ----------------------------------------------------------

    /// Destroy current content and become empty.
    void reset() noexcept {
        switch (_tag) {
        case Tag::IsT:
            _storage.t.~T();
            break;
        case Tag::IsU:
            _storage.u.~U();
            break;
        case Tag::Empty:
            break;
        }
        _tag = Tag::Empty;
    }

    // -- observers ----------------------------------------------------------

    Tag tag() const noexcept { return _tag; }
    bool empty() const noexcept { return _tag == Tag::Empty; }

    template <typename TYPE> bool is() const noexcept {
        if (is_same<T, TYPE>::value) {
            return isT();
        } else if (is_same<U, TYPE>::value) {
            return isU();
        } else {
            return false;
        }
    }

    template <typename TYPE> TYPE *ptr() {
        if (is<TYPE>()) {
            return reinterpret_cast<TYPE *>(&_storage.t);
        } else {
            return nullptr;
        }
    }

    template <typename TYPE> const TYPE *ptr() const {
        if (is<TYPE>()) {
            return reinterpret_cast<const TYPE *>(&_storage.t);
        } else {
            return nullptr;
        }
    }

    template <typename TYPE> bool equals(const TYPE &other) const {
        if (is<TYPE>()) {
            return *ptr<TYPE>() == other;
        } else {
            return false;
        }
    }

    // -- swap ---------------------------------------------------------------
    void swap(Variant &other) noexcept {
        if (this != &other) {
            if (isT() && other.isT()) {
                fl::swap(getT(), other.getT());
            } else if (isU() && other.isU()) {
                fl::swap(getU(), other.getU());
            } else if (isT() && other.isU()) {
                T tmp = getT();
                getT() = other.getU();
                other.getU() = tmp;
            } else if (isU() && other.isT()) {
                U tmp = getU();
                getU() = other.getT();
                other.getT() = tmp;
            }
        }
    }

  private:
    bool isT() const noexcept { return _tag == Tag::IsT; }
    bool isU() const noexcept { return _tag == Tag::IsU; }
    T &getT() { return _storage.t; }
    const T &getT() const { return _storage.t; }

    U &getU() { return _storage.u; }
    const U &getU() const { return _storage.u; }
    Tag _tag;
    union Storage {
        T t;
        U u;
        Storage() noexcept {}  // doesn't construct either member
        ~Storage() noexcept {} // destructor is called manually via reset()
    } _storage;
};

template <typename T, typename U, typename V> class Variant3 {
  public:
    enum class Tag : uint8_t { Empty, IsT, IsU, IsV };

    // -- ctors/dtor ---------------------------------------------------------

    Variant3() noexcept : _tag(Tag::Empty) {}
    Variant3(const T &t) : _tag(Tag::IsT) { new (&_storage.t) T(t); }
    Variant3(const U &u) : _tag(Tag::IsU) { new (&_storage.u) U(u); }
    Variant3(const V &v) : _tag(Tag::IsV) { new (&_storage.v) V(v); }

    Variant3(const Variant3 &other) : _tag(Tag::Empty) {
        switch (other._tag) {
        case Tag::IsT:
            new (&_storage.t) T(other._storage.t);
            _tag = Tag::IsT;
            break;
        case Tag::IsU:
            new (&_storage.u) U(other._storage.u);
            _tag = Tag::IsU;
            break;
        case Tag::IsV:
            new (&_storage.v) V(other._storage.v);
            _tag = Tag::IsV;
            break;
        case Tag::Empty:
            break;
        }
    }

    Variant3(T &&t) : _tag(Tag::IsT) { new (&_storage.t) T(fl::move(t)); }
    Variant3(U &&u) : _tag(Tag::IsU) { new (&_storage.u) U(fl::move(u)); }
    Variant3(V &&v) : _tag(Tag::IsV) { new (&_storage.v) V(fl::move(v)); }

    Variant3(Variant3 &&other) noexcept : _tag(Tag::Empty) {
        switch (other._tag) {
        case Tag::IsT:
            new (&_storage.t) T(fl::move(other._storage.t));
            _tag = Tag::IsT;
            break;
        case Tag::IsU:
            new (&_storage.u) U(fl::move(other._storage.u));
            _tag = Tag::IsU;
            break;
        case Tag::IsV:
            new (&_storage.v) V(fl::move(other._storage.v));
            _tag = Tag::IsV;
            break;
        case Tag::Empty:
            break;
        }
        other.reset();
    }

    Variant3 &operator=(Variant3 &&other) noexcept {
        if (this != &other) {
            reset();
            switch (other._tag) {
            case Tag::IsT:
                new (&_storage.t) T(fl::move(other._storage.t));
                _tag = Tag::IsT;
                break;
            case Tag::IsU:
                new (&_storage.u) U(fl::move(other._storage.u));
                _tag = Tag::IsU;
                break;
            case Tag::IsV:
                new (&_storage.v) V(fl::move(other._storage.v));
                _tag = Tag::IsV;
                break;
            case Tag::Empty:
                _tag = Tag::Empty;
                break;
            }
            other.reset();
        }
        return *this;
    }

    /// Emplace a T in place.
    template <typename... Args> void emplaceT(Args &&...args) {
        reset();
        new (&_storage.t) T(fl::forward<Args>(args)...);
        _tag = Tag::IsT;
    }

    /// Emplace a U in place.
    template <typename... Args> void emplaceU(Args &&...args) {
        reset();
        new (&_storage.u) U(fl::forward<Args>(args)...);
        _tag = Tag::IsU;
    }

    /// Emplace a V in place.
    template <typename... Args> void emplaceV(Args &&...args) {
        reset();
        new (&_storage.v) V(fl::forward<Args>(args)...);
        _tag = Tag::IsV;
    }

    ~Variant3() { reset(); }

    // -- assignment ---------------------------------------------------------

    Variant3 &operator=(const Variant3 &other) {
        if (this != &other) {
            reset();
            switch (other._tag) {
            case Tag::IsT:
                new (&_storage.t) T(other._storage.t);
                _tag = Tag::IsT;
                break;
            case Tag::IsU:
                new (&_storage.u) U(other._storage.u);
                _tag = Tag::IsU;
                break;
            case Tag::IsV:
                new (&_storage.v) V(other._storage.v);
                _tag = Tag::IsV;
                break;
            case Tag::Empty:
                _tag = Tag::Empty;
                break;
            }
        }
        return *this;
    }

    // -- modifiers ----------------------------------------------------------

    void reset() noexcept {
        switch (_tag) {
        case Tag::IsT:
            _storage.t.~T();
            break;
        case Tag::IsU:
            _storage.u.~U();
            break;
        case Tag::IsV:
            _storage.v.~V();
            break;
        case Tag::Empty:
            break;
        }
        _tag = Tag::Empty;
    }

    // -- observers ----------------------------------------------------------

    Tag tag() const noexcept { return _tag; }
    bool empty() const noexcept { return _tag == Tag::Empty; }

    template <typename TYPE> bool is() const noexcept {
        if (is_same<T, TYPE>::value) {
            return isT();
        } else if (is_same<U, TYPE>::value) {
            return isU();
        } else if (is_same<V, TYPE>::value) {
            return isV();
        } else {
            return false;
        }
    }

    template <typename TYPE> TYPE *ptr() {
        if (is<TYPE>()) {
            return reinterpret_cast<TYPE *>(&_storage.t);
        } else {
            return nullptr;
        }
    }

    template <typename Visitor> void visit(Visitor &visitor) const {
        switch (_tag) {
        case Tag::IsT:
            visitor.accept(getT());
            break;
        case Tag::IsU:
            visitor.accept(getU());
            break;
        case Tag::IsV:
            visitor.accept(getV());
            break;
        case Tag::Empty:
            break;
        }
    }

    template <typename Visitor> void visit(Visitor &visitor) {
        switch (_tag) {
        case Tag::IsT:
            visitor.accept(getT());
            break;
        case Tag::IsU:
            visitor.accept(getU());
            break;
        case Tag::IsV:
            visitor.accept(getV());
            break;
        case Tag::Empty:
            break;
        }
    }

    template <typename TYPE> TYPE *get() { return this->ptr<TYPE>(); }

    template <typename TYPE> const TYPE *ptr() const {
        if (is<TYPE>()) {
            return reinterpret_cast<const TYPE *>(&_storage.t);
        } else {
            return nullptr;
        }
    }

    template <typename TYPE> bool equals(const TYPE &other) const {
        if (is<TYPE>()) {
            return *ptr<TYPE>() == other;
        } else {
            return false;
        }
    }

  private:
    bool isT() const noexcept { return _tag == Tag::IsT; }
    bool isU() const noexcept { return _tag == Tag::IsU; }
    bool isV() const noexcept { return _tag == Tag::IsV; }

    T &getT() { return _storage.t; }
    const T &getT() const { return _storage.t; }

    U &getU() { return _storage.u; }
    const U &getU() const { return _storage.u; }

    V &getV() { return _storage.v; }
    const V &getV() const { return _storage.v; }

    Tag _tag;
    union Storage {
        T t;
        U u;
        V v;
        Storage() noexcept {}  // doesn't construct anything
        ~Storage() noexcept {} // destructor is manual via reset()
    } _storage;
};

// A variant that can hold any of N different types
template <typename... Types> class VariantN {
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

    VariantN() noexcept : _tag(Empty) {}

    template <typename T, typename = typename fl::enable_if<
                              contains_type<T, Types...>::value>::type>
    VariantN(const T &value) : _tag(Empty) {
        construct<T>(value);
    }

    template <typename T, typename = typename fl::enable_if<
                              contains_type<T, Types...>::value>::type>
    VariantN(T &&value) : _tag(Empty) {
        construct<T>(fl::move(value));
    }

    VariantN(const VariantN &other) : _tag(Empty) {
        if (!other.empty()) {
            copy_construct_from(other);
        }
    }

    VariantN(VariantN &&other) noexcept : _tag(Empty) {
        if (!other.empty()) {
            move_construct_from(other);
            other.reset();
        }
    }

    ~VariantN() { reset(); }

    // -- Assignment operators -----------------------------------------------

    VariantN &operator=(const VariantN &other) {
        if (this != &other) {
            reset();
            if (!other.empty()) {
                copy_construct_from(other);
            }
        }
        return *this;
    }

    VariantN &operator=(VariantN &&other) noexcept {
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
    VariantN &operator=(const T &value) {
        reset();
        construct<T>(value);
        return *this;
    }

    template <typename T, typename = typename fl::enable_if<
                              contains_type<T, Types...>::value>::type>
    VariantN &operator=(T &&value) {
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
    void copy_construct_from(const VariantN &other) {
        copy_construct_impl<0, Types...>(other);
    }

    // Helper to move construct from another variant
    void move_construct_from(VariantN &other) {
        move_construct_impl<0, Types...>(other);
    }

    // Implementation of copy construction
    template <size_t I, typename First, typename... Rest>
    void copy_construct_impl(const VariantN &other) {
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
    void move_construct_impl(VariantN &other) {
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
