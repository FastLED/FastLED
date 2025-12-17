#pragma once

#include "fl/stl/type_traits.h"
#include "fl/stl/utility.h"       // for fl::move, fl::forward, fl::swap
#include "fl/stl/stdint.h"        // for fl::size_t
#include "fl/stl/cstddef.h"       // for fl::nullptr_t
#include "fl/stl/initializer_list.h"  // for consistency (though not used in unique_ptr itself)

namespace fl {

template<typename T>
struct default_delete {
    void operator()(T* ptr) const {
        delete ptr;
    }
    
    // Allow conversion from derived to base type deleters
    template<typename U>
    default_delete(const default_delete<U>&) noexcept {}
    
    default_delete() = default;
};

template<typename T>
struct default_delete<T[]> {
    void operator()(T* ptr) const {
        delete[] ptr;
    }
};

template<typename T, typename Deleter = default_delete<T>>
class unique_ptr {
public:
    using element_type = T;
    using deleter_type = Deleter;
    using pointer = T*;

private:
    pointer mPtr;
    Deleter mDeleter;

public:
    // Constructors
    constexpr unique_ptr() noexcept : mPtr(nullptr), mDeleter() {}
    constexpr unique_ptr(fl::nullptr_t) noexcept : mPtr(nullptr), mDeleter() {}
    explicit unique_ptr(pointer p) noexcept : mPtr(p), mDeleter() {}
    unique_ptr(pointer p, const Deleter& d) noexcept : mPtr(p), mDeleter(d) {}
    unique_ptr(pointer p, Deleter&& d) noexcept : mPtr(p), mDeleter(fl::move(d)) {}
    
    // Move constructor
    unique_ptr(unique_ptr&& u) noexcept : mPtr(u.release()), mDeleter(fl::move(u.mDeleter)) {}
    
    // Converting move constructor
    template<typename U, typename E>
    unique_ptr(unique_ptr<U, E>&& u) noexcept 
        : mPtr(u.release()), mDeleter(fl::move(u.get_deleter())) {}
    
    // Copy semantics deleted
    unique_ptr(const unique_ptr&) = delete;
    unique_ptr& operator=(const unique_ptr&) = delete;
    
    // Move assignment
    unique_ptr& operator=(unique_ptr&& u) noexcept {
        if (this != &u) {
            reset(u.release());
            mDeleter = fl::move(u.mDeleter);
        }
        return *this;
    }
    
    // Converting move assignment
    template<typename U, typename E>
    unique_ptr& operator=(unique_ptr<U, E>&& u) noexcept {
        reset(u.release());
        mDeleter = fl::move(u.get_deleter());
        return *this;
    }
    
    // nullptr assignment
    unique_ptr& operator=(fl::nullptr_t) noexcept {
        reset();
        return *this;
    }
    
    // Destructor
    ~unique_ptr() {
        if (mPtr) {
            mDeleter(mPtr);
        }
    }
    
    // Observers
    pointer get() const noexcept { return mPtr; }
    Deleter& get_deleter() noexcept { return mDeleter; }
    const Deleter& get_deleter() const noexcept { return mDeleter; }
    explicit operator bool() const noexcept { return mPtr != nullptr; }
    
    // Access
    T& operator*() const { return *mPtr; }
    pointer operator->() const noexcept { return mPtr; }
    
    // Modifiers
    pointer release() noexcept {
        pointer tmp = mPtr;
        mPtr = nullptr;
        return tmp;
    }
    
    void reset(pointer p = nullptr) noexcept {
        pointer old_ptr = mPtr;
        mPtr = p;
        if (old_ptr) {
            mDeleter(old_ptr);
        }
    }
    
    void swap(unique_ptr& u) noexcept {
        using fl::swap;
        swap(mPtr, u.mPtr);
        swap(mDeleter, u.mDeleter);
    }
};

// Array specialization for scoped_array consolidation
template<typename T, typename Deleter>
class unique_ptr<T[], Deleter> {
public:
    using element_type = T;
    using deleter_type = Deleter;
    using pointer = T*;

private:
    pointer mPtr;
    Deleter mDeleter;

public:
    // Constructors
    constexpr unique_ptr() noexcept : mPtr(nullptr), mDeleter() {}
    constexpr unique_ptr(fl::nullptr_t) noexcept : mPtr(nullptr), mDeleter() {}
    explicit unique_ptr(pointer p) noexcept : mPtr(p), mDeleter() {}
    unique_ptr(pointer p, const Deleter& d) noexcept : mPtr(p), mDeleter(d) {}
    unique_ptr(pointer p, Deleter&& d) noexcept : mPtr(p), mDeleter(fl::move(d)) {}
    
