#pragma once

// ESP32 has ESP-IDF logging system, ESP8266 uses Arduino Serial
#ifdef ESP32
#include "esp_log.h"
#else
#include "Arduino.h"
#endif

namespace fl {

// Helper function for ESP native logging
#ifdef ESP32
static const char* FL_TAG = "FastLED";
#endif

// Print functions
inline void print_esp(const char* str) {
    if (!str) return;
    
#ifdef ESP32
    // ESP32: Use native ESP-IDF logging
    // This avoids Arduino Serial dependency and uses ESP's native UART
    ESP_LOGI(FL_TAG, "%s", str);
#else
    // ESP8266: Use Arduino Serial (ESP-IDF logging not available)
    #ifdef ARDUINO_H
    if (Serial) {
        Serial.print(str);
    }
    #endif
#endif
}

inline void println_esp(const char* str) {
    if (!str) return;
    
#ifdef ESP32
    // ESP32: Use native ESP-IDF logging with newline
    ESP_LOGI(FL_TAG, "%s\n", str);
#else
    // ESP8266: Use Arduino Serial println
    #ifdef ARDUINO_H
    if (Serial) {
        Serial.println(str);
    }
    #endif
#endif
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
