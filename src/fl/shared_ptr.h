#pragma once

#include "fl/namespace.h"
#include "fl/type_traits.h"
#include "fl/utility.h"
#include "fl/stdint.h"
#include "fl/cstddef.h"
#include "fl/bit_cast.h"

namespace fl {

// Forward declarations
template<typename T> class shared_ptr;
template<typename T> class weak_ptr;

namespace detail {

// Tag type for make_shared constructor
struct make_shared_tag {};

// Simple control block structure - just manages reference counts
struct ControlBlockBase {
    volatile uint32_t shared_count;
    volatile uint32_t weak_count;
    
    ControlBlockBase() : shared_count(1), weak_count(1) {}
    virtual ~ControlBlockBase() = default;
    virtual void destroy_object() = 0;
    virtual void destroy_control_block() = 0;
};

// Default deleter implementation
template<typename T>
struct default_delete {
    void operator()(T* ptr) const {
        delete ptr;
    }
};

// Control block for external objects (via constructor)
template<typename T, typename Deleter = default_delete<T>>
struct ControlBlock : public ControlBlockBase {
    T* ptr;
    Deleter deleter;
    
    ControlBlock(T* p, Deleter d) : ptr(p), deleter(d) {}
    
    void destroy_object() override {
        if (ptr) {
            deleter(ptr);
            ptr = nullptr;
        }
    }
    
    void destroy_control_block() override {
        delete this;
    }
};

// Optimized control block for make_shared (object inlined)
template<typename T>
struct InlinedControlBlock : public ControlBlockBase {
    alignas(T) char object_storage[sizeof(T)];
    bool object_constructed;
    
    InlinedControlBlock() : object_constructed(false) {}
    
    T* get_object() { 
        return fl::bit_cast_ptr<T>(object_storage); 
    }
    
    const T* get_object() const { 
        return fl::bit_cast_ptr<T>(object_storage); 
    }
    
    void destroy_object() override {
        if (object_constructed) {
            get_object()->~T();
            object_constructed = false;
        }
    }
    
    void destroy_control_block() override {
        delete this;
    }
};

} // namespace detail

// std::shared_ptr compatible implementation
template<typename T>
class shared_ptr {
private:
    T* ptr_;
    detail::ControlBlockBase* control_block_;
    
    // Internal constructor for make_shared and weak_ptr conversion
    shared_ptr(T* ptr, detail::ControlBlockBase* control_block, detail::make_shared_tag) 
        : ptr_(ptr), control_block_(control_block) {
        // Control block was created with reference count 1, no need to increment
    }
        
    void release() {
        if (control_block_) {
            if (--control_block_->shared_count == 0) {
                control_block_->destroy_object();
                if (--control_block_->weak_count == 0) {
                    control_block_->destroy_control_block();
                }
            }
        }
    }
    
    void acquire() {
        if (control_block_) {
            ++control_block_->shared_count;
        }
    }

public:
    using element_type = T;
    using weak_type = weak_ptr<T>;
    
    // Default constructor
    shared_ptr() noexcept : ptr_(nullptr), control_block_(nullptr) {}
    shared_ptr(fl::nullptr_t) noexcept : ptr_(nullptr), control_block_(nullptr) {}
    
    // Constructor from raw pointer with default deleter
    template<typename Y>
    explicit shared_ptr(Y* ptr) : ptr_(ptr) {
        if (ptr_) {
            control_block_ = new detail::ControlBlock<Y>(ptr_, detail::default_delete<Y>{});
        } else {
            control_block_ = nullptr;
        }
    }
    
    // Constructor from raw pointer with custom deleter
    template<typename Y, typename Deleter>
    shared_ptr(Y* ptr, Deleter d) : ptr_(ptr) {
        if (ptr_) {
            control_block_ = new detail::ControlBlock<Y, Deleter>(ptr_, d);
        } else {
            control_block_ = nullptr;
        }
    }
    
    // Copy constructor
    shared_ptr(const shared_ptr& other) : ptr_(other.ptr_), control_block_(other.control_block_) {
        acquire();
    }
    
