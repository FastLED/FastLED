/*
 * malloc_wrappers.cpp - Weak malloc/free wrapper implementations for nRF52
 *
 * PROBLEM SUMMARY:
 * ===============
 * The Adafruit nRF52 Arduino framework (used by PlatformIO's nordicnrf52 platform)
 * adds linker flags to wrap malloc/free/calloc/realloc in platform.txt:
 *   -Wl,--wrap=malloc -Wl,--wrap=free -Wl,--wrap=realloc -Wl,--wrap=calloc
 *
 * These flags were added in v1.1.0 (2021.09.24) to implement thread-safe memory
 * allocation for FreeRTOS environments, preventing heap corruption when the RTOS
 * scheduler and Arduino libraries concurrently access malloc/free.
 * See: https://github.com/adafruit/Adafruit_nRF52_Arduino/issues/35
 *
 * HOW --wrap WORKS:
 * ================
 * When you link with --wrap=malloc:
 *   1. All calls to malloc() are redirected to __wrap_malloc()
 *   2. Your __wrap_malloc() can do custom logic (locking, logging, etc.)
 *   3. __wrap_malloc() can call __real_malloc() to get the original malloc()
 *
 * Example:
 *   void* __wrap_malloc(size_t size) {
 *       mutex_lock();
 *       void* ptr = __real_malloc(size);  // Calls original malloc
 *       mutex_unlock();
 *       return ptr;
 *   }
 *
 * THE PLATFORMIO LDF PROBLEM:
 * ==========================
 * PlatformIO's Library Dependency Finder (LDF) scans code and includes files
 * based on what symbols it thinks are being used. The issue occurs because:
 *
 * 1. The framework's platform.txt ALWAYS adds --wrap linker flags
 * 2. The --wrap implementation in Adafruit framework lives in cores/nRF5/
 * 3. BUT those files are conditionally compiled or not always linked
 * 4. When linking FastLED sketches, the linker expects __wrap_malloc but it's missing
 * 5. Result: "undefined reference to `__wrap_malloc`"
 *
 * This happens even though:
 *   - We never explicitly use FreeRTOS features
 *   - The malloc wrappers aren't needed for our use case
 *   - It's purely a build system artifact from the Adafruit framework
 *
 * WHY WEAK SYMBOLS SOLVE THIS:
 * ============================
 * By providing __attribute__((weak)) implementations:
 *
 * 1. WHEN FRAMEWORK PROVIDES WRAPPERS:
 *    - Framework's strong symbols override our weak ones
 *    - You get the full thread-safe implementation
 *    - Our code is never linked in (discarded by linker)
 *
 * 2. WHEN FRAMEWORK DOESN'T PROVIDE WRAPPERS:
 *    - Our weak symbols satisfy the linker
 *    - They just pass through to __real_malloc() (no overhead)
 *    - Build succeeds, malloc works normally
 *
 * 3. NO PERFORMANCE IMPACT:
 *    - If framework provides wrappers: uses framework version (with mutex)
 *    - If framework doesn't: direct call to malloc (compiler likely inlines)
 *    - Either way, you get correct behavior
 *
 * COMPATIBILITY:
 * =============
 * This approach works across different build configurations:
 *   ✓ Arduino IDE with Adafruit nRF52 BSP (uses framework wrappers)
 *   ✓ PlatformIO with framework-arduinoadafruitnrf52 (uses framework wrappers)
 *   ✓ PlatformIO with basic nordicnrf52 platform (uses our weak wrappers)
 *   ✓ Sketches using FreeRTOS (gets thread-safety from framework)
 *   ✓ Sketches not using FreeRTOS (just works, no overhead)
 *
 * TECHNICAL DETAILS:
 * =================
 * The weak attribute tells the linker:
 *   - "Use this symbol IF nothing stronger is available"
 *   - Strong symbols (normal functions) always win
 *   - Multiple weak symbols = linker error
 *   - No symbol at all = our weak one is used
 *
 * This is perfect for providing "fallback" implementations that get replaced
 * when better/stronger implementations are available.
 *
 * WHY NOT JUST REMOVE --wrap FLAGS?
 * =================================
 * We can't control platform.txt - it's part of the Adafruit framework package.
 * Users install frameworks via PlatformIO package manager, we don't modify them.
 * Even if we could, removing the flags would break thread-safety for users who
 * actually need it (FreeRTOS + concurrent malloc calls).
 *
 * REFERENCES:
 * ==========
 * - Adafruit platform.txt with --wrap flags:
 *   https://github.com/adafruit/Adafruit_nRF52_Arduino/blob/master/platform.txt
 * - Thread-safe malloc issue:
 *   https://github.com/adafruit/Adafruit_nRF52_Arduino/issues/35
 * - GCC --wrap documentation:
 *   https://sourceware.org/binutils/docs/ld/Options.html#index-_002d_002dwrap_003dsymbol
 */

#if defined(__NRF52__) || defined(NRF52) || defined(ARDUINO_NRF52_ADAFRUIT)

#include <stdlib.h>

// These declarations tell the compiler that these functions exist somewhere
// They're provided by the linker when --wrap=malloc is used:
//   __real_malloc = the original libc malloc
//   __wrap_malloc = our wrapper that we must provide
extern "C" {
    // Forward declarations for the "real" functions (original libc implementations)
    void* __real_malloc(size_t size);
    void __real_free(void* ptr);
    void* __real_realloc(void* ptr, size_t size);
    void* __real_calloc(size_t nmemb, size_t size);

    // Weak wrapper implementations - these are used ONLY if the framework
    // doesn't provide its own strong implementations
    // They simply pass through to the real functions with zero overhead

    __attribute__((weak))
    void* __wrap_malloc(size_t size) {
        return __real_malloc(size);
    }

    __attribute__((weak))
    void __wrap_free(void* ptr) {
        __real_free(ptr);
    }

    __attribute__((weak))
    void* __wrap_realloc(void* ptr, size_t size) {
        return __real_realloc(ptr, size);
    }

    __attribute__((weak))
    void* __wrap_calloc(size_t nmemb, size_t size) {
        return __real_calloc(nmemb, size);
    }
}

/*
 * TESTING NOTES:
 * =============
 * To verify this works correctly:
 *
 * 1. Test with basic nRF52 sketch:
 *    - Should compile and link without errors
 *    - malloc/free should work normally
 *
 * 2. Test with FreeRTOS sketch:
 *    - Should still get thread-safe malloc if framework provides it
 *    - Check that framework's version is being used (nm shows strong symbols)
 *
 * 3. Check symbol linkage:
 *    nm firmware.elf | grep malloc
 *    - Look for 'W' (weak) or 'T' (strong) next to __wrap_malloc
 *    - 'W' means our version is used (passthrough)
 *    - 'T' means framework version is used (thread-safe)
 *
 * DEBUGGING:
 * =========
 * If you see linker errors about multiple definitions:
 *   - Framework is providing strong symbols (good!)
 *   - Our weak symbols should be ignored automatically
 *   - If not, check that __attribute__((weak)) is present
 *
 * If you still see undefined reference errors:
 *   - Check that this file is being compiled/linked
 *   - Verify platform macros (__NRF52__, etc.) are defined
 *   - Check build output for this .o file
 */

#endif // defined(__NRF52__) || defined(NRF52) || defined(ARDUINO_NRF52_ADAFRUIT)
