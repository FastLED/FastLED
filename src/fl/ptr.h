
#pragma once

#include <stddef.h>

// FastLED smart pointer. This was originally called Ptr<T> but that conflicts with
// ArduinoJson::Ptr<T> so it was renamed to Ptr<T>.

#include "fl/namespace.h"
#include "fl/scoped_ptr.h"
#include "fl/template_magic.h"


// Declares a smart pointer. FASTLED_SMART_PTR(Foo) will declare a class FooPtr
// which will be a typedef of Ptr<Foo>. After this FooPtr::New(...args) can be
// used to create a new instance of Ptr<Foo>.
#define FASTLED_SMART_PTR(type)                                                \
    class type;                                                                \
    using type##Ptr = fl::Ptr<type>;

#define FASTLED_SMART_PTR_NO_FWD(type) using type##Ptr = fl::Ptr<type>;

// If you have an interface class that you want to create a smart pointer for,
// then you need to use this to bind it to a constructor.
#define FASTLED_SMART_PTR_CONSTRUCTOR(type, constructor)                       \
    template <> class PtrTraits<type> {                                        \
      public:                                                                  \
        template <typename... Args> static Ptr<type> New(Args... args) {       \
            fl::Ptr<type> ptr = constructor(args...);                          \
            return ptr;                                                        \
        }                                                                      \
    };



