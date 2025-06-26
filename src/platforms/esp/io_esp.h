#pragma once

#include "esp_log.h"

namespace fl {

// Helper function for ESP32 native logging
static const char* FL_TAG = "FastLED";

// Print functions
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

// Input functions
inline int available_esp() {
    // ESP platforms - check Serial availability
    #ifdef ARDUINO_H
    if (Serial) {
        return Serial.available();
    }
    #endif
    return 0;
}

inline int read_esp() {
    // ESP platforms - read from Serial
    #ifdef ARDUINO_H
    if (Serial && Serial.available()) {
        return Serial.read();
    }
    #endif
    return -1;
}

} // namespace fl
