#pragma once

// IWYU pragma: private

#define FASTLED_INTERNAL  
#include "FastLED.h"

namespace fl {

#ifdef ESP8266
void esp8266_compile_tests() {
// Pinned to 0, but note the flag is inert on ESP8266: fastled_progmem.h
// selects progmem_esp8266.h from its `#elif defined(ESP8266)` arm, before
// the `#if (FASTLED_USE_PROGMEM == 1)` test is ever reached. Tables are
// flash-resident either way. This assert exists so the declared value and
// the platform's real behavior don't drift further apart -- setting it to 1
// would not move data, only make the header claim something new. See #743.
#if FASTLED_USE_PROGMEM != 0
#error "FASTLED_USE_PROGMEM must stay 0 on ESP8266 (it is inert there; PROGMEM is selected by platform dispatch in fastled_progmem.h, not by this flag)"
#endif

#if !defined(SKETCH_HAS_LARGE_MEMORY_OVERRIDDEN)
#if SKETCH_HAS_LARGE_MEMORY != 0
#error "SKETCH_HAS_LARGE_MEMORY should be 0 for ESP8266"
#endif
#if SKETCH_HAS_HUGE_MEMORY != 0
#error "SKETCH_HAS_HUGE_MEMORY should be 0 for ESP8266"
#endif
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

// The Arduino NodeMCU variant defines D5 as the raw GPIO number 14. FastLED
// must not select its legacy board-label mapping automatically, because that
// mapping accepts only 0..10 and would reject D5 (and double-map D1..D4).
#if defined(ARDUINO_ESP8266_NODEMCU) && defined(FL_ESP8266_PIN_ORDER_DEFAULTED)
#ifndef FASTLED_ESP8266_RAW_PIN_ORDER
#error "The default ESP8266 pin order must accept Arduino Dn constants as raw GPIO numbers"
#endif
#ifdef FASTLED_ESP8266_NODEMCU_PIN_ORDER
#error "The legacy NodeMCU board-label mapping must remain opt-in"
#endif
    FL_STATIC_ASSERT(FastPin<14>::validpin(),
                     "NodeMCU D5 (GPIO 14) must be a valid FastLED pin");
#endif
}
#endif // FL_IS_ESP8266

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

#if !defined(SKETCH_HAS_LARGE_MEMORY_OVERRIDDEN)
#if SKETCH_HAS_LARGE_MEMORY != 1
#error "SKETCH_HAS_LARGE_MEMORY should be 1 for ESP32 platforms"
#endif
#if SKETCH_HAS_HUGE_MEMORY != 1
#error "SKETCH_HAS_HUGE_MEMORY should be 1 for ESP32 platforms"
#endif
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

// ESP32 variants without a true-SPI Channel driver must keep AUTO usable for
// clocked chipsets through the portable GPIO fallback.
#if defined(FL_IS_ESP_32C2) || defined(FL_IS_ESP_32C3) || defined(FL_IS_ESP_32S2)
FL_STATIC_ASSERT(DefaultBus<SpiChipsetConfig>::value == Bus::BIT_BANG,
                 "ESP32 variants without true-SPI Channel support must default to BIT_BANG");
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
    
    // ESP32-S3 specific features (I2S_SPI and LCD_SPI channel drivers)
#endif
}
#endif // FL_IS_ESP32
}  // namespace fl
