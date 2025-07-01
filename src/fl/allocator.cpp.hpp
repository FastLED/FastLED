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
void *DefaultAlloc(fl::sz size) {
    void *out = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (out == nullptr) {
        // Fallback to default allocator.
        out = heap_caps_malloc(size, MALLOC_CAP_DEFAULT);
    }
    return out;
}
void DefaultFree(void *ptr) { heap_caps_free(ptr); }
#else
void *DefaultAlloc(fl::sz size) { return malloc(size); }
void DefaultFree(void *ptr) { free(ptr); }
#endif

void *(*Alloc)(fl::sz) = DefaultAlloc;
void (*Dealloc)(void *) = DefaultFree;
} // namespace

void SetPSRamAllocator(void *(*alloc)(fl::sz), void (*free)(void *)) {
    Alloc = alloc;
    Dealloc = free;
}

void *PSRamAllocate(fl::sz size, bool zero) {
    void *ptr = Alloc(size);
    if (ptr && zero) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void PSRamDeallocate(void *ptr) {
    Dealloc(ptr);
}

void Malloc(fl::sz size) { Alloc(size); }

void Free(void *ptr) { Dealloc(ptr); }

} // namespace fl
