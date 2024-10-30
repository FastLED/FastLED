
#include <stdlib.h>

#include "allocator.h"
#include "namespace.h"


#ifdef ESP32
#include "esp_heap_caps.h"
#include "esp_system.h"
#endif

FASTLED_NAMESPACE_BEGIN

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
void (*Free)(void *) = DefaultFree;
} // namespace

void SetLargeBlockAllocator(void *(*alloc)(size_t), void (*free)(void *)) {
    Alloc = alloc;
    Free = free;
}

void* LargeBlockAllocate(size_t size, bool zero) {
    void* ptr = Alloc(size);
    if (zero) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void LargeBlockDeallocate(void* ptr) {
    Free(ptr);
}

FASTLED_NAMESPACE_END