namespace fl {

class Referent; // Inherit this if you want your object to be able to go into a
                // Ptr, or WeakPtr.
template <typename T> class Ptr; // Reference counted smart pointer base class.
template <typename T> class WeakPtr; // Weak reference smart pointer base class.


template <typename T> class Ptr;
template <typename T> class WeakPtr;

template <typename T> class PtrTraits {
  public:
    using element_type = T;
    using ptr_type = Ptr<T>;

    template <typename... Args> static Ptr<T> New(Args... args) {
        T *ptr = new T(args...);
        return Ptr<T>::TakeOwnership(ptr);
    }

    static Ptr<T> New() {
        T *ptr = new T();
        return Ptr<T>::TakeOwnership(ptr);
    }
};

// Ptr is a reference-counted smart pointer that manages the lifetime of an
// object.
//
// It will work with any class implementing ref(), unref() and destroy().
//
// Please note that this Ptr class is "sticky" to it's referent, that is, no
// automatic conversion from raw pointers to Ptr or vice versa is allowed and
// must be done explicitly, see the Ptr::TakeOwnership() and Ptr::NoTracking()
// methods.
//
// To create a Ptr to a concrete object, it's best to use FASTLED_SMART_PTR(Foo)
// and then use FooPtr::New(...) to create a new instance of Ptr<Foo>.
//
// To create a Ptr of an interface bound to a subclass (common for driver code
// or when hiding implementation) use the Ptr<InterfaceClass>::TakeOwnership(new
// Subclass()) method.
//
// For objects created statically, use Ptr<Referent>::NoTracking(referent) to
// create a Ptr, as this will disable reference tracking but still allow it to
// be used as a Ptr.
//
// Example:
//   FASTLED_SMART_PTR(Foo);
//   class Foo: public fl::Referent {};
//   FooPtr foo = FooPtr::New();
//
// Example 2: (Manual binding to constructor)
//   class FooSubclass: public Foo {};
//   Ptr<Foo> bar = Ptr<FooSubclass>::TakeOwnership(new FooSubclass());
//
// Example 3: (Provide your own constructor so that FooPtr::New() works to
// create a FooSubclass)
//   class FooSubclass: public Foo {  // Foo is an interface, FooSubclass is an
//   implementation.
//     public:
//       static FooPtr New(int a, int b);
//   };
//   FASTLED_SMART_PTR_CONSTRUCTOR(Foo, FooSubclass::New);
//   FooPtr foo = FooPtr::New(1, 2);  // this will now work.
template <typename T> class Ptr : public PtrTraits<T> {
  public:
    friend class PtrTraits<T>;

    template <typename... Args> static Ptr<T> New(Args... args) {
        return PtrTraits<T>::New(args...);
    }
    // Used for low level allocations, typically for pointer to an
    // implementation where it needs to convert to a Ptr of a base class.
    static Ptr TakeOwnership(T *ptr) { return Ptr(ptr, true); }

    // Used for low level allocations, typically to handle memory that is
    // statically allocated where the destructor should not be called when
    // the refcount reaches 0.
    static Ptr NoTracking(T &referent) { return Ptr(&referent, false); }


    // Allow upcasting of Refs.
    template <typename U, typename = fl::is_derived<T, U>>
    Ptr(const Ptr<U>& refptr) : referent_(refptr.get()) {
        if (referent_ && isOwned()) {
            referent_->ref();
        }
    }

    static Ptr<T> Null() { return Ptr<T>(); }

    Ptr() : referent_(nullptr) {}

    // Forbidden to convert a raw pointer to a Referent into a Ptr, because
    // it's possible that the raw pointer comes from the stack or static memory.
    Ptr(T *referent) = delete;
    Ptr &operator=(T *referent) = delete;

    Ptr(const Ptr &other) : referent_(other.referent_) {
        if (referent_ && isOwned()) {
            referent_->ref();
        }
    }

    Ptr(Ptr &&other) noexcept : referent_(other.referent_) {
        other.referent_ = nullptr;
    }

    ~Ptr() {
        if (referent_ && isOwned()) {
            referent_->unref();
        }
    }

    Ptr &operator=(const Ptr &other) {
        if (this != &other) {
            if (referent_ && isOwned()) {
                referent_->unref();
            }
            referent_ = other.referent_;
            if (referent_ && isOwned()) {
                referent_->ref();
            }
        }
        return *this;
    }

    // Either returns the weakptr if it exists, or an empty weakptr.
    WeakPtr<T> weakRefNoCreate() const;
    WeakPtr<T> weakPtr() const { return WeakPtr<T>(*this); }

    bool operator==(const T *other) const { return referent_ == other; }

    bool operator!=(const T *other) const { return referent_ != other; }

    bool operator==(const Ptr &other) const {
        return referent_ == other.referent_;
    }
    bool operator!=(const Ptr &other) const {
        return referent_ != other.referent_;
    }

    bool operator<(const Ptr &other) const {
        return referent_ < other.referent_;
    }

    Ptr &operator=(Ptr &&other) noexcept {
        if (this != &other) {
            if (referent_ && isOwned()) {
                referent_->unref();
            }
            referent_ = other.referent_;
            other.referent_ = nullptr;
        }
        return *this;
    }

    T *get() const { return referent_; }

    T *operator->() const { return referent_; }

    T &operator*() const { return *referent_; }

    explicit operator bool() const noexcept { return referent_ != nullptr; }

    void reset() {
        if (referent_ && isOwned()) {
            referent_->unref();
        }
        referent_ = nullptr;
    }

    void reset(Ptr<T> &refptr) {
        if (refptr.referent_ != referent_) {
            if (refptr.referent_ && refptr.isOwned()) {
                refptr.referent_->ref();
            }
            if (referent_ && isOwned()) {
                referent_->unref();
            }
            referent_ = refptr.referent_;
        }
    }

    // Releases the pointer from reference counting from this Ptr.
    T *release() {
        T *temp = referent_;
        referent_ = nullptr;
        return temp;
    }

    void swap(Ptr &other) noexcept {
        T *temp = referent_;
        referent_ = other.referent_;
        other.referent_ = temp;
    }

    bool isOwned() const { return referent_ && referent_->ref_count() > 0; }

  private:
    Ptr(T *referent, bool from_heap) : referent_(referent) {
        if (referent_ && from_heap) {
            referent_->ref();
        }
    }
    T *referent_;
};

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

template <typename T> class WeakPtr {
  public:
    WeakPtr() : mWeakPtr() {}

    WeakPtr(const Ptr<T> &ptr) {
        if (ptr) {
            WeakPtr weakRefNoCreate = ptr.weakRefNoCreate();
            bool expired = weakRefNoCreate.expired();
            if (expired) {
                Ptr<WeakReferent> weakRefNoCreate = Ptr<WeakReferent>::New();
                ptr->setWeakPtr(weakRefNoCreate);
                weakRefNoCreate->setReferent(ptr.get());
            }
            mWeakPtr = ptr->mWeakPtr;
        }
    }

    template <typename U> WeakPtr(const Ptr<U> &ptr) : mWeakPtr(ptr->mWeakPtr) {
        if (ptr) {
            WeakPtr weakRefNoCreate = ptr.weakRefNoCreate();
            bool expired = weakRefNoCreate.expired();
            if (expired) {
                Ptr<WeakReferent> weakRefNoCreate = Ptr<WeakReferent>::New();
                ptr->setWeakPtr(weakRefNoCreate);
                weakRefNoCreate->setReferent(ptr.get());
            }
            mWeakPtr = ptr->mWeakPtr;
        }
    }

    WeakPtr(const WeakPtr &other) : mWeakPtr(other.mWeakPtr) {}

    template <typename U>
    WeakPtr(const WeakPtr<U> &other) : mWeakPtr(other.mWeakPtr) {}

    WeakPtr(WeakPtr &&other) noexcept : mWeakPtr(other.mWeakPtr) {}

    ~WeakPtr() { reset(); }

    operator bool() const { return mWeakPtr && mWeakPtr->getReferent(); }

    bool operator!() const {
        bool ok = *this;
        return !ok;
    }

    bool operator==(const WeakPtr &other) const {
        return mWeakPtr == other.mWeakPtr;
    }

    bool operator!=(const WeakPtr &other) const {
        return !(mWeakPtr != other.mWeakPtr);
    }

    bool operator==(const T *other) const { return lock().get() == other; }

    bool operator==(T *other) const {
        if (!mWeakPtr) {
            return other == nullptr;
        }
        return mWeakPtr->getReferent() == other;
    }

    bool operator==(const Ptr<T> &other) const {
        if (!mWeakPtr) {
            return !other;
        }
        return mWeakPtr->getReferent() == other.get();
    }

    bool operator!=(const T *other) const {
        bool equal = *this == other;
        return !equal;
    }

    WeakPtr &operator=(const WeakPtr &other) {
        this->mWeakPtr = other.mWeakPtr;
        return *this;
    }

    Ptr<T> lock() const {
        if (!mWeakPtr) {
            return Ptr<T>();
        }
        T* out = static_cast<T*>(mWeakPtr->getReferent());
        if (out->ref_count() == 0) {
            // This is a static object, so the refcount is 0.
            return Ptr<T>::NoTracking(*out);
        }
        // This is a heap object, so we need to ref it.
        return Ptr<T>::TakeOwnership(static_cast<T *>(out));
    }

    bool expired() const {
        if (!mWeakPtr) {
            return true;
        }
        if (!mWeakPtr->getReferent()) {
            return true;
        }
        return false;
    }

    void reset() {
        if (mWeakPtr) {
            mWeakPtr.reset();
        }
    }
    Ptr<WeakReferent> mWeakPtr;
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

    virtual void ref();
    virtual void unref();
    virtual void destroy();

  private:
    friend class WeakReferent;
    template <typename T> friend class Ptr;
    template <typename T> friend class WeakPtr;
    void setWeakPtr(Ptr<WeakReferent> weakRefNoCreate) {
        mWeakPtr = weakRefNoCreate;
    }
    mutable int mRefCount;
    Ptr<WeakReferent> mWeakPtr; // Optional weak reference to this object.
};

template <typename T> inline WeakPtr<T> Ptr<T>::weakRefNoCreate() const {
    if (!referent_) {
        return WeakPtr<T>();
    }
    WeakReferent *tmp = get()->mWeakPtr.get();
    if (!tmp) {
        return WeakPtr<T>();
    }
    T *referent = static_cast<T *>(tmp->getReferent());
    if (!referent) {
        return WeakPtr<T>();
    }
    // At this point, we know that our weak referent is valid.
    // However, the template parameter ensures that either we have
    // an exact type, or are at least down-castable of it.
    WeakPtr<T> out;
    out.mWeakPtr = get()->mWeakPtr;
    return out;
}

}  // namespace fl
