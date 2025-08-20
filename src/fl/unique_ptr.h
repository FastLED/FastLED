#pragma once

#include "fl/namespace.h"
#include "fl/type_traits.h"
#include "fl/utility.h"       // for fl::move, fl::forward, fl::swap
#include "fl/stdint.h"        // for fl::size_t
#include "fl/cstddef.h"       // for fl::nullptr_t
#include "fl/initializer_list.h"  // for consistency (though not used in unique_ptr itself)

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
    pointer ptr_;
    Deleter deleter_;

public:
    // Constructors
    constexpr unique_ptr() noexcept : ptr_(nullptr), deleter_() {}
    constexpr unique_ptr(fl::nullptr_t) noexcept : ptr_(nullptr), deleter_() {}
    explicit unique_ptr(pointer p) noexcept : ptr_(p), deleter_() {}
    unique_ptr(pointer p, const Deleter& d) noexcept : ptr_(p), deleter_(d) {}
    unique_ptr(pointer p, Deleter&& d) noexcept : ptr_(p), deleter_(fl::move(d)) {}
    
    // Move constructor
    unique_ptr(unique_ptr&& u) noexcept : ptr_(u.release()), deleter_(fl::move(u.deleter_)) {}
    
    // Converting move constructor
    template<typename U, typename E>
    unique_ptr(unique_ptr<U, E>&& u) noexcept 
        : ptr_(u.release()), deleter_(fl::move(u.get_deleter())) {}
    
    // Copy semantics deleted
    unique_ptr(const unique_ptr&) = delete;
    unique_ptr& operator=(const unique_ptr&) = delete;
    
    // Move assignment
    unique_ptr& operator=(unique_ptr&& u) noexcept {
        if (this != &u) {
            reset(u.release());
            deleter_ = fl::move(u.deleter_);
        }
        return *this;
    }
    
    // Converting move assignment
    template<typename U, typename E>
    unique_ptr& operator=(unique_ptr<U, E>&& u) noexcept {
        reset(u.release());
        deleter_ = fl::move(u.get_deleter());
        return *this;
    }
    
    // nullptr assignment
    unique_ptr& operator=(fl::nullptr_t) noexcept {
        reset();
        return *this;
    }
    
    // Destructor
    ~unique_ptr() {
        if (ptr_) {
            deleter_(ptr_);
        }
    }
    
    // Observers
    pointer get() const noexcept { return ptr_; }
    Deleter& get_deleter() noexcept { return deleter_; }
    const Deleter& get_deleter() const noexcept { return deleter_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }
    
    // Access
    T& operator*() const { return *ptr_; }
    pointer operator->() const noexcept { return ptr_; }
    
    // Modifiers
    pointer release() noexcept {
        pointer tmp = ptr_;
        ptr_ = nullptr;
        return tmp;
    }
    
    void reset(pointer p = nullptr) noexcept {
        pointer old_ptr = ptr_;
        ptr_ = p;
        if (old_ptr) {
            deleter_(old_ptr);
        }
    }
    
    void swap(unique_ptr& u) noexcept {
        using fl::swap;
        swap(ptr_, u.ptr_);
        swap(deleter_, u.deleter_);
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
    pointer ptr_;
    Deleter deleter_;

public:
    // Constructors
    constexpr unique_ptr() noexcept : ptr_(nullptr), deleter_() {}
    constexpr unique_ptr(fl::nullptr_t) noexcept : ptr_(nullptr), deleter_() {}
    explicit unique_ptr(pointer p) noexcept : ptr_(p), deleter_() {}
    unique_ptr(pointer p, const Deleter& d) noexcept : ptr_(p), deleter_(d) {}
    unique_ptr(pointer p, Deleter&& d) noexcept : ptr_(p), deleter_(fl::move(d)) {}
    
    // Move constructor
    unique_ptr(unique_ptr&& u) noexcept : ptr_(u.release()), deleter_(fl::move(u.deleter_)) {}
    
    // Converting move constructor
    template<typename U, typename E>
    unique_ptr(unique_ptr<U, E>&& u) noexcept 
        : ptr_(u.release()), deleter_(fl::move(u.get_deleter())) {}
    
    // Copy semantics deleted
    unique_ptr(const unique_ptr&) = delete;
    unique_ptr& operator=(const unique_ptr&) = delete;
    
    // Move assignment
    unique_ptr& operator=(unique_ptr&& u) noexcept {
        if (this != &u) {
            reset(u.release());
            deleter_ = fl::move(u.deleter_);
        }
        return *this;
    }
    
    // Converting move assignment
    template<typename U, typename E>
    unique_ptr& operator=(unique_ptr<U, E>&& u) noexcept {
        reset(u.release());
        deleter_ = fl::move(u.get_deleter());
        return *this;
    }
    
    // nullptr assignment
    unique_ptr& operator=(fl::nullptr_t) noexcept {
        reset();
        return *this;
    }
    
    // Destructor
    ~unique_ptr() {
        if (ptr_) {
            deleter_(ptr_);
        }
    }
    
    // Observers
    pointer get() const noexcept { return ptr_; }
    Deleter& get_deleter() noexcept { return deleter_; }
    const Deleter& get_deleter() const noexcept { return deleter_; }
    explicit operator bool() const noexcept { return ptr_ != nullptr; }
    
    // Array access (replaces scoped_array functionality)
    T& operator[](fl::size_t i) const { return ptr_[i]; }
    
    // Modifiers
    pointer release() noexcept {
        pointer tmp = ptr_;
        ptr_ = nullptr;
        return tmp;
    }
    
    void reset(pointer p = nullptr) noexcept {
        pointer old_ptr = ptr_;
        ptr_ = p;
        if (old_ptr) {
            deleter_(old_ptr);
        }
    }
    
    void swap(unique_ptr& u) noexcept {
        using fl::swap;
        swap(ptr_, u.ptr_);
        swap(deleter_, u.deleter_);
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

} // namespace fl 
