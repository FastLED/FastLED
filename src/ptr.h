
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

    // Disable move semantics (no ownership transfer allowed)
    scoped_ptr(scoped_ptr&&) = delete;
    scoped_ptr& operator=(scoped_ptr&&) = delete;

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


FASTLED_NAMESPACE_END
