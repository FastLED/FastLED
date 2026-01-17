#include "fl/stl/allocator.h"
#include "fl/int.h"
#include "fl/thread_local.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/cstdlib.h"
#include "fl/str.h"
#include "fl/stl/cstring.h"

#ifdef ESP32
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "platforms/esp/esp_version.h"
#endif

namespace fl {

// Explicit instantiations for commonly used allocator types
// This ensures the constexpr static members are defined for these types
template struct allocator_traits<allocator<int>>;
template struct allocator_traits<allocator_realloc<int>>;
template struct allocator_traits<allocator_psram<int>>;
template struct allocator_traits<allocator_slab<int>>;

// Define constexpr static members for allocator_traits (C++11 requires this for ODR-used variables)
template <typename Allocator>
constexpr bool allocator_traits<Allocator>::has_reallocate_v;

template <typename Allocator>
constexpr bool allocator_traits<Allocator>::has_allocate_at_least_v;

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
        fl::memset(ptr, 0, size);
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

#ifdef ESP32
// ESP32-specific memory allocation for RMT buffer pooling
// These functions provide direct access to specific memory regions for performance

void* InternalAlloc(fl::size size) {
    // MALLOC_CAP_INTERNAL: Internal DRAM (fast, not in PSRAM)
    // MALLOC_CAP_8BIT: Ensure byte-accessible memory
    void* ptr = heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (ptr) {
        fl::memset(ptr, 0, size);  // Zero-initialize
    }
    return ptr;
}

void* InternalRealloc(void* ptr, fl::size size) {
    // Realloc in internal DRAM - may relocate or expand in-place
    void* new_ptr = heap_caps_realloc(ptr, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    // Zero-initialize any newly allocated bytes (if expanded)
    // Note: heap_caps_realloc preserves existing data, we only zero new space
    // This is best-effort - we don't track old size, so we can't selectively zero
    // For safety, caller should manage initialization if needed

    return new_ptr;
}

void InternalFree(void* ptr) {
    heap_caps_free(ptr);
}

void* DMAAlloc(fl::size size) {
    // MALLOC_CAP_DMA: DMA-capable memory (limited on ESP32)
    // MALLOC_CAP_INTERNAL: Must be in internal SRAM (not PSRAM) for most DMA
    //
    // CRITICAL: Use 64-byte aligned allocation for cache coherency
    // ESP32-S3/C3/C6/H2 have data cache with 64-byte cache lines.
    // Without alignment, esp_cache_msync() may not flush/invalidate correctly,
    // causing DMA to read stale cached data.
    //
    // Round size up to 64-byte multiple for proper cache line alignment
    fl::size aligned_size = ((size + 63) / 64) * 64;

#if ESP_IDF_VERSION_4_OR_HIGHER
    // heap_caps_aligned_alloc is available in ESP-IDF v4.0+
    void* ptr = heap_caps_aligned_alloc(64, aligned_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
#else
    // ESP-IDF v3.x fallback: use regular allocation (no alignment guarantee)
    void* ptr = heap_caps_malloc(aligned_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
#endif
    if (ptr) {
        fl::memset(ptr, 0, aligned_size);  // Zero-initialize full aligned buffer
    }
    return ptr;
}

void DMAFree(void* ptr) {
    heap_caps_free(ptr);
}
#endif

} // namespace fl
