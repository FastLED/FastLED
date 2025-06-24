#pragma once

#include "FastLED.h"

void esp32_s3_tests() {
#if FASTLED_USE_PROGMEM != 0
#error "FASTLED_USE_PROGMEM should be 0 for ESP32-S3"
#endif

#if SKETCH_HAS_LOTS_OF_MEMORY != 1
#error "SKETCH_HAS_LOTS_OF_MEMORY should be 1 for ESP32-S3"
#endif

#if FASTLED_ALLOW_INTERRUPTS != 1
#error "FASTLED_ALLOW_INTERRUPTS should be 1 for ESP32-S3"
#endif

#ifndef ESP32
#error "ESP32 should be defined for ESP32-S3"
#endif

#ifndef CONFIG_IDF_TARGET_ESP32S3
#error "CONFIG_IDF_TARGET_ESP32S3 should be defined for ESP32-S3"
#endif

#ifndef F_CPU
#error "F_CPU should be defined for ESP32-S3"
#endif

// ESP32-S3 should have high F_CPU (typically 240MHz)
#if F_CPU < 80000000
#error "ESP32-S3 F_CPU should be at least 80MHz"
#endif

// ESP32-S3 should have millis support
#ifndef FASTLED_HAS_MILLIS
#error "FASTLED_HAS_MILLIS should be defined for ESP32-S3"
#endif

// ESP32-S3 specific features
#ifdef FASTLED_USES_ESP32S3_I2S
// I2S driver is being used - this is a valid configuration
#endif
}