    // Move constructor
    unique_ptr(unique_ptr&& u) noexcept : mPtr(u.release()), mDeleter(fl::move(u.mDeleter)) {}
    
    // Converting move constructor
    template<typename U, typename E>
    unique_ptr(unique_ptr<U, E>&& u) noexcept 
        : mPtr(u.release()), mDeleter(fl::move(u.get_deleter())) {}
    
    // Copy semantics deleted
    unique_ptr(const unique_ptr&) = delete;
    unique_ptr& operator=(const unique_ptr&) = delete;
    
    // Move assignment
    unique_ptr& operator=(unique_ptr&& u) noexcept {
        if (this != &u) {
            reset(u.release());
            mDeleter = fl::move(u.mDeleter);
        }
        return *this;
    }
    
    // Converting move assignment
    template<typename U, typename E>
    unique_ptr& operator=(unique_ptr<U, E>&& u) noexcept {
        reset(u.release());
        mDeleter = fl::move(u.get_deleter());
        return *this;
    }
    
    // nullptr assignment
    unique_ptr& operator=(fl::nullptr_t) noexcept {
        reset();
        return *this;
    }
    
    // Destructor
    ~unique_ptr() {
        if (mPtr) {
            mDeleter(mPtr);
        }
    }
    
    // Observers
    pointer get() const noexcept { return mPtr; }
    Deleter& get_deleter() noexcept { return mDeleter; }
    const Deleter& get_deleter() const noexcept { return mDeleter; }
    explicit operator bool() const noexcept { return mPtr != nullptr; }
    
    // Array access (replaces scoped_array functionality)
    T& operator[](fl::size_t i) const { return mPtr[i]; }
    
    // Modifiers
    pointer release() noexcept {
        pointer tmp = mPtr;
        mPtr = nullptr;
        return tmp;
    }
    
    void reset(pointer p = nullptr) noexcept {
        pointer old_ptr = mPtr;
        mPtr = p;
        if (old_ptr) {
            mDeleter(old_ptr);
        }
    }
    
    void swap(unique_ptr& u) noexcept {
        using fl::swap;
        swap(mPtr, u.mPtr);
        swap(mDeleter, u.mDeleter);
    }
};

// Non-member functions using FL equivalents
template<typename T, typename Deleter>
void swap(unique_ptr<T, Deleter>& lhs, unique_ptr<T, Deleter>& rhs) noexcept {
    lhs.swap(rhs);
}

// Comparison operators using FL equivalents
template<typename T1, typename Deleter1, typename T2, typename Deleter2>
bool operator==(const unique_ptr<T1, Deleter1>& lhs, const unique_ptr<T2, Deleter2>& rhs) {
    return lhs.get() == rhs.get();
}

template<typename T1, typename Deleter1, typename T2, typename Deleter2>
bool operator!=(const unique_ptr<T1, Deleter1>& lhs, const unique_ptr<T2, Deleter2>& rhs) {
    return !(lhs == rhs);
}

template<typename T, typename Deleter>
bool operator==(const unique_ptr<T, Deleter>& ptr, fl::nullptr_t) noexcept {
    return !ptr;
}

template<typename T, typename Deleter>
bool operator==(fl::nullptr_t, const unique_ptr<T, Deleter>& ptr) noexcept {
    return !ptr;
}

template<typename T, typename Deleter>
bool operator!=(const unique_ptr<T, Deleter>& ptr, fl::nullptr_t) noexcept {
    return static_cast<bool>(ptr);
}

template<typename T, typename Deleter>
bool operator!=(fl::nullptr_t, const unique_ptr<T, Deleter>& ptr) noexcept {
    return static_cast<bool>(ptr);
}

// make_unique function for consistency with std::make_unique
template<typename T, typename... Args>
typename fl::enable_if<!fl::is_array<T>::value, unique_ptr<T>>::type
make_unique(Args&&... args) {
    return unique_ptr<T>(new T(fl::forward<Args>(args)...));
}

// make_unique for arrays
template<typename T>
typename fl::enable_if<fl::is_array<T>::value, unique_ptr<T>>::type
make_unique(fl::size_t size) {
    typedef typename fl::remove_extent<T>::type U;
    return unique_ptr<T>(new U[size]());
}

} // namespace fl
