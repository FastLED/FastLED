

#pragma once

#include "fl/variant.h"

namespace fl {

template <typename T> class Optional;
template <typename T> using optional = Optional<T>;

struct Empty {};

template <typename T> class Optional {

  public:
    Optional() : mValue(Empty()) {}
    Optional(const Optional &other) : mValue(other.mValue) {}
    Optional(const T &value) : mValue(value) {}
    ~Optional() { mValue.reset(); }
    bool empty() const { return !mValue.template is<T>(); }
    T *ptr() { return mValue.template ptr<T>(); }
    const T *ptr() const { return mValue.template ptr<T>(); }

    void reset() { mValue.reset(); }

    Optional &operator=(const Optional &other) {
        if (this != &other) {
            mValue = other.mValue;
        }
        return *this;
    }

    Optional &operator=(const T &value) {
        mValue = value;
        return *this;
    }

    bool operator()() const { return !empty(); }
    bool operator!() const { return empty(); }

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

} // namespace fl
