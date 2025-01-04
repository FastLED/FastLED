
#pragma once

#include <stddef.h>
#include <string.h>
#include <stdlib.h>



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
    struct Deallocator{
        void operator()(T* p) {
            LargeBlockAllocator<T>::Free(p);
        }
    };
};

template<typename T>
class Allocator {
public:
    static T* Alloc(size_t n) {
        void* ptr = malloc(sizeof(T) * n);
        return reinterpret_cast<T*>(ptr);
    }

    static void Free(T* p) {
        if (p == nullptr) {
            return;
        }
        free(p);
    }

    struct Deallocator{
        void operator()(T* p) {
            Allocator<T>::Free(p);
        }
    };
};

} // namespace fl
