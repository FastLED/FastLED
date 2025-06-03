
#pragma once

#include <stddef.h>
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
        return new T[n]();
    }

    // Use this to free the allocated memory.
    static void Free(T *p) {
        if (p == nullptr) {
            return;
        }
        delete[] p;
    }
};

// #define FL_USE_PSRAM_ALLOCATOR(TYPE)

} // namespace fl
