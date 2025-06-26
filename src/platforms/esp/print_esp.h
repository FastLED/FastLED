#pragma once

#include "esp_log.h"

namespace fl {

// Helper function for ESP32 native logging
static const char* FL_TAG = "FastLED";

inline void print_esp(const char* str) {
    if (!str) return;
    // ESP32/ESP8266: Use native ESP-IDF logging
    // This avoids Arduino Serial dependency and uses ESP's native UART
    ESP_LOGI(FL_TAG, "%s", str);
}

inline void println_esp(const char* str) {
    if (!str) return;
    ESP_LOGI(FL_TAG, "%s\n", str);
}

} // namespace fl
