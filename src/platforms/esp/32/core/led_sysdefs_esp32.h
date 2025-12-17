// ok no namespace fl
#pragma once
#include "fl/stl/stdint.h"
#include "sdkconfig.h"
#ifndef ESP32
#define ESP32
#endif

#define FASTLED_ESP32

#if CONFIG_IDF_TARGET_ARCH_RISCV
#define FASTLED_RISCV
#else
#define FASTLED_XTENSA
#endif

// Handling for older versions of ESP32 Arduino core
#if !defined(ESP_IDF_VERSION)
// Older versions of ESP_IDF only supported ESP32
#define CONFIG_IDF_TARGET_ESP32 1
// Define missing version macros.  Hard code older version 3.0 since actual version is unknown
#define ESP_IDF_VERSION_VAL(major, minor, patch) ((major << 16) | (minor << 8) | (patch))
#define ESP_IDF_VERSION ESP_IDF_VERSION_VAL(3, 0, 0)
#else
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 0, 0)
#define CONFIG_IDF_TARGET_ESP32 1
#endif
#endif

// Use system millis timer
#define FASTLED_HAS_MILLIS

typedef volatile uint32_t RoReg;
typedef volatile uint32_t RwReg;
typedef unsigned long prog_uint32_t;


// Default to NOT using PROGMEM here
#ifndef FASTLED_USE_PROGMEM
# define FASTLED_USE_PROGMEM 0
#endif

#ifndef FASTLED_ALLOW_INTERRUPTS
# define FASTLED_ALLOW_INTERRUPTS 1
# define INTERRUPT_THRESHOLD 0
#endif

#define NEED_CXX_BITS

// These can be overridden
#   define FASTLED_ESP32_RAW_PIN_ORDER

// Platform-specific IRAM attribute for ISR handlers and interrupt-sensitive functions
// ESP32: Places code in internal SRAM for fast, interrupt-safe execution
// Uses __COUNTER__ to generate unique section names (.iram1.text.0, .iram1.text.1, etc.)
// for better debugging and linker control
#ifndef FL_IRAM
  // Helper macros for stringification
  #ifndef _FL_IRAM_STRINGIFY2
    #define _FL_IRAM_STRINGIFY2(x) #x
    #define _FL_IRAM_STRINGIFY(x) _FL_IRAM_STRINGIFY2(x)
  #endif

  // ESP32: IRAM_ATTR is typically provided by the framework
  // Arduino ESP32: Already defined by framework
  // ESP-IDF: Available via esp_attr.h
  #if __has_include("esp_attr.h") && !defined(IRAM_ATTR)
    #ifdef __cplusplus
      extern "C" {
      #include "esp_attr.h"  // Provides IRAM_ATTR for ESP-IDF
      }
    #else
      #include "esp_attr.h"
    #endif
  #endif

  #ifndef IRAM_ATTR
    #define IRAM_ATTR __attribute__((section(".iram1.text")))
  #endif

  // Generate unique section name using __COUNTER__ (e.g., .iram1.text.0, .iram1.text.1)
  #define _FL_IRAM_SECTION_NAME(counter) ".iram1.text." _FL_IRAM_STRINGIFY(counter)
  #define FL_IRAM __attribute__((section(_FL_IRAM_SECTION_NAME(__COUNTER__))))
#endif
