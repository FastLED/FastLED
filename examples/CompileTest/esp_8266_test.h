#pragma once

#include "FastLED.h"

void esp_8266_tests() {
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
