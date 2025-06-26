
#include <stdlib.h>

#include "fl/allocator.h"
#include "fl/namespace.h"

#ifdef ESP32
#include "esp_heap_caps.h"
#include "esp_system.h"
#endif

namespace fl {

namespace {

#ifdef ESP32
// On esp32, attempt to always allocate in psram first.
void *DefaultAlloc(size_t size) {
    void *out = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (out == nullptr) {
        // Fallback to default allocator.
        out = heap_caps_malloc(size, MALLOC_CAP_DEFAULT);
    }
    return out;
}
void DefaultFree(void *ptr) { heap_caps_free(ptr); }
#else
void *DefaultAlloc(size_t size) { return malloc(size); }
void DefaultFree(void *ptr) { free(ptr); }
#endif

void *(*Alloc)(size_t) = DefaultAlloc;
void (*Dealloc)(void *) = DefaultFree;
} // namespace

void SetPSRamAllocator(void *(*alloc)(size_t), void (*free)(void *)) {
    Alloc = alloc;
    Dealloc = free;
}

void *PSRamAllocate(size_t size, bool zero) {
    void *ptr = Alloc(size);
    if (zero) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void PSRamDeallocate(void *ptr) { Dealloc(ptr); }

void Malloc(size_t size) {
    void *ptr = Alloc(size);
    if (ptr == nullptr) {
        // Handle allocation failure, e.g., log an error or throw an exception.
        // For now, we just return without doing anything.
        return;
    }
    memset(ptr, 0, size); // Zero-initialize the memory

}
void Free(void *ptr) {
    if (ptr == nullptr) {
        return; // Handle null pointer
    }
    Dealloc(ptr); // Free the allocated memory
}

} // namespace fl
