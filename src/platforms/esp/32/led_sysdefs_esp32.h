#pragma once
#include "esp32-hal.h"
#ifndef ESP32
#define ESP32
#endif

#define FASTLED_ESP32

#if CONFIG_IDF_TARGET_ARCH_RISCV
#define FASTLED_RISCV
#endif

#if CONFIG_IDF_TARGET_ARCH_XTENSA || CONFIG_XTENSA_IMPL
#define FASTLED_XTENSA
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

