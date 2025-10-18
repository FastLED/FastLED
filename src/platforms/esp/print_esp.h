#pragma once

// Use low-level UART functions instead of ESP_LOG to avoid pulling in vfprintf
#include "driver/uart.h"

namespace fl {

// Inline strlen to avoid including string.h
inline size_t _esp_strlen(const char* str) {
    size_t len = 0;
    while (str && *str++) len++;
    return len;
}

inline void print_esp(const char* str) {
    if (!str) return;
    // ESP32: Use direct UART write instead of ESP_LOGI to avoid vfprintf dependency
    uart_write_bytes(UART_NUM_0, str, _esp_strlen(str));
}

inline void println_esp(const char* str) {
    if (!str) return;
    // ESP32: Use direct UART write with newline
    uart_write_bytes(UART_NUM_0, str, _esp_strlen(str));
    uart_write_bytes(UART_NUM_0, "\n", 1);
}

} // namespace fl
