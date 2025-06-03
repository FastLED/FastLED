
#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "fl/inplacenew.h"
#include "fl/type_traits.h"
#include "fl/unused.h"

namespace fl {

void SetPSRamAllocator(void *(*alloc)(size_t), void (*free)(void *));
void *PSRamAllocate(size_t size, bool zero = true);
void PSRamDeallocate(void *ptr);
void Malloc(size_t size);
void Free(void *ptr);

template <typename T> class PSRamAllocator {
  public:
    static T *Alloc(size_t n) {
        void *ptr = PSRamAllocate(sizeof(T) * n, true);
        return reinterpret_cast<T *>(ptr);
    }

    static void Free(T *p) {
        if (p == nullptr) {
            return;
        }
        PSRamDeallocate(p);
    }
};

// std compatible allocator.
template <typename T> class allocator {
  public:
    // Type definitions required by STL
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    
    // Rebind allocator to type U
    template <typename U>
    struct rebind {
        using other = allocator<U>;
    };
    
    // Default constructor
    allocator() noexcept {}
    
    // Copy constructor
    template <typename U>
    allocator(const allocator<U>&) noexcept {}
    
    // Destructor
    ~allocator() noexcept {}

    // Use this to allocate large blocks of memory for T.
    // This is useful for large arrays or objects that need to be allocated
    // in a single block.
    T* allocate(size_t n) {
        if (n == 0) {
            return nullptr; // Handle zero allocation
        }
        void *ptr = malloc(sizeof(T) * n);
        if (ptr == nullptr) {
            return nullptr; // Handle allocation failure
        }
        memset(ptr, 0, sizeof(T) * n); // Zero-initialize the memory
        return static_cast<T*>(ptr);
    }

    void deallocate(T* p, size_t n) {
        FASTLED_UNUSED(n);
        if (p == nullptr) {
            return; // Handle null pointer
        }
        free(p); // Free the allocated memory
    }
    
    // Construct an object at the specified address
    template <typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        new(static_cast<void*>(p)) U(fl::forward<Args>(args)...);
    }
    
    // Destroy an object at the specified address
    template <typename U>
    void destroy(U* p) {
        p->~U();
    }
};

template <typename T> class allocator_psram {
    public:
        // Type definitions required by STL
        using value_type = T;
        using pointer = T*;
        using const_pointer = const T*;
        using reference = T&;
        using const_reference = const T&;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
    
        // Rebind allocator to type U
        template <typename U>
        struct rebind {
            using other = allocator_psram<U>;
        };
    
        // Default constructor
        allocator_psram() noexcept {}
    
        // Copy constructor
        template <typename U>
        allocator_psram(const allocator_psram<U>&) noexcept {}
    
        // Destructor
        ~allocator_psram() noexcept {}
    
        // Allocate memory for n objects of type T
        T* allocate(size_t n) {
            return PSRamAllocator<T>::Alloc(n);
        }
    
        // Deallocate memory for n objects of type T
        void deallocate(T* p, size_t n) {
            PSRamAllocator<T>::Free(p);
            FASTLED_UNUSED(n);
        }
    
        // Construct an object at the specified address
        template <typename U, typename... Args>
        void construct(U* p, Args&&... args) {
            new(static_cast<void*>(p)) U(fl::forward<Args>(args)...);
        }
    
        // Destroy an object at the specified address
        template <typename U>
        void destroy(U* p) {
            p->~U();
        }
};


// Macro to specialize the Allocator for a specific type to use PSRam
// Usage: FL_USE_PSRAM_ALLOCATOR(MyClass)
#define FL_USE_PSRAM_ALLOCATOR(TYPE) \
template <> \
class allocator<TYPE> { \
  public: \
    using value_type = TYPE; \
    using pointer = TYPE*; \
    using const_pointer = const TYPE*; \
    using reference = TYPE&; \
    using const_reference = const TYPE&; \
    using size_type = size_t; \
    using difference_type = ptrdiff_t; \
    \
    template <typename U> \
    struct rebind { \
        using other = allocator<U>; \
    }; \
    \
    allocator() noexcept {} \
    \
    template <typename U> \
    allocator(const allocator<U>&) noexcept {} \
    \
    ~allocator() noexcept {} \
    \
    TYPE *allocate(size_t n) { \
        return reinterpret_cast<TYPE *>(PSRamAllocate(sizeof(TYPE) * n, true)); \
    } \
    void deallocate(TYPE *p, size_t n) { \
        if (p == nullptr) { \
            return; \
        } \
        PSRamDeallocate(p); \
    } \
    \
    template <typename U, typename... Args> \
    void construct(U* p, Args&&... args) { \
        new(static_cast<void*>(p)) U(fl::forward<Args>(args)...); \
    } \
    \
    template <typename U> \
    void destroy(U* p) { \
        p->~U(); \
    } \
};

} // namespace fl
