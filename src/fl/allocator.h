
#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

namespace fl {

void SetPSRamAllocator(void *(*alloc)(size_t), void (*free)(void *));
void *PSRamAllocate(size_t size, bool zero = true);
void PSRamDeallocate(void *ptr);

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

// Define your own allocator in the code and the the fl::vector class will
// use it.
template <typename T> class Allocator {
  public:
    // Use this to allocate large blocks of memory for T.
    // This is useful for large arrays or objects that need to be allocated
    // in a single block.

    static T *Alloc(size_t n) {
        size_t size = sizeof(T) * n;
        void * ptr = malloc(size);
        if (ptr == nullptr) {
            return nullptr; // Allocation failed
        }
        memset(ptr, 0, sizeof(T) * n); // Initialize to zero
        return reinterpret_cast<T *>(ptr);
    }

    // Use this to free the allocated memory.
    static void Free(T *p) {
        if (p == nullptr) {
            return;
        }
        free(p); // Free the memory
    }
};


// Macro to specialize the Allocator for a specific type to use PSRam
// Usage: FL_USE_PSRAM_ALLOCATOR(MyClass)
#define FL_USE_PSRAM_ALLOCATOR(TYPE) \
template <> \
class Allocator<TYPE> { \
  public: \
    static TYPE *Alloc(size_t n) { \
        return reinterpret_cast<TYPE *>(PSRamAllocate(sizeof(TYPE) * n, true)); \
    } \
    static void Free(TYPE *p) { \
        if (p == nullptr) { \
            return; \
        } \
        PSRamDeallocate(p); \
    } \
};

} // namespace fl
