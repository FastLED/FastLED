#pragma once

#include "fl/namespace.h"

namespace fl {

template <typename T> class Ptr; // Forward declaration
template <typename T> class WeakPtr; // Forward declaration

class Referent; // Forward declaration

// Don't inherit from this, this is an internal object.
class WeakReferent {
  public:
    WeakReferent() : mRefCount(0), mReferent(nullptr) {}
    ~WeakReferent() {}

    void ref() { mRefCount++; }
    int ref_count() const { return mRefCount; }
    void unref() {
        if (--mRefCount == 0) {
            destroy();
        }
    }
    void destroy() { delete this; }
    void setReferent(Referent *referent) { mReferent = referent; }
    Referent *getReferent() const { return mReferent; }

  protected:
    WeakReferent(const WeakReferent &) = default;
    WeakReferent &operator=(const WeakReferent &) = default;

  private:
    int mRefCount;
    Referent *mReferent;
};

// Base class for reference counted objects.
// NOTE: This is legacy - new code should use regular classes with fl::shared_ptr<T>
class Referent {
  public:
    // NOTE: Start with ref count 0, TakeOwnership() will call ref() to make it 1
    Referent() : mRefCount(0), mWeakReferent(nullptr) {}

    // NOTE: Copy constructor also starts with ref count 0 for new object
    Referent(const Referent &) : mRefCount(0), mWeakReferent(nullptr) {}

    // Assignment does not change reference count
    Referent &operator=(const Referent &) { return *this; }

    virtual ~Referent() {
        if (mWeakReferent) {
            mWeakReferent->setReferent(nullptr);
            mWeakReferent->unref();
        }
    }

    void ref() { mRefCount++; }

    int ref_count() const { return mRefCount; }

    void unref() {
        if (--mRefCount == 0) {
            delete this;
        }
    }

    WeakReferent *getWeakReferent() {
        if (!mWeakReferent) {
            mWeakReferent = new WeakReferent();
            mWeakReferent->setReferent(this);
            mWeakReferent->ref();
        }
        return mWeakReferent;
    }

  protected:
    int mRefCount;
    WeakReferent *mWeakReferent;
};

} // namespace fl
