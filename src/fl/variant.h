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

} // namespace fl