    // Converting copy constructor
    template<typename Y>
    shared_ptr(const shared_ptr<Y>& other) : ptr_(other.ptr_), control_block_(other.control_block_) {
        acquire();
    }
    
    // Move constructor
    shared_ptr(shared_ptr&& other) noexcept : ptr_(other.ptr_), control_block_(other.control_block_) {
        other.ptr_ = nullptr;
        other.control_block_ = nullptr;
    }
    
    // Converting move constructor
    template<typename Y>
    shared_ptr(shared_ptr<Y>&& other) noexcept : ptr_(other.ptr_), control_block_(other.control_block_) {
        other.ptr_ = nullptr;
        other.control_block_ = nullptr;
    }
    
    // Constructor from weak_ptr
    template<typename Y>
    explicit shared_ptr(const weak_ptr<Y>& weak);
    
    // Destructor
    ~shared_ptr() {
        release();
    }
    
    // Assignment operators
    shared_ptr& operator=(const shared_ptr& other) {
        if (this != &other) {
            release();
            ptr_ = other.ptr_;
            control_block_ = other.control_block_;
            acquire();
        }
        return *this;
    }
    
    template<typename Y>
    shared_ptr& operator=(const shared_ptr<Y>& other) {
        release();
        ptr_ = other.ptr_;
        control_block_ = other.control_block_;
        acquire();
        return *this;
    }
    
    shared_ptr& operator=(shared_ptr&& other) noexcept {
        if (this != &other) {
            release();
            ptr_ = other.ptr_;
            control_block_ = other.control_block_;
            other.ptr_ = nullptr;
            other.control_block_ = nullptr;
        }
        return *this;
    }
    
    template<typename Y>
    shared_ptr& operator=(shared_ptr<Y>&& other) noexcept {
        release();
        ptr_ = other.ptr_;
        control_block_ = other.control_block_;
        other.ptr_ = nullptr;
        other.control_block_ = nullptr;
        return *this;
    }
    
    // Modifiers
    void reset() noexcept {
        release();
        ptr_ = nullptr;
        control_block_ = nullptr;
    }
    
    template<typename Y>
    void reset(Y* ptr) {
        shared_ptr(ptr).swap(*this);
    }
    
    template<typename Y, typename Deleter>
    void reset(Y* ptr, Deleter d) {
        shared_ptr(ptr, d).swap(*this);
    }
    
    void swap(shared_ptr& other) noexcept {
        fl::swap(ptr_, other.ptr_);
        fl::swap(control_block_, other.control_block_);
    }
    
    // Observers
    T* get() const noexcept { return ptr_; }
    
    T& operator*() const noexcept { return *ptr_; }
    T* operator->() const noexcept { return ptr_; }
    
    T& operator[](ptrdiff_t idx) const { return ptr_[idx]; }
    
    long use_count() const noexcept {
        return control_block_ ? static_cast<long>(control_block_->shared_count) : 0;
    }
    
    bool unique() const noexcept { return use_count() == 1; }
    
    explicit operator bool() const noexcept { return ptr_ != nullptr; }
    
    // Comparison operators
    template<typename Y>
    bool operator==(const shared_ptr<Y>& other) const noexcept {
        return ptr_ == other.ptr_;
    }
    
    template<typename Y>
    bool operator!=(const shared_ptr<Y>& other) const noexcept {
        return ptr_ != other.ptr_;
    }
    
    template<typename Y>
    bool operator<(const shared_ptr<Y>& other) const noexcept {
        return ptr_ < other.ptr_;
    }
    
    template<typename Y>
    bool operator<=(const shared_ptr<Y>& other) const noexcept {
        return ptr_ <= other.ptr_;
    }
    
    template<typename Y>
    bool operator>(const shared_ptr<Y>& other) const noexcept {
        return ptr_ > other.ptr_;
    }
    
    template<typename Y>
    bool operator>=(const shared_ptr<Y>& other) const noexcept {
        return ptr_ >= other.ptr_;
    }
    
    bool operator==(fl::nullptr_t) const noexcept {
        return ptr_ == nullptr;
    }
    
