#pragma once

// ESP32 has ESP-IDF logging system, ESP8266 uses Arduino Serial
#ifdef ESP32
#include "esp_log.h"
#define IO_HAS_IMPL 1
#else
#define IO_HAS_IMPL 0   
#endif

namespace fl {

// Helper function for ESP native logging
#ifdef ESP32
static const char* FL_TAG = "FastLED";
#endif

// Print functions
inline void print_esp(const char* str) {
    if (!str) return;
    
#if IO_HAS_IMPL
    // ESP32: Use native ESP-IDF logging
    // This avoids Arduino Serial dependency and uses ESP's native UART
    ESP_LOGI(FL_TAG, "%s", str);
#endif
}

inline void println_esp(const char* str) {
    if (!str) return;
    
#if IO_HAS_IMPL
    // ESP32: Use native ESP-IDF logging with newline
    ESP_LOGI(FL_TAG, "%s\n", str);
#endif
}

// Input functions
inline int available_esp() {
    // ESP platforms - check Serial availability
    return 0;
}

inline int read_esp() {
    return -1;
}

} // namespace fl
