#pragma once

#include <new>     // for placement new
#include "fl/template_magic.h"

namespace fl {

/// A simple variant that can be Empty, hold a T, or hold a U.
template <typename T, typename U> class Variant {
  public:
    enum class Tag : uint8_t { Empty, IsT, IsU };

    // -- ctors/dtor ---------------------------------------------------------

    Variant() noexcept : _tag(Tag::Empty) {}

    Variant(const T &t) : _tag(Tag::IsT) { new (&_storage.t) T(t); }

    Variant(T &&t) : _tag(Tag::IsT) { new (&_storage.t) T(std::move(t)); }

    Variant(const U &u) : _tag(Tag::IsU) { new (&_storage.u) U(u); }

    Variant(U &&u) : _tag(Tag::IsU) { new (&_storage.u) U(std::move(u)); }

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

#ifdef FASTLED_SUPPORTS_STD_MOVE
    Variant(Variant &&other) noexcept : _tag(Tag::Empty) {
        switch (other._tag) {
        case Tag::IsT:
            new (&_storage.t) T(std::move(other._storage.t));
            _tag = Tag::IsT;
            break;
        case Tag::IsU:
            new (&_storage.u) U(std::move(other._storage.u));
            _tag = Tag::IsU;
            break;
        case Tag::Empty:
            break;
        }
        other.reset();
    }

    ~Variant() { reset(); }
#endif

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

    Variant &operator=(Variant &&other) noexcept {
        if (this != &other) {
            reset();
            switch (other._tag) {
            case Tag::IsT:
                new (&_storage.t) T(std::move(other._storage.t));
                _tag = Tag::IsT;
                break;
            case Tag::IsU:
                new (&_storage.u) U(std::move(other._storage.u));
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

    /// Emplace a T in place.
    template <typename... Args> void emplaceT(Args &&...args) {
        reset();
        new (&_storage.t) T(std::forward<Args>(args)...);
        _tag = Tag::IsT;
    }

    /// Emplace a U in place.
    template <typename... Args> void emplaceU(Args &&...args) {
        reset();
        new (&_storage.u) U(std::forward<Args>(args)...);
        _tag = Tag::IsU;
    }

    // -- observers ----------------------------------------------------------

    Tag tag() const noexcept { return _tag; }

    bool isEmpty() const noexcept { return _tag == Tag::Empty; }
    bool isT() const noexcept { return _tag == Tag::IsT; }
    bool isU() const noexcept { return _tag == Tag::IsU; }

    template<typename T1>
    bool isT() const noexcept {
        return _tag == Tag::IsT && is_same<T, T1>::value;
    }

    T &getT() { return _storage.t; }
    const T &getT() const { return _storage.t; }

    U &getU() { return _storage.u; }
    const U &getU() const { return _storage.u; }

    // -- swap ---------------------------------------------------------------

    void swap(Variant &other) noexcept {
        if (_tag == other._tag) {
            // same active member → just swap in place
            if (_tag == Tag::IsT)
                fl::swap(_storage.t, other._storage.t);
            else if (_tag == Tag::IsU)
                fl::swap(_storage.u, other._storage.u);
        } else {
            // different tags → move‐exchange
            Variant tmp(std::move(other));
            other = std::move(*this);
            *this = std::move(tmp);
        }
    }

  private:
    Tag _tag;
    union Storage {
        T t;
        U u;
        Storage() noexcept {}  // doesn't construct either member
        ~Storage() noexcept {} // destructor is called manually via reset()
    } _storage;
};

} // namespace fl
