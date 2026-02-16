// ok standalone
// Memory profiling test for JSON parsers
// Compares parse() (ArduinoJson) vs parse2() (custom parser)
// Uses global allocation overrides to track ALL memory usage

#include "fl/file_system.h"  // for FileHandle, FileSystem, make_sdcard_filesystem
#include "fl/int.h"          // for size, u32, u8
#include "fl/json.h"         // for Json
#include "fl/stl/atomic.h"   // for atomic
#include "fl/stl/cstdint.h"  // for size_t
#include "fl/stl/map.h"      // for FixedMap
#include "fl/stl/stdio.h"    // for printf
#include "fl/stl/string.h"   // for string
#include "fl/string_view.h"  // for string_view
#include "stdio.h"           // for printf // ok include
#include "stdlib.h"          // for abort, calloc, free, malloc, realloc
#include "string.h"          // for strlen // ok include

// Platform-specific includes (avoid windows.h to prevent macro conflicts)
#ifdef _WIN32
// Forward declarations to avoid windows.h
extern "C" {
    __declspec(dllimport) void* __stdcall GetModuleHandleA(const char*);
    __declspec(dllimport) void* __stdcall GetProcAddress(void*, const char*);
}
using HMODULE = void*;
#else
#include <dlfcn.h>
#endif

using namespace fl;

// ============================================================================
// GLOBAL ALLOCATION TRACKING
// ============================================================================

namespace {

// Allocation tracking state
struct AllocationStats {
    fl::atomic<size_t> current_bytes{0};
    fl::atomic<size_t> peak_bytes{0};
    fl::atomic<size_t> total_allocated{0};
    fl::atomic<int> alloc_count{0};
    fl::atomic<int> free_count{0};

    // Active allocations: pointer -> size
    // Using FixedMap with large capacity for stress testing
    fl::FixedMap<const void*, u32, 64000> allocations;

    void reset() {
        current_bytes.store(0);
        peak_bytes.store(0);
        total_allocated.store(0);
        alloc_count.store(0);
        free_count.store(0);
        allocations.clear();
    }

    void on_malloc(void* ptr, size_t size) {
        if (!ptr) return;

        // Use fetch_add instead of += for fl::atomic
        current_bytes.fetch_add(size);
        total_allocated.fetch_add(size);
        alloc_count.fetch_add(1);

        // Update peak
        size_t curr = current_bytes.load();
        size_t peak = peak_bytes.load();
        while (curr > peak && !peak_bytes.compare_exchange_weak(peak, curr)) {
            peak = peak_bytes.load();
        }

        // Track allocation
        allocations[ptr] = static_cast<u32>(size);
    }

    void on_free(void* ptr) {
        if (!ptr) return;

        auto it = allocations.find(ptr);
        if (it != allocations.end()) {
            size_t size = it->second;
            current_bytes.fetch_sub(size);
            free_count.fetch_add(1);
            // FixedMap doesn't have erase(), so we'll just mark it as removed by setting size to 0
            // This is acceptable for profiling as we track current_bytes separately
            allocations[ptr] = 0;
        }
    }

    size_t count_active_allocations() const {
        size_t count = 0;
        for (auto it = allocations.begin(); it != allocations.end(); ++it) {
            if (it->second != 0) {  // Non-zero means active allocation
                count++;
            }
        }
        return count;
    }

