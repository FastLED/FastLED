#pragma once

#include "fl/namespace.h"
#include "fl/type_traits.h"
#include "fl/utility.h"
#include "fl/stdint.h"
#include "fl/cstddef.h"
#include "fl/bit_cast.h"
#include "fl/atomic.h"


namespace fl {

// Forward declarations
template<typename T> class shared_ptr;
template<typename T> class weak_ptr;

namespace detail {

// Tag type for make_shared constructor
struct make_shared_tag {};

// No-tracking tag for make_shared_no_tracking
struct no_tracking_tag {};

// Enhanced control block structure with no-tracking support using special value
struct ControlBlockBase {
    fl::atomic_u32 shared_count;
    fl::atomic_u32 weak_count;
    
    // Special value indicating no-tracking mode
    static constexpr fl::u32 NO_TRACKING_VALUE = 0xffffffff;
    
    ControlBlockBase(bool track = true) 
        : shared_count(track ? 1 : NO_TRACKING_VALUE), weak_count(1) {}
    virtual ~ControlBlockBase() = default;
    virtual void destroy_object() = 0;
    virtual void destroy_control_block() = 0;
    
    // NEW: No-tracking aware increment/decrement
    void add_shared_ref() {
        if (shared_count != NO_TRACKING_VALUE) {
            ++shared_count;
        }
    }
    
    bool remove_shared_ref() {
        //FASTLED_WARN("ControlBlockBase::remove_shared_ref() called: this=" << this);
        if (shared_count == NO_TRACKING_VALUE) {
            //FASTLED_WARN("In no-tracking mode, returning false");
            return false;  // Never destroy in no-tracking mode
        }
        bool result = (--shared_count == 0);
        //FASTLED_WARN("After decrement, returning: " << result);
        return result;
    }
    
    // Check if this control block is in no-tracking mode
    bool is_no_tracking() const {
        return shared_count == NO_TRACKING_VALUE;
    }
};

// Default deleter implementation
template<typename T>
struct default_delete {
    void operator()(T* ptr) const {
        delete ptr;
    }
};

// Deleter that does nothing (for stack/static objects)
template<typename T>
struct no_op_deleter {
    void operator()(T*) const {
        // Intentionally do nothing - object lifetime managed externally
    }
};

// Enhanced control block for external objects with no-tracking support
template<typename T, typename Deleter = default_delete<T>>
struct ControlBlock : public ControlBlockBase {
    T* ptr;
    Deleter deleter;
    
    ControlBlock(T* p, Deleter d = Deleter(), bool track = true) 
        : ControlBlockBase(track), ptr(p), deleter(d) {}
    
