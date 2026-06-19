#pragma once

#include "fl/stl/shared_ptr.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/noexcept.h"

namespace fl {

// std::weak_ptr compatible implementation
template<typename T>
class weak_ptr {
private:
    T* mPtr;
    detail::ControlBlockBase* mControlBlock;

public:
    using element_type = T;
    
    // Default constructor
    weak_ptr() FL_NO_EXCEPT : mPtr(nullptr), mControlBlock(nullptr) {}
    
    // Copy constructor
    weak_ptr(const weak_ptr& other) FL_NO_EXCEPT : mPtr(other.mPtr), mControlBlock(other.mControlBlock) {
        if (mControlBlock) {
            ++mControlBlock->weak_count;
        }
    }
    
    // Converting copy constructor
    template<typename Y>
    weak_ptr(const weak_ptr<Y>& other) FL_NO_EXCEPT : mPtr(other.mPtr), mControlBlock(other.mControlBlock) {
        if (mControlBlock) {
            ++mControlBlock->weak_count;
        }
    }
    
    // Constructor from shared_ptr
    template<typename Y>
    weak_ptr(const shared_ptr<Y>& shared) FL_NO_EXCEPT : mPtr(shared.mPtr), mControlBlock(shared.mControlBlock) {
        if (mControlBlock) {
            ++mControlBlock->weak_count;
        }
    }
    
    // Move constructor
    weak_ptr(weak_ptr&& other) FL_NO_EXCEPT : mPtr(other.mPtr), mControlBlock(other.mControlBlock) {
        other.mPtr = nullptr;
        other.mControlBlock = nullptr;
    }
    
    // Converting move constructor
    template<typename Y>
    weak_ptr(weak_ptr<Y>&& other) FL_NO_EXCEPT : mPtr(other.mPtr), mControlBlock(other.mControlBlock) {
        other.mPtr = nullptr;
        other.mControlBlock = nullptr;
    }
    
    // Destructor
    ~weak_ptr() FL_NO_EXCEPT {
        release();
    }
    
    // Assignment operators
    weak_ptr& operator=(const weak_ptr& other) FL_NO_EXCEPT {
        if (this != &other) {
            release();
            mPtr = other.mPtr;
            mControlBlock = other.mControlBlock;
            if (mControlBlock) {
                ++mControlBlock->weak_count;
            }
        }
        return *this;
    }
    
    template<typename Y>
    weak_ptr& operator=(const weak_ptr<Y>& other) FL_NO_EXCEPT {
        release();
        mPtr = other.mPtr;
        mControlBlock = other.mControlBlock;
        if (mControlBlock) {
            ++mControlBlock->weak_count;
        }
        return *this;
    }
    
    template<typename Y>
    weak_ptr& operator=(const shared_ptr<Y>& shared) FL_NO_EXCEPT {
        release();
        mPtr = shared.mPtr;
        mControlBlock = shared.mControlBlock;
        if (mControlBlock) {
            ++mControlBlock->weak_count;
        }
        return *this;
    }
    
    weak_ptr& operator=(weak_ptr&& other) FL_NO_EXCEPT {
        if (this != &other) {
            release();
            mPtr = other.mPtr;
            mControlBlock = other.mControlBlock;
            other.mPtr = nullptr;
            other.mControlBlock = nullptr;
        }
        return *this;
    }
    
    template<typename Y>
    weak_ptr& operator=(weak_ptr<Y>&& other) FL_NO_EXCEPT {
        release();
        mPtr = other.mPtr;
        mControlBlock = other.mControlBlock;
        other.mPtr = nullptr;
        other.mControlBlock = nullptr;
        return *this;
    }
    
    // Modifiers
    void reset() FL_NO_EXCEPT {
        release();
        mPtr = nullptr;
        mControlBlock = nullptr;
    }
    
    void swap(weak_ptr& other) FL_NO_EXCEPT {
        fl::swap(mPtr, other.mPtr);
        fl::swap(mControlBlock, other.mControlBlock);
    }
    
    // Observers
    long use_count() const FL_NO_EXCEPT {
        return mControlBlock ? static_cast<long>(mControlBlock->shared_count) : 0;
    }
    
    bool expired() const FL_NO_EXCEPT { 
        return use_count() == 0; 
    }
    
    shared_ptr<T> lock() const FL_NO_EXCEPT {
        if (expired()) {
            return shared_ptr<T>();
        }
        
        // Try to acquire the shared pointer atomically
        if (mControlBlock && mControlBlock->shared_count > 0) {
            ++mControlBlock->shared_count;
            return shared_ptr<T>(mPtr, mControlBlock, detail::make_shared_tag{});
        }
        return shared_ptr<T>();
    }
    
    // Ownership (similar to shared_ptr interface)
    template<typename Y>
    bool owner_before(const weak_ptr<Y>& other) const FL_NO_EXCEPT {
        return mControlBlock < other.mControlBlock;
    }
    
    template<typename Y>
    bool owner_before(const shared_ptr<Y>& other) const FL_NO_EXCEPT {
        return mControlBlock < other.mControlBlock;
    }
    
    // Comparison operators (for compatibility with VectorSet)
    bool operator==(const weak_ptr& other) const FL_NO_EXCEPT {
        return mPtr == other.mPtr && mControlBlock == other.mControlBlock;
    }
    
    bool operator!=(const weak_ptr& other) const FL_NO_EXCEPT {
        return !(*this == other);
    }
    
    template<typename Y>
    bool operator==(const weak_ptr<Y>& other) const FL_NO_EXCEPT {
        return mPtr == other.mPtr && mControlBlock == other.mControlBlock;
    }
    
    template<typename Y>
    bool operator!=(const weak_ptr<Y>& other) const FL_NO_EXCEPT {
        return !(*this == other);
    }

private:
    void release() FL_NO_EXCEPT {
        if (mControlBlock) {
            if (--mControlBlock->weak_count == 0 && mControlBlock->shared_count == 0) {
                mControlBlock->destroy_control_block();
            }
        }
    }
    
    template<typename Y> friend class weak_ptr;
    template<typename Y> friend class shared_ptr;
};

// Utility functions
template<typename T>
void swap(weak_ptr<T>& lhs, weak_ptr<T>& rhs) FL_NO_EXCEPT {
    lhs.swap(rhs);
}

} // namespace fl

// Now we can complete the shared_ptr constructor that depends on weak_ptr
namespace fl {

template<typename T>
template<typename Y>
shared_ptr<T>::shared_ptr(const weak_ptr<Y>& weak) : mPtr(nullptr), mControlBlock(nullptr) {
    if (!weak.expired()) {
        if (weak.mControlBlock && weak.mControlBlock->shared_count > 0) {
            ++weak.mControlBlock->shared_count;
            mPtr = weak.mPtr;
            mControlBlock = weak.mControlBlock;
        }
    }
    if (!mPtr) {
        // If construction failed (object was destroyed), throw bad_weak_ptr equivalent
        // For now, just leave as default-constructed (nullptr)
    }
}

} // namespace fl