    void print_stats(const char* label) {
        printf("\n=== %s ===\n", label);
        printf("  Peak memory:      %zu bytes\n", peak_bytes.load());
        printf("  Current memory:   %zu bytes\n", current_bytes.load());
        printf("  Total allocated:  %zu bytes\n", total_allocated.load());
        printf("  Allocations:      %d\n", alloc_count.load());
        printf("  Frees:            %d\n", free_count.load());
        printf("  Active allocs:    %zu\n", count_active_allocations());
        printf("  Leaked bytes:     %zu\n", current_bytes.load());
    }
};

AllocationStats g_stats;
bool g_tracking_enabled = false;

// Get real allocator functions (platform-specific)
#ifdef _WIN32
using MallocFunc = void* (*)(size_t);
using CallocFunc = void* (*)(size_t, size_t);
using ReallocFunc = void* (*)(void*, size_t);
using FreeFunc = void (*)(void*);

MallocFunc real_malloc = nullptr;
CallocFunc real_calloc = nullptr;
ReallocFunc real_realloc = nullptr;
FreeFunc real_free = nullptr;

void init_real_allocators() {
    if (real_malloc) return;

    HMODULE ucrt = GetModuleHandleA("ucrtbase.dll");
    if (!ucrt) ucrt = GetModuleHandleA("msvcrt.dll");

    if (ucrt) {
        real_malloc = (MallocFunc)GetProcAddress(ucrt, "malloc");
        real_calloc = (CallocFunc)GetProcAddress(ucrt, "calloc");
        real_realloc = (ReallocFunc)GetProcAddress(ucrt, "realloc");
        real_free = (FreeFunc)GetProcAddress(ucrt, "free");
    }

    // If we can't find the real allocator, abort (can't fallback to malloc in override)
    if (!real_malloc) {
        printf("FATAL: Cannot find real malloc in ucrtbase.dll or msvcrt.dll\n");
        abort();
    }
}
#else
void* real_malloc(size_t size) {
    static auto fn = (void* (*)(size_t))dlsym(RTLD_NEXT, "malloc");
    return fn(size);
}

void* real_calloc(size_t n, size_t size) {
    static auto fn = (void* (*)(size_t, size_t))dlsym(RTLD_NEXT, "calloc");
    return fn(n, size);
}

void* real_realloc(void* ptr, size_t size) {
    static auto fn = (void* (*)(void*, size_t))dlsym(RTLD_NEXT, "realloc");
    return fn(ptr, size);
}

void real_free(void* ptr) {
    static auto fn = (void (*)(void*))dlsym(RTLD_NEXT, "free");
    fn(ptr);
}
#endif

} // anonymous namespace

// ============================================================================
// GLOBAL ALLOCATION OVERRIDES
// ============================================================================

extern "C" {

void* malloc(size_t size) {
#ifdef _WIN32
    init_real_allocators();
#endif
    void* ptr = real_malloc(size);
    if (g_tracking_enabled) {
        g_stats.on_malloc(ptr, size);
    }
    return ptr;
}

void* calloc(size_t n, size_t size) {
#ifdef _WIN32
    init_real_allocators();
#endif
    void* ptr = real_calloc(n, size);
    if (g_tracking_enabled) {
        g_stats.on_malloc(ptr, n * size);
    }
    return ptr;
}

void* realloc(void* ptr, size_t size) {
#ifdef _WIN32
    init_real_allocators();
#endif
    if (g_tracking_enabled && ptr) {
        g_stats.on_free(ptr);
    }
    void* new_ptr = real_realloc(ptr, size);
    if (g_tracking_enabled && new_ptr) {
        g_stats.on_malloc(new_ptr, size);
    }
    return new_ptr;
}

void free(void* ptr) {
#ifdef _WIN32
    init_real_allocators();
#endif
    if (g_tracking_enabled) {
        g_stats.on_free(ptr);
    }
    real_free(ptr);
}

} // extern "C"

// Operator new/delete overrides
void* operator new(size_t size) {
    void* ptr = ::malloc(size);  // Explicitly call global malloc (our override)
    if (!ptr) {
        printf("FATAL: operator new failed to allocate %zu bytes\n", size);
        abort();
    }
    return ptr;
}

void* operator new[](size_t size) {
    void* ptr = ::malloc(size);  // Explicitly call global malloc (our override)
    if (!ptr) {
        printf("FATAL: operator new[] failed to allocate %zu bytes\n", size);
        abort();
    }
    return ptr;
}

void operator delete(void* ptr) noexcept {
    ::free(ptr);  // Explicitly call global free (our override)
}

void operator delete[](void* ptr) noexcept {
    ::free(ptr);  // Explicitly call global free (our override)
}

void operator delete(void* ptr, size_t) noexcept {
    ::free(ptr);  // Explicitly call global free (our override)
}

void operator delete[](void* ptr, size_t) noexcept {
    ::free(ptr);  // Explicitly call global free (our override)
}

// ============================================================================
// STRESS TEST JSON DATA
// ============================================================================

