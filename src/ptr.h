
#pragma once

#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

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
    T& operator[](std::size_t i) const {
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
    virtual ~Referent() = default;
    virtual void ref() {
        mRefCount++;
    }
    virtual int ref_count() const {
        return mRefCount;
    }

    virtual void unref() {
        if (--mRefCount == 0) {
            delete this;
        }
    }

protected:
    Referent(const Referent&) = default;
    Referent& operator=(const Referent&) = default;
    Referent(Referent&&) = default;
    Referent& operator=(Referent&&) = default;
    int mRefCount = 0;
};


template <typename T>
class RefPtr {
public:
    RefPtr() : referent_(nullptr) {}
    
    RefPtr(T* referent) : referent_(referent) {
        if (referent_) {
            referent_->ref();
        }
    }
    
    RefPtr(const RefPtr& other) : referent_(other.referent_) {
        if (referent_) {
            referent_->ref();
        }
    }
    
    RefPtr(RefPtr&& other) noexcept : referent_(other.referent_) {
        other.referent_ = nullptr;
    }
    
    ~RefPtr() {
        if (referent_) {
            referent_->unref();
        }
    }

    RefPtr& operator=(T* referent) {
        if (referent != referent_) {
            if (referent) {
                referent->ref();
            }
            referent_ = referent;
        }
        return *this;
    }

    RefPtr& operator=(const RefPtr& other) {
        if (this != &other) {
            if (other.referent_) {
                other.referent_->ref();
            }
            referent_ = other.referent_;
        }
        return *this;
    }

    RefPtr& operator=(RefPtr&& other) noexcept {
        if (this != &other) {
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

    void reset(T* referent = nullptr) {
        if (referent != referent_) {
            if (referent) {
                referent->ref();
            }
            if (referent_) {
                referent_->unref();
            }
            referent_ = referent;
        }
    }

    void swap(RefPtr& other) noexcept {
        T* temp = referent_;
        referent_ = other.referent_;
        other.referent_ = temp;
    }

private:
    T* referent_;
};

FASTLED_NAMESPACE_END
