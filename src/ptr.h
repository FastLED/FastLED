
#pragma once

#include "namespace.h"
#include <stddef.h>

FASTLED_NAMESPACE_BEGIN


template<typename T>
class RefPtr;

// Helper type to detect RefPtr
template<typename T>
struct is_refptr {
    static const bool value = false;
};

template<typename T>
struct is_refptr<RefPtr<T>> {
    static const bool value = true;
};



template <typename T>
class scoped_ptr {
public:
    // Constructor
    explicit scoped_ptr(T* ptr = nullptr) : ptr_(ptr) {}

    // Destructor
    ~scoped_ptr() {
        delete ptr_;
    }

    // Disable copy semantics (no copying allowed)
    scoped_ptr(const scoped_ptr&) = delete;
    scoped_ptr& operator=(const scoped_ptr&) = delete;

    // Move constructor
    scoped_ptr(scoped_ptr&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }

    // Move assignment operator
    scoped_ptr& operator=(scoped_ptr&& other) noexcept {
        if (this != &other) {
            reset(other.ptr_);
            other.ptr_ = nullptr;
        }
        return *this;
    }

    // Access the managed object
    T* operator->() const {
        return ptr_;
    }

    // Dereference the managed object
    T& operator*() const {
        return *ptr_;
    }

    // Get the raw pointer
    T* get() const {
        return ptr_;
    }

    // Boolean conversion operator
    explicit operator bool() const noexcept {
        return ptr_ != nullptr;
    }

    // Logical NOT operator
    bool operator!() const noexcept {
        return ptr_ == nullptr;
    }

    // Release the managed object and reset the pointer
    void reset(T* ptr = nullptr) {
        if (ptr_ == ptr) {
            return;
        }
        delete ptr_;
        ptr_ = ptr;
    }

private:
    T* ptr_; // Managed pointer
};


template <typename T>
class scoped_array {
public:
    // Constructor
    explicit scoped_array(T* arr = nullptr) : arr_(arr) {}

    // Destructor
    ~scoped_array() {
        delete[] arr_;
    }

    // Disable copy semantics (no copying allowed)
    scoped_array(const scoped_array&) = delete;
    scoped_array& operator=(const scoped_array&) = delete;

    // Move constructor
    scoped_array(scoped_array&& other) noexcept : arr_(other.arr_) {
        other.arr_ = nullptr;
    }

    // Move assignment operator
    scoped_array& operator=(scoped_array&& other) noexcept {
        if (this != &other) {
            reset(other.arr_);
            other.arr_ = nullptr;
        }
        return *this;
    }

    // Array subscript operator
    T& operator[](size_t i) const {
        return arr_[i];
    }

    // Get the raw pointer
    T* get() const {
        return arr_;
    }

    // Boolean conversion operator
    explicit operator bool() const noexcept {
        return arr_ != nullptr;
    }

    // Logical NOT operator
    bool operator!() const noexcept {
        return arr_ == nullptr;
    }

    // Release the managed array and reset the pointer
    void reset(T* arr = nullptr) {
        if (arr_ == arr) {
            return;
        }
        delete[] arr_;
        arr_ = arr;
    }

private:
    T* arr_; // Managed array pointer
};



class Referent {
public:
    Referent() = default;

    virtual void ref() {
        mRefCount++;
    }
    virtual int ref_count() const {
        return mRefCount;
    }

    virtual void unref() {
        if (--mRefCount == 0) {
            this->destroy();
        }
    }

    virtual void destroy() {
        delete this;
    }

protected:
    virtual ~Referent() = default;
    Referent(const Referent&) = default;
    Referent& operator=(const Referent&) = default;
    Referent(Referent&&) = default;
    Referent& operator=(Referent&&) = default;
    int mRefCount = 0;
};

// RefPtr is a reference-counted smart pointer that manages the lifetime of an object.
// This RefPtr has to be used with classes that inherit from Referent.
// There is an important feature for this RefPtr though, it is designed to bind to pointers
// that *may* have been allocated on the stack or static memory.
template <typename T>
class RefPtr {
public:
    // element_type is the type of the managed object
    using element_type = T;
    static RefPtr FromHeap(T* referent) {
        return RefPtr(referent, true);
    }

    // This is a special constructor that is used to create a RefPtr from a raw pointer
    static RefPtr FromStatic(T& referent) {
        return RefPtr(&referent, false);
    }

    // create an upcasted RefPtr
    template <typename U>
    RefPtr(RefPtr<U>& refptr) : referent_(refptr.get()) {
        if (referent_ && isOwned()) {
            referent_->ref();
        }
    }

    RefPtr() : referent_(nullptr) {}
    
    // Forbidden to convert a raw pointer to a Referent into a RefPtr, because
    // it's possible that the raw pointer comes from the stack or static memory.
    RefPtr(T* referent) = delete;
    RefPtr& operator=(T* referent) = delete;
    
    RefPtr(const RefPtr& other) : referent_(other.referent_) {
        if (referent_ && isOwned()) {
            referent_->ref();
        }
    }
    
    RefPtr(RefPtr&& other) noexcept : referent_(other.referent_) {
        other.referent_ = nullptr;
    }
    
    ~RefPtr() {
        if (referent_ && isOwned()) {
            referent_->unref();
        }
    }

    RefPtr& operator=(const RefPtr& other) {
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

    RefPtr& operator=(RefPtr&& other) noexcept {
        if (this != &other) {
            if (referent_ && isOwned()) {
                referent_->unref();
            }
            referent_ = other.referent_;
            other.referent_ = nullptr;
        }
        return *this;
    }

    T* get() const {
        return referent_;
    }

    T* operator->() const {
        return referent_;
    }

    T& operator*() const {
        return *referent_;
    }

    explicit operator bool() const noexcept {
        return referent_ != nullptr;
    }

    void reset() {
        if (referent_ && isOwned()) {
            referent_->unref();
        }
        referent_ = nullptr;
    }

    void reset(RefPtr<T>& refptr) {
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

    void swap(RefPtr& other) noexcept {
        T* temp = referent_;
        referent_ = other.referent_;
        other.referent_ = temp;
    }

    bool isOwned() const {
        return referent_ && referent_->ref_count() > 0;
    }

private:
    RefPtr(T* referent, bool from_heap) : referent_(referent) {
        if (referent_ && from_heap) {
            referent_->ref();
        }
    }
    T* referent_;
};

FASTLED_NAMESPACE_END
