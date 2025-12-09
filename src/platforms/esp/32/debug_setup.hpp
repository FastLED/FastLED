/// @file debug_setup.hpp
/// Debug initialization for ESP32 platforms
///
/// This file provides debug setup hooks when FASTLED_DEBUG is defined.
/// It runs before setup() using FL_INIT to enable verbose logging and other
/// debug features.

#if defined(ESP32) && defined(FASTLED_DEBUG)

#include "esp_log.h"
#include "fl/compiler_control.h"

namespace fl {
namespace detail {

/// Initialize debug settings before main() runs
void fastled_debug_init() {
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

FL_INIT(fl::detail::fastled_debug_init);

#endif  // defined(ESP32) && defined(FASTLED_DEBUG)