    bool operator!=(fl::nullptr_t) const noexcept {
        return ptr_ != nullptr;
    }

private:
    template<typename Y> friend class shared_ptr;
    template<typename Y> friend class weak_ptr;
    
    template<typename Y, typename... Args>
    friend shared_ptr<Y> make_shared(Args&&... args);
    
    template<typename Y, typename A, typename... Args>
    friend shared_ptr<Y> allocate_shared(const A& alloc, Args&&... args);
};

// Factory functions

// make_shared with optimized inlined storage
template<typename T, typename... Args>
shared_ptr<T> make_shared(Args&&... args) {
    auto* control = new detail::InlinedControlBlock<T>();
    new(control->get_object()) T(fl::forward<Args>(args)...);
    control->object_constructed = true;
    return shared_ptr<T>(control->get_object(), control, detail::make_shared_tag{});
}

// allocate_shared (simplified version without full allocator support for now)
template<typename T, typename A, typename... Args>
shared_ptr<T> allocate_shared(const A& /* alloc */, Args&&... args) {
    // For now, just delegate to make_shared
    // Full allocator support would require more complex control block management
    return make_shared<T>(fl::forward<Args>(args)...);
}

// Non-member comparison operators
template<typename T, typename Y>
bool operator==(const shared_ptr<T>& lhs, const shared_ptr<Y>& rhs) noexcept {
    return lhs.get() == rhs.get();
}

template<typename T, typename Y>
bool operator!=(const shared_ptr<T>& lhs, const shared_ptr<Y>& rhs) noexcept {
    return lhs.get() != rhs.get();
}

template<typename T, typename Y>
bool operator<(const shared_ptr<T>& lhs, const shared_ptr<Y>& rhs) noexcept {
    return lhs.get() < rhs.get();
}

template<typename T, typename Y>
bool operator<=(const shared_ptr<T>& lhs, const shared_ptr<Y>& rhs) noexcept {
    return lhs.get() <= rhs.get();
}

template<typename T, typename Y>
bool operator>(const shared_ptr<T>& lhs, const shared_ptr<Y>& rhs) noexcept {
    return lhs.get() > rhs.get();
}

template<typename T, typename Y>
bool operator>=(const shared_ptr<T>& lhs, const shared_ptr<Y>& rhs) noexcept {
    return lhs.get() >= rhs.get();
}

template<typename T>
bool operator==(const shared_ptr<T>& lhs, fl::nullptr_t) noexcept {
    return lhs.get() == nullptr;
}

template<typename T>
bool operator==(fl::nullptr_t, const shared_ptr<T>& rhs) noexcept {
    return nullptr == rhs.get();
}

template<typename T>
bool operator!=(const shared_ptr<T>& lhs, fl::nullptr_t) noexcept {
    return lhs.get() != nullptr;
}

template<typename T>
bool operator!=(fl::nullptr_t, const shared_ptr<T>& rhs) noexcept {
    return nullptr != rhs.get();
}

// Utility functions
template<typename T>
void swap(shared_ptr<T>& lhs, shared_ptr<T>& rhs) noexcept {
    lhs.swap(rhs);
}

// Casts
template<typename T, typename Y>
shared_ptr<T> static_pointer_cast(const shared_ptr<Y>& other) noexcept {
    auto ptr = static_cast<T*>(other.get());
    return shared_ptr<T>(ptr, other.control_block_, detail::make_shared_tag{});
}


template<typename T, typename Y>
shared_ptr<T> const_pointer_cast(const shared_ptr<Y>& other) noexcept {
    auto ptr = const_cast<T*>(other.get());
    return shared_ptr<T>(ptr, other.control_block_, detail::make_shared_tag{});
}

template<typename T, typename Y>
shared_ptr<T> reinterpret_pointer_cast(const shared_ptr<Y>& other) noexcept {
    auto ptr = fl::bit_cast<T*>(other.get());
    return shared_ptr<T>(ptr, other.control_block_, detail::make_shared_tag{});
}

} // namespace fl 
