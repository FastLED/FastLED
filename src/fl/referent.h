#pragma once

#include "fl/namespace.h"
#include "fl/no_rtti_check.h"  // Enforce RTTI is disabled

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
    WeakReferent(WeakReferent &&) = default;
    WeakReferent &operator=(WeakReferent &&) = default;

  private:
    mutable int mRefCount;
    Referent *mReferent;
};

// Objects that inherit this class can be reference counted and put into
// a Ptr object. They can also be put into a WeakPtr object.
class Referent {
  public:
    virtual int ref_count() const;

  protected:
    Referent();
    virtual ~Referent();
    Referent(const Referent &);
    Referent &operator=(const Referent &);
    Referent(Referent &&);
    Referent &operator=(Referent &&);

    // Lifetime management has to be marked const.
    virtual void ref() const;
    virtual void unref() const;
    virtual void destroy() const;

  private:
    friend class WeakReferent;
    template <typename T> friend class Ptr;
    template <typename T> friend class WeakPtr;
    void setWeakPtr(WeakReferent* weakRefNoCreate) {
        if (mWeakPtr) {
            mWeakPtr->unref();
        }
        mWeakPtr = weakRefNoCreate;
        if (mWeakPtr) {
            mWeakPtr->ref();
        }
    }
    WeakReferent* getWeakPtr() const { return mWeakPtr; }
    mutable int mRefCount;
    mutable WeakReferent* mWeakPtr; // Optional weak reference to this object.
};

} // namespace fl
