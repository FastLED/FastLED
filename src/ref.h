
#pragma once

#include <stddef.h>

// FastLED smart pointer. This was originally called Ptr<T> but that conflicts with
// ArduinoJson::Ptr<T> so it was renamed to Ref<T>.

#include "namespace.h"
#include "scoped_ptr.h"


FASTLED_NAMESPACE_BEGIN

class Referent; // Inherit this if you want your object to be able to go into a
                // Ref, or WeakRef.
template <typename T> class Ref; // Reference counted smart pointer base class.
template <typename T> class WeakRef; // Weak reference smart pointer base class.

// Declares a smart pointer. FASTLED_SMART_REF(Foo) will declare a class FooRef
// which will be a typedef of Ref<Foo>. After this FooRef::New(...args) can be
// used to create a new instance of Ref<Foo>.
#define FASTLED_SMART_REF(type)                                                \
    class type;                                                                \
    using type##Ref = Ref<type>;

#define FASTLED_SMART_REF_NO_FWD(type) using type##Ref = Ref<type>;

// If you have an interface class that you want to create a smart pointer for,
// then you need to use this to bind it to a constructor.
#define FASTLED_SMART_REF_CONSTRUCTOR(type, constructor)                       \
    template <> class RefTraits<type> {                                        \
      public:                                                                  \
        template <typename... Args> static Ref<type> New(Args... args) {       \
            Ref<type> ptr = constructor(args...);                              \
            return ptr;                                                        \
        }                                                                      \
    };

template <typename T> class Ref;
template <typename T> class WeakRef;

template <typename T> class RefTraits {
  public:
    using element_type = T;
    using ptr_type = Ref<T>;

    template <typename... Args> static Ref<T> New(Args... args) {
        T *ptr = new T(args...);
        return Ref<T>::TakeOwnership(ptr);
    }

    static Ref<T> New() {
        T *ptr = new T();
        return Ref<T>::TakeOwnership(ptr);
    }
};

