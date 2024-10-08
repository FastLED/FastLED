
#pragma once

#include "namespace.h"
#include <stddef.h>

FASTLED_NAMESPACE_BEGIN

#define DECLARE_SMART_PTR(type)                                                \
    class type;                                                                \
    using type##Ptr = Ptr<type>;

template <typename T> class Ptr;

template <typename T> struct ref_unwrapper {
    using type = T;
    using ref_type = Ptr<T>;
};

// specialization for Ptr<Ptr<T>>
template <typename T> struct ref_unwrapper<Ptr<T>> {
    using type = T;
    using ref_type = Ptr<T>;
};

#define REF_PTR(T)                                                             \
    class T;                                                                   \
    using T##Ptr = Ptr<T>;

#define PROTECTED_DESTRUCTOR(T)                                                \
  protected:                                                                   \
    virtual ~T()


template <typename T> class scoped_ptr {
  public:
    // Constructor
    explicit scoped_ptr(T *ptr = nullptr) : ptr_(ptr) {}

    // Destructor
    ~scoped_ptr() { delete ptr_; }

    // Disable copy semantics (no copying allowed)
    scoped_ptr(const scoped_ptr &) = delete;
    scoped_ptr &operator=(const scoped_ptr &) = delete;

    // Move constructor
    scoped_ptr(scoped_ptr &&other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }

    // Move assignment operator
    scoped_ptr &operator=(scoped_ptr &&other) noexcept {
        if (this != &other) {
            reset(other.ptr_);
            other.ptr_ = nullptr;
        }
        return *this;
    }

    // Access the managed object
    T *operator->() const { return ptr_; }

    // Dereference the managed object
    T &operator*() const { return *ptr_; }

    // Get the raw pointer
    T *get() const { return ptr_; }

    // Boolean conversion operator
    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    // Logical NOT operator
    bool operator!() const noexcept { return ptr_ == nullptr; }

    // Release the managed object and reset the pointer
    void reset(T *ptr = nullptr) {
        if (ptr_ == ptr) {
            return;
        }
        delete ptr_;
        ptr_ = ptr;
    }

  private:
    T *ptr_; // Managed pointer
};

template <typename T> class scoped_array {
  public:
    // Constructor
    explicit scoped_array(T *arr = nullptr) : arr_(arr) {}

    // Destructor
    ~scoped_array() { delete[] arr_; }

    // Disable copy semantics (no copying allowed)
    scoped_array(const scoped_array &) = delete;
    scoped_array &operator=(const scoped_array &) = delete;

    // Move constructor
    scoped_array(scoped_array &&other) noexcept : arr_(other.arr_) {
        other.arr_ = nullptr;
    }

    // Move assignment operator
    scoped_array &operator=(scoped_array &&other) noexcept {
        if (this != &other) {
            reset(other.arr_);
            other.arr_ = nullptr;
        }
        return *this;
    }

    // Array subscript operator
    T &operator[](size_t i) const { return arr_[i]; }

    // Get the raw pointer
    T *get() const { return arr_; }

    // Boolean conversion operator
    explicit operator bool() const noexcept { return arr_ != nullptr; }

    // Logical NOT operator
    bool operator!() const noexcept { return arr_ == nullptr; }

    // Release the managed array and reset the pointer
    void reset(T *arr = nullptr) {
        if (arr_ == arr) {
            return;
        }
        delete[] arr_;
        arr_ = arr;
    }

  private:
    T *arr_; // Managed array pointer
};

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

class PtrBase {
  public:
    template <typename T, typename... Args>
    static typename ref_unwrapper<T>::ref_type New(Args... args) {
        using U = typename ref_unwrapper<T>::type;
        return Ptr<U>::FromHeap(new U(args...));
    }

    template <typename T> static typename ref_unwrapper<T>::ref_type New() {
        using U = typename ref_unwrapper<T>::type;
        return Ptr<U>::FromHeap(new U());
    }
};

// Ptr is a reference-counted smart pointer that manages the lifetime of an
// object. This Ptr has to be used with classes that inherit from Referent.
// There is an important feature for this Ptr though, it is designed to bind
// to pointers that *may* have been allocated on the stack or static memory.
template <typename T>
class Ptr: public PtrBase {
  public:
    // element_type is the type of the managed object
    using element_type = T;
    static Ptr FromHeap(T *referent) { return Ptr(referent, true); }

    template <typename U> static Ptr FromHeap(U *referent) {
        return Ptr(referent, true);
    }

    template <typename... Args>
    static Ptr<T> New(Args... args) {
        return PtrBase::New<T>(args...);
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
