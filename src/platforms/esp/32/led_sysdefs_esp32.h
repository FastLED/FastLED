#pragma once
#include "esp32-hal.h"
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

