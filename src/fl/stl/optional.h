

#pragma once

#include "fl/stl/variant.h"
#include "fl/stl/type_traits.h"

namespace fl {

// nullopt support for compatibility with std::optional patterns
struct nullopt_t {};
constexpr nullopt_t nullopt{};

template <typename T> class Optional;
template <typename T> using optional = Optional<T>;

struct Empty {};

template <typename T> class Optional {

  public:
    Optional() : mValue(Empty()) {}
    Optional(nullopt_t) : mValue(Empty()) {}
    Optional(const Optional &other) : mValue(other.mValue) {}
    Optional(Optional &&other) noexcept : mValue(fl::move(other.mValue)) {}
    
    Optional(const T &value) : mValue(value) {}
    Optional(T &&value) : mValue(fl::move(value)) {}
    ~Optional() { mValue.reset(); }

    void emplace(T &&value) { mValue = fl::move(value); }

    bool empty() const { return !mValue.template is<T>(); }
    bool has_value() const { return !empty(); }  // std::optional compatibility
    T *ptr() { return mValue.template ptr<T>(); }
    const T *ptr() const { return mValue.template ptr<T>(); }

    void reset() { mValue.reset(); }

    Optional &operator=(const Optional &other) {
        if (this != &other) {
            mValue = other.mValue;
        }
        return *this;
    }

    Optional &operator=(Optional &&other) noexcept {
        if (this != &other) {
            mValue = fl::move(other.mValue);
        }
        return *this;
    }

    Optional &operator=(nullopt_t) {
        mValue = Empty();
        return *this;
    }

    Optional &operator=(const T &value) {
        mValue = value;
        return *this;
    }
    
    Optional &operator=(T &&value) {
        mValue = fl::move(value);
        return *this;
    }

    bool operator()() const { return !empty(); }
    bool operator!() const { return empty(); }

    // Explicit conversion to bool for contextual boolean evaluation
    explicit operator bool() const { return !empty(); }

    bool operator==(const Optional &other) const {
        if (empty() && other.empty()) {
            return true;
        }
        if (empty() || other.empty()) {
            return false;
        }
        return *ptr() == *other.ptr();
    }

    bool operator!=(const Optional &other) const { return !(*this == other); }

    bool operator==(const T &value) const {
        if (empty()) {
            return false;
        }
        return *ptr() == value;
    }

    bool operator==(nullopt_t) const { return empty(); }
    bool operator!=(nullopt_t) const { return !empty(); }

    // Dereference operators for compatibility with std::optional
    T& operator*() { return *ptr(); }
    const T& operator*() const { return *ptr(); }
    T* operator->() { return ptr(); }
    const T* operator->() const { return ptr(); }

    // value() method for std::optional compatibility
    T& value() { return *ptr(); }
    const T& value() const { return *ptr(); }

    // value_or() method for std::optional compatibility
    // Returns the contained value if present, otherwise returns the provided default
    T value_or(const T& default_value) const {
        return has_value() ? *ptr() : default_value;
    }

    // value_or() overload for rvalue default values
    T value_or(T&& default_value) const {
        return has_value() ? *ptr() : fl::move(default_value);
    }

    template <typename TT, typename UU>
    bool operator==(const variant<TT, UU> &other) const {
        if (!other.template holdsTypeOf<T>()) {
            return false;
        }
        if (empty()) {
            return false;
        }
        if (other.empty()) {
            return false;
        }
        return *ptr() == *other.template ptr<T>();
    }

    void swap(Optional &other) {
        if (this != &other) {
            mValue.swap(other.mValue);
        }
    }

  private:
    fl::variant<T, Empty> mValue;
};

// Specialization for rvalue references Optional<T&&>
// This allows optionals to hold and forward rvalue references properly
template <typename T> class Optional<T&&> {
  public:
    Optional() : mPtr(nullptr) {}
    Optional(nullopt_t) : mPtr(nullptr) {}

    // Construct from rvalue reference
    Optional(T&& value) : mPtr(&value) {}

    // Copy constructor deleted - rvalue references cannot be copied
    Optional(const Optional&) = delete;

    // Move constructor - transfers the reference
    Optional(Optional&& other) noexcept : mPtr(other.mPtr) {
        other.mPtr = nullptr;
    }

    ~Optional() { reset(); }

    bool empty() const { return mPtr == nullptr; }
    bool has_value() const { return !empty(); }
    T* ptr() { return mPtr; }
    const T* ptr() const { return mPtr; }

    void reset() { mPtr = nullptr; }

    // Copy assignment deleted - rvalue references cannot be copied
    Optional& operator=(const Optional&) = delete;

    // Move assignment
    Optional& operator=(Optional&& other) noexcept {
        if (this != &other) {
            mPtr = other.mPtr;
            other.mPtr = nullptr;
        }
        return *this;
    }

    Optional& operator=(nullopt_t) {
        reset();
        return *this;
    }

    bool operator()() const { return !empty(); }
    bool operator!() const { return empty(); }

    // Explicit conversion to bool
    explicit operator bool() const { return !empty(); }

    bool operator==(const Optional& other) const {
        if (empty() && other.empty()) {
            return true;
        }
        if (empty() || other.empty()) {
            return false;
        }
        return *mPtr == *other.mPtr;
    }

    bool operator!=(const Optional& other) const { return !(*this == other); }

    bool operator==(nullopt_t) const { return empty(); }
    bool operator!=(nullopt_t) const { return !empty(); }

    // Dereference operators - returns rvalue reference to forward the value
    T&& operator*() { return fl::forward<T>(*mPtr); }
    const T& operator*() const { return *mPtr; }
    T&& get() { return fl::forward<T>(*mPtr); }
    const T& get() const { return *mPtr; }

    // Arrow operator - returns pointer for member access
    T* operator->() { return mPtr; }
    const T* operator->() const { return mPtr; }

    // value() method for std::optional compatibility
    T&& value() { return fl::forward<T>(*mPtr); }
    const T& value() const { return *mPtr; }

  private:
    T* mPtr;  // Pointer to the rvalue reference (only valid during the lifetime of the temporary)
};

// Helper function to create optionals
template <typename T>
optional<T> make_optional(const T& value) {
    return optional<T>(value);
}

template <typename T>
optional<T> make_optional(T&& value) {
    return optional<T>(fl::move(value));
}

} // namespace fl
