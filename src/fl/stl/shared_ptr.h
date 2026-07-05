#pragma once

#include "fl/stl/type_traits.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/bit_cast.h"
#include "fl/stl/atomic.h"
#include "fl/stl/int.h"
#include "fl/stl/align.h"
#include "fl/stl/cstdlib.h"
#include "fl/stl/noexcept.h"


namespace fl {

// Forward declarations
template<typename T> class shared_ptr;  // IWYU pragma: keep
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
    
    ControlBlockBase(bool track = true) FL_NO_EXCEPT
        : shared_count(track ? 1 : NO_TRACKING_VALUE), weak_count(1) {}
    // Destructor defined out-of-line in shared_ptr.cpp.hpp to anchor vtable
    // to a single translation unit, preventing ODR violations when using shared libraries.
    virtual ~ControlBlockBase() FL_NO_EXCEPT;
    virtual void destroy_object() FL_NO_EXCEPT = 0;
    virtual void destroy_control_block() FL_NO_EXCEPT = 0;
    
    // Reference counting functions - defined out-of-line in shared_ptr.cpp.hpp
    // to prevent UBSAN vptr mismatch errors when objects cross shared library boundaries.
    // When these are inline, different binaries get different vtables, and UBSAN
    // detects the mismatch when checking member access.
    void add_shared_ref() FL_NO_EXCEPT;
    bool remove_shared_ref() FL_NO_EXCEPT;

    // Check if this control block is in no-tracking mode
    bool is_no_tracking() const FL_NO_EXCEPT;
};

// Default deleter implementation
template<typename T>
struct default_delete {
    void operator()(T* ptr) const FL_NO_EXCEPT {
        delete ptr;
    }
};

// Deleter that does nothing (for stack/static objects)
template<typename T>
struct no_op_deleter {
    void operator()(T*) const FL_NO_EXCEPT {
        // Intentionally do nothing - object lifetime managed externally
    }
};

// Array deleter implementation for delete[]
template<typename T>
struct array_delete {
    void operator()(T* ptr) const FL_NO_EXCEPT {
        delete[] ptr;
    }
};

// Enhanced control block for external objects with no-tracking support
template<typename T, typename Deleter = default_delete<T>>
struct ControlBlock : public ControlBlockBase {
    T* ptr;
    Deleter deleter;

    ControlBlock(T* p, Deleter d = Deleter(), bool track = true) FL_NO_EXCEPT
        : ControlBlockBase(track), ptr(p), deleter(d) {}

    void destroy_object() FL_NO_EXCEPT override {
        if (ptr && !is_no_tracking()) {  // Only delete if tracking
            deleter(ptr);
            ptr = nullptr;
        }
    }

    void destroy_control_block() FL_NO_EXCEPT override {
        delete this;
    }
};

// Helper to compute maximum alignment for InlinedControlBlock
// Ensures alignment is at least as strict as ControlBlockBase, but respects T's requirements if larger
template<typename T>
struct control_block_alignment {
    static constexpr fl::size value =
        (alignof(T) > alignof(ControlBlockBase)) ? alignof(T) : alignof(ControlBlockBase);
};

// Inlined control block for make_shared optimization
// Stores the object inline with the control block (single allocation)
template<typename T>
struct FL_ALIGNAS(control_block_alignment<T>::value) InlinedControlBlock : public ControlBlockBase {
    FL_ALIGNAS(T) char storage[sizeof(T)];  // Object storage (properly aligned)
    bool object_constructed;                 // Track construction state

    InlinedControlBlock() FL_NO_EXCEPT
        : ControlBlockBase(true), object_constructed(false) {}

    // Aligned operator new/delete: when T has alignment > default new alignment
    // (e.g. FL_ALIGNAS(64)), plain `new` won't honour it on pre-C++17 compilers.
    // GCC emits -Waligned-new in that case.  These overrides route through
    // fl::aligned_alloc / fl::aligned_free so the block is always properly aligned.
    static void* operator new(fl::size_t size) FL_NO_EXCEPT {
        constexpr fl::size_t align = control_block_alignment<T>::value;
        return fl::aligned_alloc(align, size);
    }
    static void operator delete(void* ptr) {
        fl::aligned_free(ptr);
    }

