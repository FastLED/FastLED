/// @file debug_setup.hpp
/// Debug initialization for ESP32 platforms
///
/// This file provides debug setup utilities when FASTLED_DEBUG is defined.
/// Call fastled_debug_init() manually from your setup() function to enable
/// verbose logging and other debug features.
///
/// Example usage:
/// @code
/// void setup() {
///     fl::detail::fastled_debug_init();  // Enable verbose ESP logging
///     FastLED.addLeds<...>(...);
/// }
/// @endcode

#if defined(ESP32) && defined(FASTLED_DEBUG)

#include "esp_log.h"

namespace fl {
namespace detail {

/// Initialize debug settings for ESP32
///
/// Call this function from your setup() to enable verbose logging.
/// This was previously called automatically via FL_INIT, but now requires
/// manual invocation for explicit control over debug initialization.
inline void fastled_debug_init() {
    // Set all ESP-IDF components to VERBOSE logging
    esp_log_level_set("*", ESP_LOG_VERBOSE);

    // Additional debug initialization can be added here:
    // - Stack trace setup
    // - Heap debugging
    // - Performance counters
    // - etc.
}

}  // namespace detail
}  // namespace fl

#endif  // defined(ESP32) && defined(FASTLED_DEBUG)
