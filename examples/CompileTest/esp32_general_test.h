#pragma once

#include "FastLED.h"

void esp32_general_tests() {
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
}
