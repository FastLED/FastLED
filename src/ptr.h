
#pragma once

#include "namespace.h"
#include <stddef.h>
#include "scoped_ptr.h"

FASTLED_NAMESPACE_BEGIN

#define DECLARE_SMART_PTR(type)                                                \
    class type;                                                                \
    using type##Ptr = Ptr<type>;



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
    int mRefCount = 0;
};

template <typename T> class Ptr;


template<typename T>
class PtrTraits {
  public:
    template <typename... Args>
    static Ptr<T> New(Args... args) {
        T* ptr = new T(args...);
        return Ptr<T>::FromHeap(ptr);
    }

    static Ptr<T> New() {
        T* ptr = new T();
        return Ptr<T>::FromHeap(ptr);
    }
};

// Ptr is a reference-counted smart pointer that manages the lifetime of an
// object. This Ptr has to be used with classes that inherit from Referent.
// There is an important feature for this Ptr though, it is designed to bind
// to pointers that *may* have been allocated on the stack or static memory.
template <typename T>
class Ptr: public PtrTraits<T> {
  public:
    // element_type is the type of the managed object
    using element_type = T;
    static Ptr FromHeap(T *referent) { return Ptr(referent, true); }

    template <typename U> static Ptr FromHeap(U *referent) {
        return Ptr(referent, true);
    }

    template <typename... Args>
    static Ptr<T> New(Args... args) {
        return PtrTraits<T>::New(args...);
    }

    // This is a special constructor that is used to create a Ptr from a raw
    // pointer
    static Ptr FromStatic(T &referent) { return Ptr(&referent, false); }

    // create an upcasted Ptr
    template <typename U> Ptr(Ptr<U> &refptr) : referent_(refptr.get()) {
        if (referent_ && isOwned()) {
            referent_->ref();
        }
    }

    template <typename U>
    Ptr(const Ptr<U> &refptr) : referent_(refptr.get()) {
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
