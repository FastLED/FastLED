#pragma once

// ESP32 has direct UART/puts for lightweight output, ESP8266 uses Arduino Serial
#ifdef ESP32
// Use low-level UART functions instead of ESP_LOG to avoid pulling in vfprintf
#include "driver/uart.h"
#include <string.h>  // for strlen
#define IO_HAS_IMPL 1
#else
#define IO_HAS_IMPL 0   
#endif

namespace fl {

// Helper function for ESP native lightweight output
#ifdef ESP32
static const char* FL_TAG = "FastLED";
#endif

// Print functions using low-level UART instead of ESP_LOG
inline void print_esp(const char* str) {
    if (!str) return;
    
#if IO_HAS_IMPL
    // ESP32: Use direct UART write instead of ESP_LOGI to avoid vfprintf dependency
    // This is much lighter than printf but still provides actual output
    uart_write_bytes(UART_NUM_0, str, strlen(str));
#endif
}

inline void println_esp(const char* str) {
    if (!str) return;
    
#if IO_HAS_IMPL
    // ESP32: Use direct UART write with newline
    uart_write_bytes(UART_NUM_0, str, strlen(str));
    uart_write_bytes(UART_NUM_0, "\n", 1);
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
