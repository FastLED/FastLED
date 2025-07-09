#include <stdlib.h>

#include "fl/allocator.h"
#include "fl/namespace.h"
#include "fl/int.h"
#include "fl/thread_local.h"

#ifdef ESP32
#include "esp_heap_caps.h"
#include "esp_system.h"
#endif

namespace fl {

namespace {

#ifdef ESP32
// On esp32, attempt to always allocate in psram first.
void *DefaultAlloc(fl::size size) {
    void *out = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (out == nullptr) {
        // Fallback to default allocator.
        out = heap_caps_malloc(size, MALLOC_CAP_DEFAULT);
    }
    return out;
}
void DefaultFree(void *ptr) { heap_caps_free(ptr); }
#else
void *DefaultAlloc(fl::size size) { return malloc(size); }
void DefaultFree(void *ptr) { free(ptr); }
#endif

void *(*Alloc)(fl::size) = DefaultAlloc;
void (*Dealloc)(void *) = DefaultFree;

#if defined(FASTLED_TESTING)
// Test hook interface pointer
MallocFreeHook* gMallocFreeHook = nullptr;

int& tls_reintrancy_count() {
    static fl::ThreadLocal<int> enabled;
    return enabled.access();
}

struct MemoryGuard {
    int& reintrancy_count;
    MemoryGuard(): reintrancy_count(tls_reintrancy_count()) {
        reintrancy_count++;
    }
    ~MemoryGuard() {
        reintrancy_count--;
    }
    bool enabled() const {
        return reintrancy_count <= 1;
    }
};

#endif

} // namespace

#if defined(FASTLED_TESTING)
void SetMallocFreeHook(MallocFreeHook* hook) {
    gMallocFreeHook = hook;
}

void ClearMallocFreeHook() {
    gMallocFreeHook = nullptr;
}
#endif

void SetPSRamAllocator(void *(*alloc)(fl::size), void (*free)(void *)) {
    Alloc = alloc;
    Dealloc = free;
}

void *PSRamAllocate(fl::size size, bool zero) {

    void *ptr = Alloc(size);
    if (ptr && zero) {
        memset(ptr, 0, size);
    }
    
#if defined(FASTLED_TESTING)
    if (gMallocFreeHook && ptr) {
        MemoryGuard allows_hook;
        if (allows_hook.enabled()) {
            gMallocFreeHook->onMalloc(ptr, size);
        }
    }
#endif
    
    return ptr;
}

void PSRamDeallocate(void *ptr) {
#if defined(FASTLED_TESTING)
    if (gMallocFreeHook && ptr) {
        // gMallocFreeHook->onFree(ptr);
        MemoryGuard allows_hook;
        if (allows_hook.enabled()) {
            gMallocFreeHook->onFree(ptr);
        }
    }
#endif
    
    Dealloc(ptr);
}

void* Malloc(fl::size size) { 
    void* ptr = Alloc(size); 
    
#if defined(FASTLED_TESTING)
    if (gMallocFreeHook && ptr) {
        MemoryGuard allows_hook;    
        if (allows_hook.enabled()) {
            gMallocFreeHook->onMalloc(ptr, size);
        }
    }
#endif
    
    return ptr;
}

void Free(void *ptr) { 
#if defined(FASTLED_TESTING)
    if (gMallocFreeHook && ptr) {
        MemoryGuard allows_hook;
        if (allows_hook.enabled()) {
            gMallocFreeHook->onFree(ptr);
        }
    }
#endif
    
    Dealloc(ptr); 
}

} // namespace fl
