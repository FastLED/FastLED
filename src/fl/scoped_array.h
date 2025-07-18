#pragma once

#include "fl/allocator.h"
#include "fl/inplacenew.h"
#include "fl/namespace.h"
#include "fl/deprecated.h"
#include "fl/cstddef.h"

namespace fl {

// Keep existing ArrayDeleter for compatibility with scoped_array
template <typename T> struct ArrayDeleter {
    ArrayDeleter() = default;
    void operator()(T *ptr) { delete[] ptr; }
};

// Keep existing PointerDeleter for compatibility (though default_delete is preferred)
template <typename T> struct PointerDeleter {
    void operator()(T *ptr) { delete ptr; }
};

// Keep existing scoped_array class unchanged as they serve different purposes than unique_ptr
template <typename T, typename Deleter = ArrayDeleter<T>> class scoped_array {
  public:
    FASTLED_DEPRECATED_CLASS("Use fl::vector<T, fl::allocator_psram<T>> instead");
    // Constructor
    explicit scoped_array(T *arr = nullptr) : arr_(arr) {}
    scoped_array(T *arr, Deleter deleter) : arr_(arr), deleter_(deleter) {}

    // Destructor
    ~scoped_array() { deleter_(arr_); }

    // Disable copy semantics (no copying allowed)
    scoped_array(const scoped_array &) = delete;
    scoped_array &operator=(const scoped_array &) = delete;

    // Move constructor
    scoped_array(scoped_array &&other) noexcept
        : arr_(other.arr_), deleter_(other.deleter_) {
        other.arr_ = nullptr;
        other.deleter_ = {};
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
    T &operator[](fl::size_t i) const { return arr_[i]; }

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
        deleter_(arr_);
        arr_ = arr;
    }

    void clear() { reset(); }

    T *release() {
        T *tmp = arr_;
        arr_ = nullptr;
        return tmp;
    }

    void swap(scoped_array &other) noexcept {
        T *tmp = arr_;
        arr_ = other.arr_;
        other.arr_ = tmp;
    }

  private:
    T *arr_; // Managed array pointer
    Deleter deleter_ = {};
};

// A variant of scoped_ptr where allocation is done completly via a fl::allocator.
template <typename T, typename Alloc = fl::allocator<T>> class scoped_array2 {
  public:
    FASTLED_DEPRECATED_CLASS("Use fl::vector<T, fl::allocator_psram<T>> instead");
    Alloc mAlloc; // Allocator instance to manage memory allocation
    // Constructor
    explicit scoped_array2(fl::size_t size = 0)
        : arr_(nullptr), size_(size) {
        if (size > 0) {
            arr_ = mAlloc.allocate(size);
            // Default initialize each element
            for (fl::size_t i = 0; i < size; ++i) {
                mAlloc.construct(&arr_[i]);
            }
        }
    }

    // Destructor
    ~scoped_array2() {
        if (arr_) {
            // Call destructor on each element
            for (fl::size_t i = 0; i < size_; ++i) {
                mAlloc.destroy(&arr_[i]);
            }
            mAlloc.deallocate(arr_, size_);
        }
    }

    // Disable copy semantics (no copying allowed)
    scoped_array2(const scoped_array2 &) = delete;
    scoped_array2 &operator=(const scoped_array2 &) = delete;

    // Move constructor
    scoped_array2(scoped_array2 &&other) noexcept
        : arr_(other.arr_), size_(other.size_) {
        other.arr_ = nullptr;
        other.size_ = 0;
    }

    // Move assignment operator
    scoped_array2 &operator=(scoped_array2 &&other) noexcept {
        if (this != &other) {
            reset();
            arr_ = other.arr_;
            size_ = other.size_;
            other.arr_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    // Array subscript operator
    T &operator[](fl::size_t i) const { return arr_[i]; }

    // Get the raw pointer
    T *get() const { return arr_; }

    // Get the size of the array
    fl::size_t size() const { return size_; }

    // Boolean conversion operator
    explicit operator bool() const noexcept { return arr_ != nullptr; }

    // Logical NOT operator
    bool operator!() const noexcept { return arr_ == nullptr; }

    // Release the managed array and reset the pointer
    void reset(fl::size_t new_size = 0) {
        if (arr_) {
            // Call destructor on each element
            for (fl::size_t i = 0; i < size_; ++i) {
                // arr_[i].~T();
                mAlloc.destroy(&arr_[i]);
            }
            // ::operator delete(arr_);
            mAlloc.deallocate(arr_, size_);
            arr_ = nullptr;
        }
        
        size_ = new_size;
        if (new_size > 0) {
            // arr_ = static_cast<T*>(::operator new(new_size * sizeof(T)));
            arr_ = mAlloc.allocate(new_size);
            // Default initialize each element
            for (fl::size_t i = 0; i < new_size; ++i) {
                // new (&arr_[i]) T();
                mAlloc.construct(&arr_[i]);
            }
        }
    }

    // Release ownership of the array
    T *release() {
        T *tmp = arr_;
        arr_ = nullptr;
        size_ = 0;
        return tmp;
    }

    void swap(scoped_array2 &other) noexcept {
        T *tmp_arr = arr_;
        fl::size_t tmp_size = size_;
        
        arr_ = other.arr_;
        size_ = other.size_;
        
        other.arr_ = tmp_arr;
        other.size_ = tmp_size;
    }

  private:
    T *arr_ = nullptr;     // Managed array pointer
    fl::size_t size_ = 0;      // Size of the array
};

} // namespace fl 
