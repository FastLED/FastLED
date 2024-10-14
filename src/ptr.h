
#pragma once

// FastLED smart pointer. Used mainly by the fx components.

#include "namespace.h"
#include "scoped_ptr.h"
#include <stddef.h>

FASTLED_NAMESPACE_BEGIN

// Declares a smart pointer. DECLARE_SMART_PTR(Foo) will declare a class FooPtr
// which will be a typedef of Ptr<Foo>. After this FooPtr::New(...args) can be
// used to create a new instance of Ptr<Foo>.
#define DECLARE_SMART_PTR(type)                                                \
    class type;                                                                \
    using type##Ptr = Ptr<type>;

// If you have an interface class that you want to create a smart pointer for,
// then you need to use this to bind it to a constructor.
#define DECLARE_SMART_PTR_CONSTRUCTOR(type, constructor)                       \
    template <> class PtrTraits<type> {                                        \
      public:                                                                  \
        template <typename... Args> static Ptr<type> New(Args... args) {       \
            Ptr<type> ptr = constructor(args...);                              \
            return ptr;                                                        \
        }                                                                      \
    };

// Objects that inherit this class can be reference counted and put into
// a Ptr object.
class Referent {
  public:
    Referent() = default;

    virtual void ref() { mRefCount++; }
    virtual int ref_count() const { return mRefCount; }

    virtual void unref() {
        if (--mRefCount == 0) {
            this->destroy();
        }
    }

    virtual void destroy() { delete this; }

  protected:
    virtual ~Referent() = default;
    Referent(const Referent &) = default;
    Referent &operator=(const Referent &) = default;
    Referent(Referent &&) = default;
    Referent &operator=(Referent &&) = default;
    // In order for a Referent to be passed around as const, the refcount must be
    // mutable.
    mutable int mRefCount = 0;
};

template <typename T> class Ptr;

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
// automatic conversion from raw pointers to Ptr or vice versa is allowed and must
// be done explicitly, see the Ptr::TakeOwnership() and Ptr::NoTracking() methods.
//
// To create a Ptr to a concrete object, it's best to use DECLARE_SMART_PTR(Foo) and then
// use FooPtr::New(...) to create a new instance of Ptr<Foo>.
//
// To create a Ptr of an interface bound to a subclass (common for driver code or when hiding
// implementation) use the Ptr<InterfaceClass>::TakeOwnership(new Subclass()) method.
//
// For objects created statically, use Ptr<Referent>::NoTracking(referent) to create a Ptr, as this
// will disable reference tracking but still allow it to be used as a Ptr.
//
// Example:
//   DECLARE_SMART_PTR(Foo);
//   class Foo: public Referent {};
//   FooPtr foo = FooPtr::New();
//
// Example 2: (Manual binding to constructor)
//   class FooSubclass: public Foo {};
//   Ptr<Foo> bar = Ptr<FooSubclass>::TakeOwnership(new FooSubclass());
//
// Example 3: (Provide your own constructor so that FooPtr::New() works to create a FooSubclass)
//   class FooSubclass: public Foo {  // Foo is an interface, FooSubclass is an implementation.
//     public:
//       static FooPtr New(int a, int b);
//   };  
//   DECLARE_SMART_PTR_CONSTRUCTOR(Foo, FooSubclass::New);
//   FooPtr foo = FooPtr::New(1, 2);  // this will now work.
template <typename T> class Ptr : public PtrTraits<T> {
  public:
    friend class PtrTraits<T>;
    
    template <typename... Args> static Ptr<T> New(Args... args) {
        return PtrTraits<T>::New(args...);
    }
    // Used for low level allocations, typically for pointer to an implementation
    // where it needs to convert to a Ptr of a base class.
    static Ptr TakeOwnership(T *ptr) { return Ptr(ptr, true); }

    // Used for low level allocations, typically to handle memory that is
    // statically allocated where the destructor should not be called when
    // the refcount reaches 0.
    static Ptr NoTracking(T &referent) { return Ptr(&referent, false); }

    // create an upcasted Ptr
    template <typename U> Ptr(Ptr<U> &refptr) : referent_(refptr.get()) {
        if (referent_ && isOwned()) {
            referent_->ref();
        }
    }

    template <typename U> Ptr(const Ptr<U> &refptr) : referent_(refptr.get()) {
        if (referent_ && isOwned()) {
            referent_->ref();
        }
    }

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
    T* release() {
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

FASTLED_NAMESPACE_END
