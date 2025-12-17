#pragma once

#include "fl/stl/allocator.h"
#include "fl/stl/new.h"
#include "fl/deprecated.h"
#include "fl/stl/cstddef.h"

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
    explicit scoped_array(T *arr = nullptr) : mArr(arr) {}
    scoped_array(T *arr, Deleter deleter) : mArr(arr), mDeleter(deleter) {}

    // Destructor
    ~scoped_array() { mDeleter(mArr); }

    // Disable copy semantics (no copying allowed)
    scoped_array(const scoped_array &) = delete;
    scoped_array &operator=(const scoped_array &) = delete;

    // Move constructor
    scoped_array(scoped_array &&other) noexcept
        : mArr(other.mArr), mDeleter(other.mDeleter) {
        other.mArr = nullptr;
        other.mDeleter = {};
    }

    // Move assignment operator
    scoped_array &operator=(scoped_array &&other) noexcept {
        if (this != &other) {
            reset(other.mArr);
            other.mArr = nullptr;
        }
        return *this;
    }

    // Array subscript operator
    T &operator[](fl::size_t i) const { return mArr[i]; }

    // Get the raw pointer
    T *get() const { return mArr; }

    // Boolean conversion operator
    explicit operator bool() const noexcept { return mArr != nullptr; }

    // Logical NOT operator
    bool operator!() const noexcept { return mArr == nullptr; }

    // Release the managed array and reset the pointer
    void reset(T *arr = nullptr) {
        if (mArr == arr) {
            return;
        }
        mDeleter(mArr);
        mArr = arr;
    }

    void clear() { reset(); }

    T *release() {
        T *tmp = mArr;
        mArr = nullptr;
        return tmp;
    }

    void swap(scoped_array &other) noexcept {
        T *tmp = mArr;
        mArr = other.mArr;
        other.mArr = tmp;
    }

  private:
    T *mArr; // Managed array pointer
    Deleter mDeleter = {};
};

// A variant of scoped_ptr where allocation is done completly via a fl::allocator.
template <typename T, typename Alloc = fl::allocator<T>> class scoped_array2 {
  public:
    FASTLED_DEPRECATED_CLASS("Use fl::vector<T, fl::allocator_psram<T>> instead");
    Alloc mAlloc; // Allocator instance to manage memory allocation
    // Constructor
    explicit scoped_array2(fl::size_t size = 0)
        : mArr(nullptr), mSize(size) {
        if (size > 0) {
            mArr = mAlloc.allocate(size);
            // Default initialize each element
            for (fl::size_t i = 0; i < size; ++i) {
                mAlloc.construct(&mArr[i]);
            }
        }
    }

    // Destructor
    ~scoped_array2() {
        if (mArr) {
            // Call destructor on each element
            for (fl::size_t i = 0; i < mSize; ++i) {
                mAlloc.destroy(&mArr[i]);
            }
            mAlloc.deallocate(mArr, mSize);
        }
    }

    // Disable copy semantics (no copying allowed)
    scoped_array2(const scoped_array2 &) = delete;
    scoped_array2 &operator=(const scoped_array2 &) = delete;

    // Move constructor
    scoped_array2(scoped_array2 &&other) noexcept
        : mArr(other.mArr), mSize(other.mSize) {
        other.mArr = nullptr;
        other.mSize = 0;
    }

    // Move assignment operator
    scoped_array2 &operator=(scoped_array2 &&other) noexcept {
        if (this != &other) {
            reset();
            mArr = other.mArr;
            mSize = other.mSize;
            other.mArr = nullptr;
            other.mSize = 0;
        }
        return *this;
    }

    // Array subscript operator
    T &operator[](fl::size_t i) const { return mArr[i]; }

    // Get the raw pointer
    T *get() const { return mArr; }

    // Get the size of the array
    fl::size_t size() const { return mSize; }

    // Boolean conversion operator
    explicit operator bool() const noexcept { return mArr != nullptr; }

    // Logical NOT operator
    bool operator!() const noexcept { return mArr == nullptr; }

    // Release the managed array and reset the pointer
    void reset(fl::size_t new_size = 0) {
        if (mArr) {
            // Call destructor on each element
            for (fl::size_t i = 0; i < mSize; ++i) {
                // mArr[i].~T();
                mAlloc.destroy(&mArr[i]);
            }
            // ::operator delete(mArr);
            mAlloc.deallocate(mArr, mSize);
            mArr = nullptr;
        }
        
        mSize = new_size;
        if (new_size > 0) {
            // mArr = static_cast<T*>(::operator new(new_size * sizeof(T)));
            mArr = mAlloc.allocate(new_size);
            // Default initialize each element
            for (fl::size_t i = 0; i < new_size; ++i) {
                // new (&mArr[i]) T();
                mAlloc.construct(&mArr[i]);
            }
        }
    }

    // Release ownership of the array
    T *release() {
        T *tmp = mArr;
        mArr = nullptr;
        mSize = 0;
        return tmp;
    }

    void swap(scoped_array2 &other) noexcept {
        T *tmp_arr = mArr;
        fl::size_t tmp_size = mSize;
        
        mArr = other.mArr;
        mSize = other.mSize;
        
        other.mArr = tmp_arr;
        other.mSize = tmp_size;
    }

  private:
    T *mArr = nullptr;     // Managed array pointer
    fl::size_t mSize = 0;      // Size of the array
};

// Helper function to create scoped_array (similar to make_unique)
template<typename T>
scoped_array<T> make_scoped_array(fl::size_t size) {
    return scoped_array<T>(new T[size]);
}

} // namespace fl
