

#pragma once

#include "fl/variant.h"

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

    template <typename TT, typename UU>
    bool operator==(const Variant<TT, UU> &other) const {
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
    fl::Variant<T, Empty> mValue;
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