// Synthetic JSON representing FastLED ScreenMap with multiple strips
// Size: ~10KB, deeply nested, mixed arrays/objects
constexpr const char* STRESS_TEST_JSON = R"({
  "version": "1.0",
  "fps": 60,
  "brightness": 0.85,
  "strips": [
    {
      "id": "strip_0",
      "type": "WS2812B",
      "length": 100,
      "x": [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99],
      "y": [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
      "diameter": 0.5,
      "color_order": "RGB"
    },
    {
      "id": "strip_1",
      "type": "APA102",
      "length": 100,
      "x": [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99],
      "y": [1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1],
      "diameter": 0.3,
      "color_order": "BGR"
    },
    {
      "id": "strip_2",
      "type": "WS2812B",
      "length": 100,
      "x": [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99],
      "y": [2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2],
      "diameter": 0.5,
      "color_order": "RGB"
    }
  ],
  "effects": [
    {"name": "rainbow", "speed": 1.5, "brightness": 0.9},
    {"name": "twinkle", "speed": 2.0, "brightness": 0.7},
    {"name": "fade", "speed": 0.5, "brightness": 1.0}
  ],
  "metadata": {
    "created": "2024-01-15",
    "author": "FastLED",
    "description": "Memory profiling stress test JSON"
  }
})";

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Helper to run memory profiling on a JSON string
void profile_json_memory(const char* test_name, const fl::string& json_data) {
    printf("\n\n");
    printf("================================================================================\n");
    printf("%s\n", test_name);
    printf("================================================================================\n");
    printf("JSON size: %zu bytes (%.2f KB)\n", json_data.size(), json_data.size() / 1024.0);
    printf("\n");

#ifdef _WIN32
    init_real_allocators();
#endif

    // Baseline: Measure overhead of tracking itself
    g_tracking_enabled = false;
    {
        Json warmup = Json::parse(json_data);
        (void)warmup;
    }

    // Test 1: ArduinoJson parse()
    g_stats.reset();
    g_tracking_enabled = true;

    size_t parse1_peak = 0;
    size_t parse1_allocs = 0;
    {
        Json result1 = Json::parse(json_data);
        if (result1.is_null()) {
            printf("❌ ERROR: ArduinoJson parse() failed\n");
            g_tracking_enabled = false;
            return;
        }

        parse1_peak = g_stats.peak_bytes.load();
        parse1_allocs = g_stats.alloc_count.load();
    }

    g_tracking_enabled = false;
    g_stats.print_stats("ArduinoJson parse()");

    // Test 2: Custom parse2()
    g_stats.reset();
    g_tracking_enabled = true;

    size_t parse2_peak = 0;
    size_t parse2_allocs = 0;
    {
        Json result2 = Json::parse2(json_data);
        if (result2.is_null()) {
            printf("❌ ERROR: Custom parse2() failed\n");
            g_tracking_enabled = false;
            return;
        }

        parse2_peak = g_stats.peak_bytes.load();
        parse2_allocs = g_stats.alloc_count.load();
    }

    g_tracking_enabled = false;
    g_stats.print_stats("Custom parse2()");

    // Comparison
    printf("\n");
    printf("================================================================================\n");
    printf("MEMORY COMPARISON\n");
    printf("================================================================================\n");

    double memory_ratio = (double)parse2_peak / (double)parse1_peak;
    double alloc_ratio = (double)parse2_allocs / (double)parse1_allocs;

    printf("Peak memory:   parse2() = %.1f%% of parse()  (%zu vs %zu bytes)\n",
           memory_ratio * 100.0, parse2_peak, parse1_peak);
    printf("Allocations:   parse2() = %.1f%% of parse()  (%zu vs %zu allocs)\n",
           alloc_ratio * 100.0, parse2_allocs, parse1_allocs);

    if (parse2_peak < parse1_peak) {
        size_t saved = parse1_peak - parse2_peak;
        printf("✓ Memory saved: %zu bytes (%.1f%% reduction)\n",
               saved, (1.0 - memory_ratio) * 100.0);
    } else {
        size_t extra = parse2_peak - parse1_peak;
        printf("✗ Extra memory: %zu bytes (%.1f%% increase)\n",
               extra, (memory_ratio - 1.0) * 100.0);
    }

    if (parse2_allocs < parse1_allocs) {
        int saved = parse1_allocs - parse2_allocs;
        printf("✓ Allocations saved: %d (%.1f%% reduction)\n",
               saved, (1.0 - alloc_ratio) * 100.0);
    } else {
        int extra = parse2_allocs - parse1_allocs;
        printf("✗ Extra allocations: %d (%.1f%% increase)\n",
               extra, (alloc_ratio - 1.0) * 100.0);
    }

    printf("================================================================================\n");
    printf("\n");
}

// ============================================================================
// MEMORY PROFILING FUNCTIONS
// ============================================================================

