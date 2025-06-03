
#pragma once

#include <stddef.h>
#include <string.h>

namespace fl {

void SetLargeBlockAllocator(void *(*alloc)(size_t), void (*free)(void *));
void *LargeBlockAllocate(size_t size, bool zero = true);
void LargeBlockDeallocate(void *ptr);

template <typename T> class LargeBlockAllocator {
  public:
    static T *Alloc(size_t n) {
        void *ptr = LargeBlockAllocate(sizeof(T) * n, true);
        return reinterpret_cast<T *>(ptr);
    }

    static void Free(T *p) {
        if (p == nullptr) {
            return;
        }
        LargeBlockDeallocate(p);
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

} // namespace fl
