#pragma once

#define FASTLED_INTERNAL  
#include "FastLED.h"

namespace fl {

#ifdef ESP8266
void esp8266_compile_tests() {
#if FASTLED_USE_PROGMEM != 0
#error "FASTLED_USE_PROGMEM should be 0 for ESP8266"
#endif

#if SKETCH_HAS_LOTS_OF_MEMORY != 0
#error "SKETCH_HAS_LOTS_OF_MEMORY should be 0 for ESP8266"
#endif

#if FASTLED_ALLOW_INTERRUPTS != 1
#error "FASTLED_ALLOW_INTERRUPTS should be 1 for ESP8266"
#endif

#ifndef ESP8266
#error "ESP8266 should be defined for ESP8266 platforms"
#endif

#ifndef F_CPU
#error "F_CPU should be defined for ESP8266"
#endif

// ESP8266 should have reasonable F_CPU (typically 80MHz or 160MHz)
#if F_CPU < 80000000
#error "ESP8266 F_CPU should be at least 80MHz"
#endif

// ESP8266 should have millis support
#ifndef FASTLED_HAS_MILLIS
#error "FASTLED_HAS_MILLIS should be defined for ESP8266"
#endif
}
#endif // ESP8266

#ifdef ESP32
void esp32_compile_tests() {
#ifndef ESP32
#error "ESP32 should be defined for ESP32 platforms"
#endif

#ifndef FASTLED_ESP32
#error "FASTLED_ESP32 should be defined for ESP32 platforms"
#endif

#if FASTLED_USE_PROGMEM != 0
#error "FASTLED_USE_PROGMEM should be 0 for ESP32 platforms"
#endif

#if SKETCH_HAS_LOTS_OF_MEMORY != 1
#error "SKETCH_HAS_LOTS_OF_MEMORY should be 1 for ESP32 platforms"
#endif

#if FASTLED_ALLOW_INTERRUPTS != 1
#error "FASTLED_ALLOW_INTERRUPTS should be 1 for ESP32 platforms"
#endif

#ifndef F_CPU
#error "F_CPU should be defined for ESP32 platforms"
#endif

// ESP32 should have reasonable F_CPU
#if F_CPU < 80000000
#error "ESP32 F_CPU should be at least 80MHz"
#endif

// Check for architecture-specific defines
#if !defined(FASTLED_XTENSA) && !defined(FASTLED_RISCV)
#error "Either FASTLED_XTENSA or FASTLED_RISCV should be defined for ESP32"
#endif

// ESP32 should have millis support
#ifndef FASTLED_HAS_MILLIS
#error "FASTLED_HAS_MILLIS should be defined for ESP32"
#endif

// Check for ESP32 driver capabilities
#if !defined(FASTLED_ESP32_HAS_RMT) && !defined(FASTLED_ESP32_HAS_CLOCKLESS_SPI)
#warning "No clockless drivers defined - you may not be able to drive WS2812 and similar chipsets"
#endif

// ESP32-S3 specific tests
#if defined(CONFIG_IDF_TARGET_ESP32S3)
    #ifndef CONFIG_IDF_TARGET_ESP32S3
    #error "CONFIG_IDF_TARGET_ESP32S3 should be defined for ESP32-S3"
    #endif
    
    // ESP32-S3 should have high F_CPU (typically 240MHz)
    #if F_CPU < 80000000
    #error "ESP32-S3 F_CPU should be at least 80MHz"
    #endif
    
    // ESP32-S3 specific features
    #ifdef FASTLED_USES_ESP32S3_I2S
    // I2S driver is being used - this is a valid configuration
    #endif
#endif
}
#endif // ESP32
}  // namespace fl