// Ref is a reference-counted smart pointer that manages the lifetime of an
// object.
//
// It will work with any class implementing ref(), unref() and destroy().
//
// Please note that this Ref class is "sticky" to it's referent, that is, no
// automatic conversion from raw pointers to Ref or vice versa is allowed and
// must be done explicitly, see the Ref::TakeOwnership() and Ref::NoTracking()
// methods.
//
// To create a Ref to a concrete object, it's best to use FASTLED_SMART_REF(Foo)
// and then use FooRef::New(...) to create a new instance of Ref<Foo>.
//
// To create a Ref of an interface bound to a subclass (common for driver code
// or when hiding implementation) use the Ref<InterfaceClass>::TakeOwnership(new
// Subclass()) method.
//
// For objects created statically, use Ref<Referent>::NoTracking(referent) to
// create a Ref, as this will disable reference tracking but still allow it to
// be used as a Ref.
//
// Example:
//   FASTLED_SMART_REF(Foo);
//   class Foo: public Referent {};
//   FooRef foo = FooRef::New();
//
// Example 2: (Manual binding to constructor)
//   class FooSubclass: public Foo {};
//   Ref<Foo> bar = Ref<FooSubclass>::TakeOwnership(new FooSubclass());
//
// Example 3: (Provide your own constructor so that FooRef::New() works to
// create a FooSubclass)
//   class FooSubclass: public Foo {  // Foo is an interface, FooSubclass is an
//   implementation.
//     public:
//       static FooRef New(int a, int b);
//   };
//   FASTLED_SMART_REF_CONSTRUCTOR(Foo, FooSubclass::New);
//   FooRef foo = FooRef::New(1, 2);  // this will now work.
template <typename T> class Ref : public RefTraits<T> {
  public:
    friend class RefTraits<T>;

    template <typename... Args> static Ref<T> New(Args... args) {
        return RefTraits<T>::New(args...);
    }
    // Used for low level allocations, typically for pointer to an
    // implementation where it needs to convert to a Ref of a base class.
    static Ref TakeOwnership(T *ptr) { return Ref(ptr, true); }

    // Used for low level allocations, typically to handle memory that is
    // statically allocated where the destructor should not be called when
    // the refcount reaches 0.
    static Ref NoTracking(T &referent) { return Ref(&referent, false); }

    // create an upcasted Ref
    template <typename U> Ref(Ref<U> &refptr) : referent_(refptr.get()) {
        if (referent_ && isOwned()) {
            referent_->ref();
        }
    }

    template <typename U> Ref(const Ref<U> &refptr) : referent_(refptr.get()) {
        if (referent_ && isOwned()) {
            referent_->ref();
        }
    }

    static Ref<T> Null() { return Ref<T>(); }

    Ref() : referent_(nullptr) {}

    // Forbidden to convert a raw pointer to a Referent into a Ref, because
    // it's possible that the raw pointer comes from the stack or static memory.
    Ref(T *referent) = delete;
    Ref &operator=(T *referent) = delete;

    Ref(const Ref &other) : referent_(other.referent_) {
        if (referent_ && isOwned()) {
            referent_->ref();
        }
    }

    Ref(Ref &&other) noexcept : referent_(other.referent_) {
        other.referent_ = nullptr;
    }

    ~Ref() {
        if (referent_ && isOwned()) {
            referent_->unref();
        }
    }

    Ref &operator=(const Ref &other) {
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
    WeakRef<T> weakRefNoCreate() const;
    WeakRef<T> weakRef() const { return WeakRef<T>(*this); }

    bool operator==(const T *other) const { return referent_ == other; }

    bool operator!=(const T *other) const { return referent_ != other; }

    bool operator==(const Ref &other) const {
        return referent_ == other.referent_;
    }
    bool operator!=(const Ref &other) const {
        return referent_ != other.referent_;
    }

    bool operator<(const Ref &other) const {
        return referent_ < other.referent_;
    }

    Ref &operator=(Ref &&other) noexcept {
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

    void reset(Ref<T> &refptr) {
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

    // Releases the pointer from reference counting from this Ref.
    T *release() {
        T *temp = referent_;
        referent_ = nullptr;
        return temp;
    }

    void swap(Ref &other) noexcept {
        T *temp = referent_;
        referent_ = other.referent_;
        other.referent_ = temp;
    }

    bool isOwned() const { return referent_ && referent_->ref_count() > 0; }

  private:
    Ref(T *referent, bool from_heap) : referent_(referent) {
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

template <typename T> class WeakRef {
  public:
    WeakRef() : mWeakRef() {}

    WeakRef(const Ref<T> &ptr) {
        if (ptr) {
            WeakRef weakRefNoCreate = ptr.weakRefNoCreate();
            bool expired = weakRefNoCreate.expired();
            if (expired) {
                Ref<WeakReferent> weakRefNoCreate = Ref<WeakReferent>::New();
                ptr->setWeakRef(weakRefNoCreate);
                weakRefNoCreate->setReferent(ptr.get());
            }
            mWeakRef = ptr->mWeakRef;
        }
    }

    template <typename U> WeakRef(const Ref<U> &ptr) : mWeakRef(ptr->mWeakRef) {
        if (ptr) {
            WeakRef weakRefNoCreate = ptr.weakRefNoCreate();
            bool expired = weakRefNoCreate.expired();
            if (expired) {
                Ref<WeakReferent> weakRefNoCreate = Ref<WeakReferent>::New();
                ptr->setWeakRef(weakRefNoCreate);
                weakRefNoCreate->setReferent(ptr.get());
            }
            mWeakRef = ptr->mWeakRef;
        }
    }

    WeakRef(const WeakRef &other) : mWeakRef(other.mWeakRef) {}

    template <typename U>
    WeakRef(const WeakRef<U> &other) : mWeakRef(other.mWeakRef) {}

    WeakRef(WeakRef &&other) noexcept : mWeakRef(other.mWeakRef) {}

    ~WeakRef() { reset(); }

    operator bool() const { return mWeakRef && mWeakRef->getReferent(); }

    bool operator!() const {
        bool ok = *this;
        return !ok;
    }

    bool operator==(const WeakRef &other) const {
        return mWeakRef == other.mWeakRef;
    }

    bool operator!=(const WeakRef &other) const {
        return !(mWeakRef != other.mWeakRef);
    }

    bool operator==(const T *other) const { return lock().get() == other; }

    bool operator==(T *other) const {
        if (!mWeakRef) {
            return other == nullptr;
        }
        return mWeakRef->getReferent() == other;
    }

    bool operator==(const Ref<T> &other) const {
        if (!mWeakRef) {
            return !other;
        }
        return mWeakRef->getReferent() == other.get();
    }

    bool operator!=(const T *other) const {
        bool equal = *this == other;
        return !equal;
    }

    WeakRef &operator=(const WeakRef &other) {
        this->mWeakRef = other.mWeakRef;
        return *this;
    }

    Ref<T> lock() const {
        if (!mWeakRef) {
            return Ref<T>();
        }
        T* out = static_cast<T*>(mWeakRef->getReferent());
        if (out->ref_count() == 0) {
            // This is a static object, so the refcount is 0.
            return Ref<T>::NoTracking(*out);
        }
        // This is a heap object, so we need to ref it.
        return Ref<T>::TakeOwnership(static_cast<T *>(out));
    }

    bool expired() const {
        if (!mWeakRef) {
            return true;
        }
        if (!mWeakRef->getReferent()) {
            return true;
        }
        return false;
    }

    void reset() {
        if (mWeakRef) {
            mWeakRef.reset();
        }
    }
    Ref<WeakReferent> mWeakRef;
};

// Objects that inherit this class can be reference counted and put into
// a Ref object. They can also be put into a WeakRef object.
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
    template <typename T> friend class Ref;
    template <typename T> friend class WeakRef;
    void setWeakRef(Ref<WeakReferent> weakRefNoCreate) {
        mWeakRef = weakRefNoCreate;
    }
    mutable int mRefCount;
    Ref<WeakReferent> mWeakRef; // Optional weak reference to this object.
};

template <typename T> inline WeakRef<T> Ref<T>::weakRefNoCreate() const {
    if (!referent_) {
        return WeakRef<T>();
    }
    WeakReferent *tmp = get()->mWeakRef.get();
    if (!tmp) {
        return WeakRef<T>();
    }
    T *referent = static_cast<T *>(tmp->getReferent());
    if (!referent) {
        return WeakRef<T>();
    }
    // At this point, we know that our weak referent is valid.
    // However, the template parameter ensures that either we have
    // an exact type, or are at least down-castable of it.
    WeakRef<T> out;
    out.mWeakRef = get()->mWeakRef;
    return out;
}

FASTLED_NAMESPACE_END
