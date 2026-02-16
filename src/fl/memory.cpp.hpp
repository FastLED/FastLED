#include "fl/memory.h"

// Platform-specific headers
#include "platforms/is_platform.h"  // IWYU pragma: keep (needed for FL_IS_* macros)
#ifdef FL_IS_ESP32
// IWYU pragma: begin_keep
#include <Arduino.h>
// IWYU pragma: end_keep  // For ESP object
#include "esp_heap_caps.h"  // For heap_caps_get_free_size()
#elif defined(FL_IS_ESP8266)
// IWYU pragma: begin_keep
#include <Arduino.h>
// IWYU pragma: end_keep  // For ESP object
#elif defined(FL_IS_AVR)
// AVR doesn't have a built-in getFreeHeap, but we can compute it
// by measuring the gap between heap and stack pointers
extern "C" char *__brkval;  // Heap top pointer (set by malloc)
extern "C" char *__malloc_heap_start;  // Heap start (linker symbol)
#endif

namespace fl {

#ifdef FL_IS_ESP32
// ESP32 implementation - report both SRAM and PSRAM
HeapInfo getFreeHeap() {
    HeapInfo info;

    // MALLOC_CAP_INTERNAL: Internal SRAM (fast, always available)
    info.free_sram = static_cast<fl::size>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

    // MALLOC_CAP_SPIRAM: External PSRAM (slower, may not be available)
    // Returns 0 if PSRAM is not installed or not enabled
    info.free_psram = static_cast<fl::size>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    return info;
}

#elif defined(FL_IS_ESP8266)
// ESP8266 implementation - SRAM only (no PSRAM support)
HeapInfo getFreeHeap() {
    HeapInfo info;
    info.free_sram = static_cast<fl::size>(ESP.getFreeHeap());
    info.free_psram = 0;  // No PSRAM on ESP8266
    return info;
}

#elif defined(FL_IS_AVR)
// AVR implementation - compute free RAM between heap and stack (SRAM only)
HeapInfo getFreeHeap() {
    HeapInfo info;

    // Stack grows downward from top of RAM, heap grows upward
    // Free memory is the gap between them
    char stack_top;
    char *heap_top = __brkval ? __brkval : __malloc_heap_start;

    // Distance from heap top to current stack position
    info.free_sram = static_cast<fl::size>(&stack_top - heap_top);
    info.free_psram = 0;  // No PSRAM on AVR

    return info;
}

#else
// Default implementation for platforms without heap introspection
// (Native/Stub, WASM, ARM variants without malloc_stats, etc.)
HeapInfo getFreeHeap() {
    HeapInfo info;
    info.free_sram = 0;   // Not available
    info.free_psram = 0;  // Not available
    return info;
}

#endif

} // namespace fl
