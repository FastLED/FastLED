

#pragma once

#include "fl/stl/variant.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/noexcept.h"

namespace fl {

// nullopt support for compatibility with std::optional patterns
struct nullopt_t {};
constexpr nullopt_t nullopt{};

template <typename T> class Optional;
template <typename T> using optional = Optional<T>;

struct Empty {};

template <typename T> class Optional {

  public:
    Optional() FL_NOEXCEPT : mValue(Empty()) {}
    Optional(nullopt_t) FL_NOEXCEPT : mValue(Empty()) {}
    Optional(const Optional &other) FL_NOEXCEPT : mValue(other.mValue) {}
    Optional(Optional &&other) FL_NOEXCEPT : mValue(fl::move(other.mValue)) {}
    
    Optional(const T &value) FL_NOEXCEPT : mValue(value) {}
    Optional(T &&value) FL_NOEXCEPT : mValue(fl::move(value)) {}
    ~Optional() FL_NOEXCEPT { mValue.reset(); }

    /// Emplace with rvalue reference
    void emplace(T &&value) FL_NOEXCEPT { mValue = fl::move(value); }

    /// @brief Construct value in-place with variadic arguments
    template <typename... Args>
    void emplace(Args&&... args) FL_NOEXCEPT {
        mValue = T(fl::forward<Args>(args)...);
    }

    bool empty() const FL_NOEXCEPT { return !mValue.template is<T>(); }
    bool has_value() const FL_NOEXCEPT { return !empty(); }  // std::optional compatibility
    T *ptr() FL_NOEXCEPT { return mValue.template ptr<T>(); }
    const T *ptr() const FL_NOEXCEPT { return mValue.template ptr<T>(); }

    void reset() FL_NOEXCEPT { mValue.reset(); }

    Optional &operator=(const Optional &other) FL_NOEXCEPT {
        if (this != &other) {
            mValue = other.mValue;
        }
        return *this;
    }

    Optional &operator=(Optional &&other) FL_NOEXCEPT {
        if (this != &other) {
            mValue = fl::move(other.mValue);
        }
        return *this;
    }

    Optional &operator=(nullopt_t) FL_NOEXCEPT {
        mValue = Empty();
        return *this;
    }

    Optional &operator=(const T &value) FL_NOEXCEPT {
        mValue = value;
        return *this;
    }
    
    Optional &operator=(T &&value) FL_NOEXCEPT {
        mValue = fl::move(value);
        return *this;
    }

    bool operator()() const FL_NOEXCEPT { return !empty(); }
    bool operator!() const FL_NOEXCEPT { return empty(); }

    // Explicit conversion to bool for contextual boolean evaluation
    explicit operator bool() const FL_NOEXCEPT { return !empty(); }

    bool operator==(const Optional &other) const FL_NOEXCEPT {
        if (empty() && other.empty()) {
            return true;
        }
        if (empty() || other.empty()) {
            return false;
        }
        return *ptr() == *other.ptr();
    }

    bool operator!=(const Optional &other) const FL_NOEXCEPT { return !(*this == other); }

    bool operator==(const T &value) const FL_NOEXCEPT {
        if (empty()) {
            return false;
        }
        return *ptr() == value;
    }

    bool operator==(nullopt_t) const FL_NOEXCEPT { return empty(); }
    bool operator!=(nullopt_t) const FL_NOEXCEPT { return !empty(); }

    // Dereference operators for compatibility with std::optional
    T& operator*() FL_NOEXCEPT { return *ptr(); }
    const T& operator*() const FL_NOEXCEPT { return *ptr(); }
    T* operator->() FL_NOEXCEPT { return ptr(); }
    const T* operator->() const FL_NOEXCEPT { return ptr(); }

    // value() method for std::optional compatibility
    T& value() FL_NOEXCEPT { return *ptr(); }
    const T& value() const FL_NOEXCEPT { return *ptr(); }

    // value_or() method for std::optional compatibility
    // Returns the contained value if present, otherwise returns the provided default
    T value_or(const T& default_value) const FL_NOEXCEPT {
        return has_value() ? *ptr() : default_value;
    }

    // value_or() overload for rvalue default values
    T value_or(T&& default_value) const FL_NOEXCEPT {
        return has_value() ? *ptr() : fl::move(default_value);
    }

    template <typename TT, typename UU>
    bool operator==(const variant<TT, UU> &other) const FL_NOEXCEPT {
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

    void swap(Optional &other) FL_NOEXCEPT {
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
    Optional() FL_NOEXCEPT : mPtr(nullptr) {}
    Optional(nullopt_t) FL_NOEXCEPT : mPtr(nullptr) {}

    // Construct from rvalue reference
    Optional(T&& value) FL_NOEXCEPT : mPtr(&value) {}

    // Copy constructor deleted - rvalue references cannot be copied
    Optional(const Optional&) FL_NOEXCEPT = delete;

    // Move constructor - transfers the reference
    Optional(Optional&& other) FL_NOEXCEPT : mPtr(other.mPtr) {
        other.mPtr = nullptr;
    }

    ~Optional() FL_NOEXCEPT { reset(); }

    bool empty() const FL_NOEXCEPT { return mPtr == nullptr; }
    bool has_value() const FL_NOEXCEPT { return !empty(); }
    T* ptr() FL_NOEXCEPT { return mPtr; }
    const T* ptr() const FL_NOEXCEPT { return mPtr; }

    void reset() FL_NOEXCEPT { mPtr = nullptr; }

    // Copy assignment deleted - rvalue references cannot be copied
    Optional& operator=(const Optional&) FL_NOEXCEPT = delete;

    // Move assignment
    Optional& operator=(Optional&& other) FL_NOEXCEPT {
        if (this != &other) {
            mPtr = other.mPtr;
            other.mPtr = nullptr;
        }
        return *this;
    }

    Optional& operator=(nullopt_t) FL_NOEXCEPT {
        reset();
        return *this;
    }

    bool operator()() const FL_NOEXCEPT { return !empty(); }
    bool operator!() const FL_NOEXCEPT { return empty(); }

    // Explicit conversion to bool
    explicit operator bool() const FL_NOEXCEPT { return !empty(); }

    bool operator==(const Optional& other) const FL_NOEXCEPT {
        if (empty() && other.empty()) {
            return true;
        }
        if (empty() || other.empty()) {
            return false;
        }
        return *mPtr == *other.mPtr;
    }

    bool operator!=(const Optional& other) const FL_NOEXCEPT { return !(*this == other); }

    bool operator==(nullopt_t) const FL_NOEXCEPT { return empty(); }
    bool operator!=(nullopt_t) const FL_NOEXCEPT { return !empty(); }

    // Dereference operators - returns rvalue reference to forward the value
    T&& operator*() FL_NOEXCEPT { return fl::forward<T>(*mPtr); }
    const T& operator*() const FL_NOEXCEPT { return *mPtr; }
    T&& get() FL_NOEXCEPT { return fl::forward<T>(*mPtr); }
    const T& get() const FL_NOEXCEPT { return *mPtr; }

    // Arrow operator - returns pointer for member access
    T* operator->() FL_NOEXCEPT { return mPtr; }
    const T* operator->() const FL_NOEXCEPT { return mPtr; }

    // value() method for std::optional compatibility
    T&& value() FL_NOEXCEPT { return fl::forward<T>(*mPtr); }
    const T& value() const FL_NOEXCEPT { return *mPtr; }

  private:
    T* mPtr;  // Pointer to the rvalue reference (only valid during the lifetime of the temporary)
};

// Helper function to create optionals
template <typename T>
optional<T> make_optional(const T& value) FL_NOEXCEPT {
    return optional<T>(value);
}

template <typename T>
optional<T> make_optional(T&& value) FL_NOEXCEPT {
    return optional<T>(fl::move(value));
}

} // namespace fl
