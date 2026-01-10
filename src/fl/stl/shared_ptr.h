#pragma once

#include "fl/stl/type_traits.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/bit_cast.h"
#include "fl/stl/atomic.h"
#include "fl/int.h"


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

// Array deleter implementation for delete[]
template<typename T>
struct array_delete {
    void operator()(T* ptr) const {
        delete[] ptr;
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
    T* mPtr;
    detail::ControlBlockBase* mControlBlock;
    
    // Internal constructor for make_shared and weak_ptr conversion
    shared_ptr(T* ptr, detail::ControlBlockBase* control_block, detail::make_shared_tag) 
        : mPtr(ptr), mControlBlock(control_block) {
        // Control block was created with reference count 1, no need to increment
    }
    
    // Internal constructor for no-tracking
    shared_ptr(T* ptr, detail::ControlBlockBase* control_block, detail::no_tracking_tag) 
        : mPtr(ptr), mControlBlock(control_block) {
        // Control block created with no_tracking=true, no reference increment needed
    }
        
    // void release() {
    //     if (mControlBlock) {
    //         if (mControlBlock->remove_shared_ref()) {
    //             mControlBlock->destroy_object();
    //             if (--mControlBlock->weak_count == 0) {
    //                 mControlBlock->destroy_control_block();
    //             }
    //         }
    //     }
    // }
    
    void acquire() {
        if (mControlBlock) {
            mControlBlock->add_shared_ref();
        }
    }

public:
    using element_type = T;
    using weak_type = weak_ptr<T>;
    
    // Default constructor
    shared_ptr() noexcept : mPtr(nullptr), mControlBlock(nullptr) {}
    shared_ptr(fl::nullptr_t) noexcept : mPtr(nullptr), mControlBlock(nullptr) {}
    

    
    // Copy constructor
    shared_ptr(const shared_ptr& other) : mPtr(other.mPtr), mControlBlock(other.mControlBlock) {
        acquire();
    }
    
    // Converting copy constructor (allows upcasting: shared_ptr<Derived> → shared_ptr<Base>)
    template<typename Y, typename = typename fl::enable_if<fl::is_base_of<T, Y>::value>::type>
    shared_ptr(const shared_ptr<Y>& other) : mPtr(static_cast<T*>(other.mPtr)), mControlBlock(other.mControlBlock) {
        acquire();
    }
    
    // Move constructor
    shared_ptr(shared_ptr&& other) noexcept : mPtr(other.mPtr), mControlBlock(other.mControlBlock) {
        other.mPtr = nullptr;
        other.mControlBlock = nullptr;
    }
    
    // Converting move constructor (allows upcasting: shared_ptr<Derived> → shared_ptr<Base>)
    template<typename Y, typename = typename fl::enable_if<fl::is_base_of<T, Y>::value>::type>
    shared_ptr(shared_ptr<Y>&& other) noexcept : mPtr(static_cast<T*>(other.mPtr)), mControlBlock(other.mControlBlock) {
        other.mPtr = nullptr;
        other.mControlBlock = nullptr;
    }
    
    // Constructor from weak_ptr
    template<typename Y>
    explicit shared_ptr(const weak_ptr<Y>& weak);
    
    // Destructor
    ~shared_ptr() {
        //FASTLED_WARN("shared_ptr destructor called, mPtr=" << mPtr 
        //          << ", mControlBlock=" << mControlBlock);
        reset();
    }
    
    // Assignment operators
    shared_ptr& operator=(const shared_ptr& other) {
        if (this != &other) {
            reset();
            mPtr = other.mPtr;
            mControlBlock = other.mControlBlock;
            acquire();
        }
        return *this;
    }
    
    // Converting copy assignment (allows upcasting: shared_ptr<Derived> → shared_ptr<Base>)
    template<typename Y, typename = typename fl::enable_if<fl::is_base_of<T, Y>::value>::type>
    shared_ptr& operator=(const shared_ptr<Y>& other) {
        reset();
        mPtr = static_cast<T*>(other.mPtr);
        mControlBlock = other.mControlBlock;
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
    
    // Converting move assignment (allows upcasting: shared_ptr<Derived> → shared_ptr<Base>)
    template<typename Y, typename = typename fl::enable_if<fl::is_base_of<T, Y>::value>::type>
    shared_ptr& operator=(shared_ptr<Y>&& other) noexcept {
        if (static_cast<void*>(this) != static_cast<void*>(&other)) {
            reset();
            mPtr = static_cast<T*>(other.mPtr);
            mControlBlock = other.mControlBlock;
            other.mPtr = nullptr;
            other.mControlBlock = nullptr;
        }
        return *this;
    }
    
    // Modifiers
    void reset() noexcept {
        //FASTLED_WARN("shared_ptr::reset() called: mPtr=" << mPtr 
        //          << ", mControlBlock=" << mControlBlock);
        if (mControlBlock) {
            //FASTLED_WARN("control_block exists, calling remove_shared_ref()");
            if (mControlBlock->remove_shared_ref()) {
                //FASTLED_WARN("mControlBlock->remove_shared_ref() returned true, destroying object");
                mControlBlock->destroy_object();
                if (--mControlBlock->weak_count == 0) {
                    //FASTLED_WARN("weak_count reached 0, destroying control block");
                    mControlBlock->destroy_control_block();
                }
            }
        }
        mPtr = nullptr;
        mControlBlock = nullptr;
    }

    void reset(shared_ptr&& other) noexcept {
        this->swap(other);
        other.reset();
    }

    void swap(shared_ptr& other) noexcept {
        fl::swap(mPtr, other.mPtr);
        fl::swap(mControlBlock, other.mControlBlock);
    }

    void swap(shared_ptr&& other) noexcept {
        fl::swap(mPtr, other.mPtr);
        fl::swap(mControlBlock, other.mControlBlock);
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
    T* get() const noexcept { return mPtr; }
    
    T& operator*() const noexcept { return *mPtr; }
    T* operator->() const noexcept { return mPtr; }
    
    T& operator[](ptrdiff_t idx) const { return mPtr[idx]; }
    
    // NEW: use_count returns 0 for no-tracking shared_ptrs
    long use_count() const noexcept {
        if (!mControlBlock) return 0;
        if (mControlBlock->shared_count == detail::ControlBlockBase::NO_TRACKING_VALUE) {
            return 0;
        }
        return static_cast<long>(mControlBlock->shared_count);
    }
    
    bool unique() const noexcept { return use_count() == 1; }
    
    explicit operator bool() const noexcept { return mPtr != nullptr; }
    
    // NEW: Check if this is a no-tracking shared_ptr
    bool is_no_tracking() const noexcept {
        return mControlBlock && mControlBlock->is_no_tracking();
    }
    
    // Comparison operators for nullptr only (to avoid ambiguity with non-member operators)
    
    bool operator==(fl::nullptr_t) const noexcept {
        return mPtr == nullptr;
    }
    
    bool operator!=(fl::nullptr_t) const noexcept {
        return mPtr != nullptr;
    }

private:

    // Constructor from raw pointer with default deleter
    template<typename Y>
    explicit shared_ptr(Y* ptr) : mPtr(ptr) {
        if (mPtr) {
            mControlBlock = new detail::ControlBlock<Y>(ptr, detail::default_delete<Y>{});
        } else {
            mControlBlock = nullptr;
        }
    }
    
    // Constructor from raw pointer with custom deleter
    template<typename Y, typename Deleter>
    shared_ptr(Y* ptr, Deleter d) : mPtr(ptr) {
        if (mPtr) {
            mControlBlock = new detail::ControlBlock<Y, Deleter>(mPtr, d);
        } else {
            mControlBlock = nullptr;
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

    template<typename Y>
    friend shared_ptr<Y> make_shared_array(size_t n);
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

// make_shared_array for array allocations
template<typename T>
shared_ptr<T> make_shared_array(size_t n) {
    T* arr = new T[n]();  // Zero-initialize the array
    auto* control = new detail::ControlBlock<T, detail::array_delete<T>>(arr, detail::array_delete<T>{});
    return shared_ptr<T>(arr, control, detail::make_shared_tag{});
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
    return shared_ptr<T>(ptr, other.mControlBlock, detail::make_shared_tag{});
}


template<typename T, typename Y>
shared_ptr<T> const_pointer_cast(const shared_ptr<Y>& other) noexcept {
    auto ptr = const_cast<T*>(other.get());
    return shared_ptr<T>(ptr, other.mControlBlock, detail::make_shared_tag{});
}

template<typename T, typename Y>
shared_ptr<T> reinterpret_pointer_cast(const shared_ptr<Y>& other) noexcept {
    auto ptr = fl::bit_cast<T*>(other.get());
    return shared_ptr<T>(ptr, other.mControlBlock, detail::make_shared_tag{});
}

} // namespace fl

// Smart pointer typedef macros
// These create convenient typedefs like FooPtr = fl::shared_ptr<Foo>

// Forward declares a class and creates a shared_ptr typedef
// Example: FASTLED_SHARED_PTR(Foo) -> creates FooPtr as fl::shared_ptr<Foo>
#define FASTLED_SHARED_PTR(type)                                                \
    class type;                                                                \
    using type##Ptr = fl::shared_ptr<type>;

// Same as FASTLED_SHARED_PTR but for structs
#define FASTLED_SHARED_PTR_STRUCT(type)                                        \
    struct type;                                                               \
    using type##Ptr = fl::shared_ptr<type>;

// Creates typedef without forward declaration (for template instantiations)
// Example: using FooInt = Foo<int>; FASTLED_SHARED_PTR_NO_FWD(FooInt)
#define FASTLED_SHARED_PTR_NO_FWD(type) using type##Ptr = fl::shared_ptr<type>;

// Backward compatibility aliases (deprecated - prefer FASTLED_SHARED_PTR variants)
#define FASTLED_SMART_PTR(type) FASTLED_SHARED_PTR(type)
#define FASTLED_SMART_PTR_STRUCT(type) FASTLED_SHARED_PTR_STRUCT(type)
#define FASTLED_SMART_PTR_NO_FWD(type) FASTLED_SHARED_PTR_NO_FWD(type)
