
#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

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
    // Use this to allocate large blocks of memory for T.
    // This is useful for large arrays or objects that need to be allocated
    // in a single block.

    T* allocate(size_t n ) {
        if (n == 0) {
            return nullptr; // Handle zero allocation
        }
        void *ptr = malloc(sizeof(T) * n);
        if (ptr == nullptr) {
            return nullptr; // Handle allocation failure
        }
        memset(ptr, 0, sizeof(T) * n); // Zero-initialize the memory
        return ptr;
    }

    void deallocate( T* p, size_t n ) {
        if (p == nullptr) {
            return; // Handle null pointer
        }
        free(p); // Free the allocated memory
    }
};


// Macro to specialize the Allocator for a specific type to use PSRam
// Usage: FL_USE_PSRAM_ALLOCATOR(MyClass)
#define FL_USE_PSRAM_ALLOCATOR(TYPE) \
template <> \
class allocator<TYPE> { \
  public: \
    TYPE *allocate(size_t n) { \
        return reinterpret_cast<TYPE *>(PSRamAllocate(sizeof(TYPE) * n, true)); \
    } \
    void deallocate(TYPE *p) { \
        if (p == nullptr) { \
            return; \
        } \
        PSRamDeallocate(p); \
    } \
};

} // namespace fl