int test_phase1_validation() {
    printf("\n\n");
    printf("================================================================================\n");
    printf("JSON PHASE 1 VALIDATION TEST - ZERO HEAP ALLOCATIONS\n");
    printf("================================================================================\n");

#ifdef _WIN32
    init_real_allocators();
#endif

    // Test Phase 1 validation with complex JSON
    const char* test_json = STRESS_TEST_JSON;
    printf("JSON size: %zu bytes\n", ::strlen(test_json));
    printf("Testing Phase 1 validation (tokenization + validation only)...\n\n");

    g_stats.reset();
    g_tracking_enabled = true;

    // Phase 1 validation only - this MUST allocate ZERO heap memory
    // Use zero-copy string_view to avoid fl::string allocation
    bool valid = Json::parse2_validate_only(fl::string_view(test_json, ::strlen(test_json)));

    g_tracking_enabled = false;

    printf("Validation result: %s\n", valid ? "VALID" : "INVALID");

    // Check results
    size_t phase1_allocs = g_stats.alloc_count.load();
    size_t phase1_bytes = g_stats.total_allocated.load();

    printf("Phase 1 Validation Results:\n");
    printf("  Allocations: %zu\n", phase1_allocs);
    printf("  Total bytes: %zu\n", phase1_bytes);
    printf("\n");

    // CRITICAL: Phase 1 must allocate ZERO heap memory
    if (phase1_allocs == 0 && phase1_bytes == 0) {
        printf("✓✓✓ PASS: Phase 1 validation allocates ZERO heap memory\n");
    } else {
        printf("✗✗✗ FAIL: Phase 1 validation allocated memory!\n");
        printf("    Expected: 0 allocations, 0 bytes\n");
        printf("    Actual:   %zu allocations, %zu bytes\n", phase1_allocs, phase1_bytes);
        printf("================================================================================\n\n");
        return 1;  // Failure
    }
    printf("================================================================================\n\n");
    return 0;  // Success
}

int test_small_json_profiling() {
    profile_json_memory("SMALL JSON MEMORY PROFILE (10KB Synthetic)", STRESS_TEST_JSON);
    return 0;  // Success
}

int test_large_json_profiling() {
    printf("\n\n");
    printf("================================================================================\n");
    printf("LOADING LARGE JSON FILE\n");
    printf("================================================================================\n");

    // Initialize FileSystem for testing
    FileSystem fs;
    FsImplPtr fsImpl = make_sdcard_filesystem(0);
    if (!fs.begin(fsImpl)) {
        printf("❌ ERROR: Failed to initialize test filesystem\n");
        return 1;  // Failure
    }

    // Open and read the JSON file
    FileHandlePtr fh = fs.openRead("tests/profile/benchmark_1mb.json");
    if (!fh || !fh->valid()) {
        printf("⚠️  WARNING: Could not open tests/profile/benchmark_1mb.json\n");
        printf("   Skipping large JSON memory profile test.\n");
        printf("   Download it with: curl -o tests/profile/benchmark_1mb.json https://microsoftedge.github.io/Demos/json-dummy-data/1MB.json\n");
        return 0;  // Skip, not a failure
    }

    // Read file contents
    fl::size file_size = fh->size();
    fl::string large_json;
    large_json.resize(file_size);
    fl::size bytes_read = fh->read(reinterpret_cast<u8*>(&large_json[0]), file_size);
    fh->close();

    if (bytes_read != file_size) {
        printf("❌ ERROR: Read %zu bytes but expected %zu bytes\n", bytes_read, file_size);
        return 1;  // Failure
    }

    printf("✓ Loaded %zu bytes (%.2f KB)\n\n", bytes_read, bytes_read / 1024.0);

    // Run memory profiling on large JSON
    profile_json_memory("LARGE JSON MEMORY PROFILE (1MB Real-World)", large_json);

    return 0;  // Success
}

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================

int main(int argc, char** argv) {
    printf("================================================================================\n");
    printf("JSON MEMORY PROFILER\n");
    printf("================================================================================\n");
    printf("This profiler tracks ALL heap allocations using global malloc/free overrides.\n");
    printf("Compares ArduinoJson parse() vs custom parse2() memory usage.\n");
    printf("================================================================================\n\n");

    int failures = 0;

    // Run all profiling tests
    failures += test_phase1_validation();
    failures += test_small_json_profiling();
    failures += test_large_json_profiling();

    // Summary
    printf("\n\n");
    printf("================================================================================\n");
    if (failures == 0) {
        printf("✓✓✓ ALL PROFILING TESTS PASSED\n");
    } else {
        printf("✗✗✗ %d TEST(S) FAILED\n", failures);
    }
    printf("================================================================================\n");

    return failures;
}
