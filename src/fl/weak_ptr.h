#pragma once

#include "fl/shared_ptr.h"
#include "fl/type_traits.h"

namespace fl {

// std::weak_ptr compatible implementation
template<typename T>
class weak_ptr {
private:
    T* ptr_;
    detail::ControlBlockBase* control_block_;

public:
    using element_type = T;
    
    // Default constructor
    weak_ptr() noexcept : ptr_(nullptr), control_block_(nullptr) {}
    
    // Copy constructor
    weak_ptr(const weak_ptr& other) noexcept : ptr_(other.ptr_), control_block_(other.control_block_) {
        if (control_block_) {
            ++control_block_->weak_count;
        }
    }
    
    // Converting copy constructor
    template<typename Y>
    weak_ptr(const weak_ptr<Y>& other) noexcept : ptr_(other.ptr_), control_block_(other.control_block_) {
        if (control_block_) {
            ++control_block_->weak_count;
        }
    }
    
    // Constructor from shared_ptr
    template<typename Y>
    weak_ptr(const shared_ptr<Y>& shared) noexcept : ptr_(shared.ptr_), control_block_(shared.control_block_) {
        if (control_block_) {
            ++control_block_->weak_count;
        }
    }
    
    // Move constructor
    weak_ptr(weak_ptr&& other) noexcept : ptr_(other.ptr_), control_block_(other.control_block_) {
        other.ptr_ = nullptr;
        other.control_block_ = nullptr;
    }
    
    // Converting move constructor
    template<typename Y>
    weak_ptr(weak_ptr<Y>&& other) noexcept : ptr_(other.ptr_), control_block_(other.control_block_) {
        other.ptr_ = nullptr;
        other.control_block_ = nullptr;
    }
    
    // Destructor
    ~weak_ptr() {
        release();
    }
    
    // Assignment operators
    weak_ptr& operator=(const weak_ptr& other) noexcept {
        if (this != &other) {
            release();
            ptr_ = other.ptr_;
            control_block_ = other.control_block_;
            if (control_block_) {
                ++control_block_->weak_count;
            }
        }
        return *this;
    }
    
    template<typename Y>
    weak_ptr& operator=(const weak_ptr<Y>& other) noexcept {
        release();
        ptr_ = other.ptr_;
        control_block_ = other.control_block_;
        if (control_block_) {
            ++control_block_->weak_count;
        }
        return *this;
    }
    
    template<typename Y>
    weak_ptr& operator=(const shared_ptr<Y>& shared) noexcept {
        release();
        ptr_ = shared.ptr_;
        control_block_ = shared.control_block_;
        if (control_block_) {
            ++control_block_->weak_count;
        }
        return *this;
    }
    
    weak_ptr& operator=(weak_ptr&& other) noexcept {
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
    weak_ptr& operator=(weak_ptr<Y>&& other) noexcept {
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
    
    void swap(weak_ptr& other) noexcept {
        fl::swap(ptr_, other.ptr_);
        fl::swap(control_block_, other.control_block_);
    }
    
    // Observers
    long use_count() const noexcept {
        return control_block_ ? static_cast<long>(control_block_->shared_count) : 0;
    }
    
    bool expired() const noexcept { 
        return use_count() == 0; 
    }
    
    shared_ptr<T> lock() const noexcept {
        if (expired()) {
            return shared_ptr<T>();
        }
        
        // Try to acquire the shared pointer atomically
        if (control_block_ && control_block_->shared_count > 0) {
            ++control_block_->shared_count;
            return shared_ptr<T>(ptr_, control_block_, detail::make_shared_tag{});
        }
        return shared_ptr<T>();
    }
    
    // Ownership (similar to shared_ptr interface)
    template<typename Y>
    bool owner_before(const weak_ptr<Y>& other) const noexcept {
        return control_block_ < other.control_block_;
    }
    
    template<typename Y>
    bool owner_before(const shared_ptr<Y>& other) const noexcept {
        return control_block_ < other.control_block_;
    }
    
    // Comparison operators (for compatibility with VectorSet)
    bool operator==(const weak_ptr& other) const noexcept {
        return ptr_ == other.ptr_ && control_block_ == other.control_block_;
    }
    
    bool operator!=(const weak_ptr& other) const noexcept {
        return !(*this == other);
    }
    
    template<typename Y>
    bool operator==(const weak_ptr<Y>& other) const noexcept {
        return ptr_ == other.ptr_ && control_block_ == other.control_block_;
    }
    
    template<typename Y>
    bool operator!=(const weak_ptr<Y>& other) const noexcept {
        return !(*this == other);
    }

private:
    void release() {
        if (control_block_) {
            if (--control_block_->weak_count == 0 && control_block_->shared_count == 0) {
                control_block_->destroy_control_block();
            }
        }
    }
    
    template<typename Y> friend class weak_ptr;
    template<typename Y> friend class shared_ptr;
};

// Utility functions
template<typename T>
void swap(weak_ptr<T>& lhs, weak_ptr<T>& rhs) noexcept {
    lhs.swap(rhs);
}

} // namespace fl

// Now we can complete the shared_ptr constructor that depends on weak_ptr
namespace fl {

template<typename T>
template<typename Y>
shared_ptr<T>::shared_ptr(const weak_ptr<Y>& weak) : ptr_(nullptr), control_block_(nullptr) {
    if (!weak.expired()) {
        if (weak.control_block_ && weak.control_block_->shared_count > 0) {
            ++weak.control_block_->shared_count;
            ptr_ = weak.ptr_;
            control_block_ = weak.control_block_;
        }
    }
    if (!ptr_) {
        // If construction failed (object was destroyed), throw bad_weak_ptr equivalent
        // For now, just leave as default-constructed (nullptr)
    }
}

} // namespace fl
