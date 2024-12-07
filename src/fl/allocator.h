
#pragma once

#include <stddef.h>
#include <string.h>



namespace fl {

void SetLargeBlockAllocator(void *(*alloc)(size_t), void (*free)(void *));
void* LargeBlockAllocate(size_t size, bool zero = true);
void LargeBlockDeallocate(void* ptr);

template<typename T>
class LargeBlockAllocator {
public:
    static T* Alloc(size_t n) {
        void* ptr = LargeBlockAllocate(sizeof(T) * n, true);
        return reinterpret_cast<T*>(ptr);
    }

    static void Free(T* p) {
        if (p == nullptr) {
            return;
        }
        LargeBlockDeallocate(p);
    }
};

} // namespace fl