    void destroy_object() override {
        if (ptr && !is_no_tracking()) {  // Only delete if tracking
            deleter(ptr);
            ptr = nullptr;
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
    
    // Internal constructor for no-tracking
    shared_ptr(T* ptr, detail::ControlBlockBase* control_block, detail::no_tracking_tag) 
        : ptr_(ptr), control_block_(control_block) {
        // Control block created with no_tracking=true, no reference increment needed
    }
        
    // void release() {
    //     if (control_block_) {
    //         if (control_block_->remove_shared_ref()) {
    //             control_block_->destroy_object();
    //             if (--control_block_->weak_count == 0) {
    //                 control_block_->destroy_control_block();
    //             }
    //         }
    //     }
    // }
    
    void acquire() {
        if (control_block_) {
            control_block_->add_shared_ref();
        }
    }

public:
    using element_type = T;
    using weak_type = weak_ptr<T>;
    
    // Default constructor
    shared_ptr() noexcept : ptr_(nullptr), control_block_(nullptr) {}
    shared_ptr(fl::nullptr_t) noexcept : ptr_(nullptr), control_block_(nullptr) {}
    

    
    // Copy constructor
    shared_ptr(const shared_ptr& other) : ptr_(other.ptr_), control_block_(other.control_block_) {
        acquire();
    }
    
    // Converting copy constructor
    template<typename Y, typename = typename fl::enable_if<fl::is_base_of<T, Y>::value || fl::is_base_of<Y, T>::value>::type>
    shared_ptr(const shared_ptr<Y>& other) : ptr_(static_cast<T*>(other.ptr_)), control_block_(other.control_block_) {
        acquire();
    }
    
    // Move constructor
    shared_ptr(shared_ptr&& other) noexcept : ptr_(other.ptr_), control_block_(other.control_block_) {
        other.ptr_ = nullptr;
        other.control_block_ = nullptr;
    }
    
    // Converting move constructor
    template<typename Y, typename = typename fl::enable_if<fl::is_base_of<T, Y>::value || fl::is_base_of<Y, T>::value>::type>
    shared_ptr(shared_ptr<Y>&& other) noexcept : ptr_(static_cast<T*>(other.ptr_)), control_block_(other.control_block_) {
        other.ptr_ = nullptr;
        other.control_block_ = nullptr;
    }
    
    // Constructor from weak_ptr
    template<typename Y>
    explicit shared_ptr(const weak_ptr<Y>& weak);
    
    // Destructor
    ~shared_ptr() {
        //FASTLED_WARN("shared_ptr destructor called, ptr_=" << ptr_ 
        //          << ", control_block_=" << control_block_);
        reset();
    }
    
    // Assignment operators
    shared_ptr& operator=(const shared_ptr& other) {
        if (this != &other) {
            reset();
            ptr_ = other.ptr_;
            control_block_ = other.control_block_;
            acquire();
        }
        return *this;
    }
    
    template<typename Y>
    shared_ptr& operator=(const shared_ptr<Y>& other) {
        reset();
        ptr_ = other.ptr_;
        control_block_ = other.control_block_;
        acquire();
        return *this;
    }
    
    shared_ptr& operator=(shared_ptr&& other) noexcept {
        if (this != &other) {
            this->swap(other);
            other.reset();
        }
        return *this;
    }
    
    template<typename Y>
    shared_ptr& operator=(shared_ptr<Y>&& other) noexcept {
        if (static_cast<void*>(this) != static_cast<void*>(&other)) {
            this->swap(other);
            other.reset();
        }
        return *this;
    }
    
    // Modifiers
    void reset() noexcept {
        //FASTLED_WARN("shared_ptr::reset() called: ptr_=" << ptr_ 
        //          << ", control_block_=" << control_block_);
        if (control_block_) {
            //FASTLED_WARN("control_block exists, calling remove_shared_ref()");
            if (control_block_->remove_shared_ref()) {
                //FASTLED_WARN("control_block_->remove_shared_ref() returned true, destroying object");
                control_block_->destroy_object();
                if (--control_block_->weak_count == 0) {
                    //FASTLED_WARN("weak_count reached 0, destroying control block");
                    control_block_->destroy_control_block();
                }
            }
        }
        ptr_ = nullptr;
        control_block_ = nullptr;
    }

    void reset(shared_ptr&& other) noexcept {
        this->swap(other);
        other.reset();
    }

    void swap(shared_ptr& other) noexcept {
        fl::swap(ptr_, other.ptr_);
        fl::swap(control_block_, other.control_block_);
    }

    void swap(shared_ptr&& other) noexcept {
        fl::swap(ptr_, other.ptr_);
        fl::swap(control_block_, other.control_block_);
    }


    
    // template<typename Y>
    // void reset(Y* ptr) {
    //     shared_ptr(ptr).swap(*this);
    // }
    
    // template<typename Y, typename Deleter>
    // void reset(Y* ptr, Deleter d) {
    //     shared_ptr(ptr, d).swap(*this);
    // }
    

    
    // Observers
    T* get() const noexcept { return ptr_; }
    
    T& operator*() const noexcept { return *ptr_; }
    T* operator->() const noexcept { return ptr_; }
    
    T& operator[](ptrdiff_t idx) const { return ptr_[idx]; }
    
    // NEW: use_count returns 0 for no-tracking shared_ptrs
    long use_count() const noexcept {
        if (!control_block_) return 0;
        if (control_block_->shared_count == detail::ControlBlockBase::NO_TRACKING_VALUE) {
            return 0;
        }
        return static_cast<long>(control_block_->shared_count);
    }
    
    bool unique() const noexcept { return use_count() == 1; }
    
    explicit operator bool() const noexcept { return ptr_ != nullptr; }
    
    // NEW: Check if this is a no-tracking shared_ptr
    bool is_no_tracking() const noexcept {
        return control_block_ && control_block_->is_no_tracking();
    }
    
    // Comparison operators for nullptr only (to avoid ambiguity with non-member operators)
    
    bool operator==(fl::nullptr_t) const noexcept {
        return ptr_ == nullptr;
    }
    
    bool operator!=(fl::nullptr_t) const noexcept {
        return ptr_ != nullptr;
    }

private:

    // Constructor from raw pointer with default deleter
    template<typename Y>
    explicit shared_ptr(Y* ptr) : ptr_(ptr) {
        if (ptr_) {
            control_block_ = new detail::ControlBlock<Y>(ptr, detail::default_delete<Y>{});
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

    template<typename Y> friend class shared_ptr;
    template<typename Y> friend class weak_ptr;
    
    template<typename Y, typename... Args>
    friend shared_ptr<Y> make_shared(Args&&... args);

    template<typename Y, typename Deleter, typename... Args>
    friend shared_ptr<Y> make_shared_with_deleter(Deleter d, Args&&... args);
    
    template<typename Y, typename A, typename... Args>
    friend shared_ptr<Y> allocate_shared(const A& alloc, Args&&... args);
    
    template<typename Y>
    friend shared_ptr<Y> make_shared_no_tracking(Y& obj);
};

// Factory functions

// make_shared with optimized inlined storage
template<typename T, typename... Args>
shared_ptr<T> make_shared(Args&&... args) {
    T* obj = new T(fl::forward<Args>(args)...);
    auto* control = new detail::ControlBlock<T>(obj);
    //FASTLED_WARN("make_shared created object at " << obj 
    //          << " with control block at " << control);
    //new(control->get_object()) T(fl::forward<Args>(args)...);
    //control->object_constructed = true;
    return shared_ptr<T>(obj, control, detail::make_shared_tag{});
}

template<typename T, typename Deleter, typename... Args>
shared_ptr<T> make_shared_with_deleter(Deleter d, Args&&... args) {
    T* obj = new T(fl::forward<Args>(args)...);
    auto* control = new detail::ControlBlock<T, Deleter>(obj, d);
    //new(control->get_object()) T(fl::forward<Args>(args)...);
    //control->object_constructed = true;
    return shared_ptr<T>(obj, control, detail::make_shared_tag{});
}

namespace detail {
    template<typename T>
    struct NoDeleter {
        void operator()(T*) const {
            // Intentionally do nothing - object lifetime managed externally
        }
    };
}

// NEW: Creates a shared_ptr that does not modify the reference count
// The shared_ptr and any copies will not affect object lifetime
template<typename T>
shared_ptr<T> make_shared_no_tracking(T& obj) {
    auto* control = new detail::ControlBlock<T, detail::NoDeleter<T>>(&obj, detail::NoDeleter<T>{}, false);  // track = false (enables no-tracking mode)
    return shared_ptr<T>(&obj, control, detail::no_tracking_tag{});
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