    // Get pointer to the inline object storage
    T* get_object() FL_NO_EXCEPT {
        return fl::bit_cast<T*>(&storage[0]);
    }

    const T* get_object() const FL_NO_EXCEPT {
        return fl::bit_cast<const T*>(&storage[0]);
    }

    void destroy_object() FL_NO_EXCEPT override {
        if (object_constructed) {
            get_object()->~T();  // Manual destructor call
            object_constructed = false;
        }
    }

    void destroy_control_block() FL_NO_EXCEPT override {
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
    shared_ptr(T* ptr, detail::ControlBlockBase* control_block, detail::make_shared_tag) FL_NO_EXCEPT
        : mPtr(ptr), mControlBlock(control_block) {
        // Control block was created with reference count 1, no need to increment
    }
    
    // Internal constructor for no-tracking (control_block should be nullptr)
    shared_ptr(T* ptr, detail::ControlBlockBase* control_block, detail::no_tracking_tag) FL_NO_EXCEPT
        : mPtr(ptr), mControlBlock(control_block) {
        // No control block - this is a zero-overhead wrapper around a raw pointer
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
    
    void acquire() FL_NO_EXCEPT {
        if (mControlBlock) {
            mControlBlock->add_shared_ref();
        }
    }

public:
    using element_type = T;
    using weak_type = weak_ptr<T>;
    
    // Default constructor
    shared_ptr() FL_NO_EXCEPT : mPtr(nullptr), mControlBlock(nullptr) {}
    shared_ptr(fl::nullptr_t) FL_NO_EXCEPT : mPtr(nullptr), mControlBlock(nullptr) {}
    

    
    // Copy constructor
    shared_ptr(const shared_ptr& other) FL_NO_EXCEPT : mPtr(other.mPtr), mControlBlock(other.mControlBlock) {
        acquire();
    }
    
    // Converting copy constructor (allows upcasting: shared_ptr<Derived> → shared_ptr<Base>)
    template<typename Y, typename = typename fl::enable_if<fl::is_base_of<T, Y>::value>::type>
    shared_ptr(const shared_ptr<Y>& other) FL_NO_EXCEPT : mPtr(static_cast<T*>(other.mPtr)), mControlBlock(other.mControlBlock) {
        acquire();
    }
    
    // Move constructor
    shared_ptr(shared_ptr&& other) FL_NO_EXCEPT : mPtr(other.mPtr), mControlBlock(other.mControlBlock) {
        other.mPtr = nullptr;
        other.mControlBlock = nullptr;
    }
    
    // Converting move constructor (allows upcasting: shared_ptr<Derived> → shared_ptr<Base>)
    template<typename Y, typename = typename fl::enable_if<fl::is_base_of<T, Y>::value>::type>
    shared_ptr(shared_ptr<Y>&& other) FL_NO_EXCEPT : mPtr(static_cast<T*>(other.mPtr)), mControlBlock(other.mControlBlock) {
        other.mPtr = nullptr;
        other.mControlBlock = nullptr;
    }
    
    // Constructor from weak_ptr
    template<typename Y>
    explicit shared_ptr(const weak_ptr<Y>& weak) FL_NO_EXCEPT;

    // Aliasing constructor (C++11 standard §20.7.2.2.1)
    // Shares ownership with 'other' but stores 'ptr'
    // Used by pointer cast functions (static_pointer_cast, etc.)
    template<typename Y>
    shared_ptr(const shared_ptr<Y>& other, T* ptr) FL_NO_EXCEPT
        : mPtr(ptr), mControlBlock(other.mControlBlock) {
        acquire();
    }
    
    // Destructor
    ~shared_ptr() FL_NO_EXCEPT {
        //FL_WARN("shared_ptr destructor called, mPtr=" << mPtr 
        //          << ", mControlBlock=" << mControlBlock);
        reset();
    }
    
    // Assignment operators
    shared_ptr& operator=(const shared_ptr& other) FL_NO_EXCEPT {
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
    shared_ptr& operator=(const shared_ptr<Y>& other) FL_NO_EXCEPT {
        reset();
        mPtr = static_cast<T*>(other.mPtr);
        mControlBlock = other.mControlBlock;
        acquire();
        return *this;
    }
    
    shared_ptr& operator=(shared_ptr&& other) FL_NO_EXCEPT {
        if (this != &other) {
            this->swap(other);
            other.reset();
        }
        return *this;
    }
    
    // Converting move assignment (allows upcasting: shared_ptr<Derived> → shared_ptr<Base>)
    template<typename Y, typename = typename fl::enable_if<fl::is_base_of<T, Y>::value>::type>
    shared_ptr& operator=(shared_ptr<Y>&& other) FL_NO_EXCEPT {
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
    void reset() FL_NO_EXCEPT {
        //FL_WARN("shared_ptr::reset() called: mPtr=" << mPtr 
        //          << ", mControlBlock=" << mControlBlock);
        if (mControlBlock) {
            //FL_WARN("control_block exists, calling remove_shared_ref()");
            if (mControlBlock->remove_shared_ref()) {
                //FL_WARN("mControlBlock->remove_shared_ref() returned true, destroying object");
                mControlBlock->destroy_object();
                if (--mControlBlock->weak_count == 0) {
                    //FL_WARN("weak_count reached 0, destroying control block");
                    mControlBlock->destroy_control_block();
                }
            }
        }
        mPtr = nullptr;
        mControlBlock = nullptr;
    }

    void reset(shared_ptr&& other) FL_NO_EXCEPT {
        this->swap(other);
        other.reset();
    }

    void swap(shared_ptr& other) FL_NO_EXCEPT {
        fl::swap(mPtr, other.mPtr);
        fl::swap(mControlBlock, other.mControlBlock);
    }

    void swap(shared_ptr&& other) FL_NO_EXCEPT {
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
    T* get() const FL_NO_EXCEPT { return mPtr; }
    
    T& operator*() const FL_NO_EXCEPT { return *mPtr; }
    T* operator->() const FL_NO_EXCEPT { return mPtr; }
    
    T& operator[](ptrdiff_t idx) const FL_NO_EXCEPT { return mPtr[idx]; }
    
    // NEW: use_count returns 0 for no-tracking shared_ptrs
    long use_count() const FL_NO_EXCEPT {
        if (!mControlBlock) return 0;
        if (mControlBlock->shared_count == detail::ControlBlockBase::NO_TRACKING_VALUE) {
            return 0;
        }
        return static_cast<long>(mControlBlock->shared_count);
    }
    
    bool unique() const FL_NO_EXCEPT { return use_count() == 1; }
    
    explicit operator bool() const FL_NO_EXCEPT { return mPtr != nullptr; }
    
    // NEW: Check if this is a no-tracking shared_ptr
    bool is_no_tracking() const FL_NO_EXCEPT {
        return mControlBlock && mControlBlock->is_no_tracking();
    }
    
    // Comparison operators for nullptr only (to avoid ambiguity with non-member operators)
    
    bool operator==(fl::nullptr_t) const FL_NO_EXCEPT {
        return mPtr == nullptr;
    }
    
    bool operator!=(fl::nullptr_t) const FL_NO_EXCEPT {
        return mPtr != nullptr;
    }

private:

    // Constructor from raw pointer with default deleter
    template<typename Y>
    explicit shared_ptr(Y* ptr) FL_NO_EXCEPT : mPtr(ptr) {
        if (mPtr) {
            mControlBlock = new detail::ControlBlock<Y>(ptr, detail::default_delete<Y>{});
        } else {
            mControlBlock = nullptr;
        }
    }
    
    // Constructor from raw pointer with custom deleter
    template<typename Y, typename Deleter>
    shared_ptr(Y* ptr, Deleter d) FL_NO_EXCEPT : mPtr(ptr) {
        if (mPtr) {
            mControlBlock = new detail::ControlBlock<Y, Deleter>(mPtr, d);
        } else {
            mControlBlock = nullptr;
        }
    }

    template<typename Y> friend class shared_ptr;
    template<typename Y> friend class weak_ptr;
    
    template<typename Y, typename... Args>
    friend shared_ptr<Y> make_shared(Args&&... args) FL_NO_EXCEPT;

    template<typename Y, typename Deleter, typename... Args>
    friend shared_ptr<Y> make_shared_with_deleter(Deleter d, Args&&... args) FL_NO_EXCEPT;

    template<typename Y, typename A, typename... Args>
    friend shared_ptr<Y> allocate_shared(const A& alloc, Args&&... args) FL_NO_EXCEPT;

    template<typename Y>
    friend shared_ptr<Y> make_shared_no_tracking(Y& obj) FL_NO_EXCEPT;

    template<typename Y>
    friend shared_ptr<Y> make_shared_array(size_t n) FL_NO_EXCEPT;
};

// Factory functions

// make_shared with optimized inlined storage (single allocation)
template<typename T, typename... Args>
shared_ptr<T> make_shared(Args&&... args) FL_NO_EXCEPT {
    // Allocate the control block's raw memory explicitly and null-check it
    // BEFORE constructing. `new detail::InlinedControlBlock<T>()` cannot be
    // trusted to skip construction on allocation failure: its custom
    // operator new returns null under OOM, but the new-expression still
    // runs the constructor on that null pointer — writing through address 0
    // (EXCVADDR 0 crash) — and the old `if (control == nullptr)` guard was
    // dead code because it ran only AFTER the null construction. This was
    // the primary FastLED#3588 corruptor: under low free heap (which an
    // active WiFi SoftAP creates, but so does any memory pressure) every
    // make_shared<json_value> in the JSON-RPC build path faulted. Separate
    // allocation from construction so the null check actually protects it.
    void* mem = detail::InlinedControlBlock<T>::operator new(
        sizeof(detail::InlinedControlBlock<T>));
    if (mem == nullptr) {
        return shared_ptr<T>();  // OOM — degrade to an empty shared_ptr
    }
    auto* control = ::new (mem) detail::InlinedControlBlock<T>();  // global placement new on valid memory
    T* obj = control->get_object();
    new(obj) T(fl::forward<Args>(args)...);  // Placement new
    control->object_constructed = true;
    return shared_ptr<T>(obj, control, detail::make_shared_tag{});
}

template<typename T, typename Deleter, typename... Args>
shared_ptr<T> make_shared_with_deleter(Deleter d, Args&&... args) FL_NO_EXCEPT {
    T* obj = new T(fl::forward<Args>(args)...);
    if (obj == nullptr) {
        return shared_ptr<T>(); // OOM — see make_shared
    }
    auto* control = new detail::ControlBlock<T, Deleter>(obj, d);
    if (control == nullptr) {
        d(obj);
        return shared_ptr<T>(); // OOM — see make_shared
    }
    //new(control->get_object()) T(fl::forward<Args>(args)...);
    //control->object_constructed = true;
    return shared_ptr<T>(obj, control, detail::make_shared_tag{});
}

// NEW: Creates a shared_ptr that does not modify the reference count
// The shared_ptr and any copies will not affect object lifetime.
// No control block is allocated - this is a zero-overhead wrapper around a raw pointer.
template<typename T>
shared_ptr<T> make_shared_no_tracking(T& obj) FL_NO_EXCEPT {
    return shared_ptr<T>(&obj, nullptr, detail::no_tracking_tag{});
}

// make_shared_array for array allocations
template<typename T>
shared_ptr<T> make_shared_array(size_t n) FL_NO_EXCEPT {
    T* arr = new T[n]();  // Zero-initialize the array
    if (arr == nullptr) {
        return shared_ptr<T>(); // OOM — see make_shared
    }
    auto* control = new detail::ControlBlock<T, detail::array_delete<T>>(arr, detail::array_delete<T>{});
    if (control == nullptr) {
        delete[] arr;
        return shared_ptr<T>(); // OOM — see make_shared
    }
    return shared_ptr<T>(arr, control, detail::make_shared_tag{});
}

// allocate_shared (simplified version without full allocator support for now)
template<typename T, typename A, typename... Args>
shared_ptr<T> allocate_shared(const A& /* alloc */, Args&&... args) FL_NO_EXCEPT {
    // For now, just delegate to make_shared
    // Full allocator support would require more complex control block management
    return make_shared<T>(fl::forward<Args>(args)...);
}

// Non-member comparison operators
template<typename T, typename Y>
bool operator==(const shared_ptr<T>& lhs, const shared_ptr<Y>& rhs) FL_NO_EXCEPT {
    return lhs.get() == rhs.get();
}

template<typename T, typename Y>
bool operator!=(const shared_ptr<T>& lhs, const shared_ptr<Y>& rhs) FL_NO_EXCEPT {
    return lhs.get() != rhs.get();
}

template<typename T, typename Y>
bool operator<(const shared_ptr<T>& lhs, const shared_ptr<Y>& rhs) FL_NO_EXCEPT {
    return lhs.get() < rhs.get();
}

template<typename T, typename Y>
bool operator<=(const shared_ptr<T>& lhs, const shared_ptr<Y>& rhs) FL_NO_EXCEPT {
    return lhs.get() <= rhs.get();
}

template<typename T, typename Y>
bool operator>(const shared_ptr<T>& lhs, const shared_ptr<Y>& rhs) FL_NO_EXCEPT {
    return lhs.get() > rhs.get();
}

template<typename T, typename Y>
bool operator>=(const shared_ptr<T>& lhs, const shared_ptr<Y>& rhs) FL_NO_EXCEPT {
    return lhs.get() >= rhs.get();
}

template<typename T>
bool operator==(const shared_ptr<T>& lhs, fl::nullptr_t) FL_NO_EXCEPT {
    return lhs.get() == nullptr;
}

template<typename T>
bool operator==(fl::nullptr_t, const shared_ptr<T>& rhs) FL_NO_EXCEPT {
    return nullptr == rhs.get();
}

template<typename T>
bool operator!=(const shared_ptr<T>& lhs, fl::nullptr_t) FL_NO_EXCEPT {
    return lhs.get() != nullptr;
}

template<typename T>
bool operator!=(fl::nullptr_t, const shared_ptr<T>& rhs) FL_NO_EXCEPT {
    return nullptr != rhs.get();
}

// Utility functions
template<typename T>
void swap(shared_ptr<T>& lhs, shared_ptr<T>& rhs) FL_NO_EXCEPT {
    lhs.swap(rhs);
}

// Casts - using aliasing constructor (matches std::shared_ptr behavior)
template<typename T, typename Y>
shared_ptr<T> static_pointer_cast(const shared_ptr<Y>& other) FL_NO_EXCEPT {
    return shared_ptr<T>(other, static_cast<T*>(other.get()));
}

template<typename T, typename Y>
shared_ptr<T> const_pointer_cast(const shared_ptr<Y>& other) FL_NO_EXCEPT {
    return shared_ptr<T>(other, const_cast<T*>(other.get()));
}

template<typename T, typename Y>
shared_ptr<T> reinterpret_pointer_cast(const shared_ptr<Y>& other) FL_NO_EXCEPT {
    return shared_ptr<T>(other, fl::bit_cast<T*>(other.get()));
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
