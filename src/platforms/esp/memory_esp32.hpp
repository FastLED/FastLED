// IWYU pragma: private

/// @file platforms/esp/memory_esp32.hpp
/// ESP32 memory statistics implementation

#include "fl/stl/stdint.h"
#include "platforms/is_platform.h"

// Platform-specific includes for ESP32
#if defined(FL_IS_ESP32)
    // esp_heap_caps.h is a core ESP-IDF header, present identically under
    // Arduino-ESP32 (which bundles ESP-IDF) and bare ESP-IDF builds.
    FL_EXTERN_C_BEGIN
    #include "esp_heap_caps.h"
    #include "fl/stl/noexcept.h"
    FL_EXTERN_C_END
#endif

namespace fl {
namespace platforms {

/// @brief Get free heap memory (ESP32 implementation)
/// @return Number of free bytes in heap
inline size_t getFreeHeap() FL_NO_EXCEPT {
#if defined(FL_IS_ESP32)
    return heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
#else
    return 0;
#endif
}

/// @brief Get total heap size (ESP32 implementation)
/// @return Total heap size (not available on all ESP32 variants)
/// @note esp_get_heap_size() is not available on ESP32-C6 and some other variants
inline size_t getHeapSize() FL_NO_EXCEPT {
#if defined(FL_IS_ESP32)
    // esp_get_heap_size() is not available on all ESP32 variants (e.g., ESP32-C6)
    // Return 0 for now - could use esp_get_free_heap_size() as approximation if needed
    return 0;
#else
    return 0;
#endif
}

/// @brief Get minimum free heap (ESP32 implementation)
/// @return Minimum free heap ever recorded (low water mark)
inline size_t getMinFreeHeap() FL_NO_EXCEPT {
#if defined(FL_IS_ESP32)
    return heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
#else
    return 0;
#endif
}

} // namespace platforms
} // namespace fl
