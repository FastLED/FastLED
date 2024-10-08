
#pragma once

#include "namespace.h"
#include <stddef.h>

FASTLED_NAMESPACE_BEGIN

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

    T* release() {
        T* tmp = ptr_;
        ptr_ = nullptr;
        return tmp;
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

    T* release() {
        T* tmp = arr_;
        arr_ = nullptr;
        return tmp;
    }

  private:
    T *arr_; // Managed array pointer
};


FASTLED_NAMESPACE_END
