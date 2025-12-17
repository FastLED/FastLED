// ok no namespace fl
#pragma once

#include "fl/stl/stdint.h"

#ifndef ESP8266
#define ESP8266
#endif

#define FASTLED_ESP8266

// Use system millis timer
#define FASTLED_HAS_MILLIS

typedef volatile uint32_t RoReg;
typedef volatile uint32_t RwReg;
typedef uint32_t prog_uint32_t;


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
#if !defined(FASTLED_ESP8266_RAW_PIN_ORDER) && !defined(FASTLED_ESP8266_NODEMCU_PIN_ORDER) && !defined(FASTLED_ESP8266_D1_PIN_ORDER)
# ifdef ARDUINO_ESP8266_NODEMCU
#   define FASTLED_ESP8266_NODEMCU_PIN_ORDER
# else
#   define FASTLED_ESP8266_RAW_PIN_ORDER
# endif
#endif

// Platform-specific IRAM attribute for ISR handlers and interrupt-sensitive functions
// ESP8266: Places code in internal SRAM for fast, interrupt-safe execution
// Uses __COUNTER__ to generate unique section names (.iram.text.0, .iram.text.1, etc.)
// for better debugging and linker control
#ifndef FL_IRAM
  // Helper macros for stringification
  #ifndef _FL_IRAM_STRINGIFY2
    #define _FL_IRAM_STRINGIFY2(x) #x
    #define _FL_IRAM_STRINGIFY(x) _FL_IRAM_STRINGIFY2(x)
  #endif

  // ESP8266: IRAM_ATTR is provided by Arduino ESP8266 SDK (via ets_sys.h -> c_types.h)
  // No need to include headers - it's already defined by the platform
  #ifndef IRAM_ATTR
    #define IRAM_ATTR __attribute__((section(".iram.text")))
  #endif

  // Generate unique section name using __COUNTER__ (e.g., .iram.text.0, .iram.text.1)
  #define _FL_IRAM_SECTION_NAME(counter) ".iram.text." _FL_IRAM_STRINGIFY(counter)
  #define FL_IRAM __attribute__((section(_FL_IRAM_SECTION_NAME(__COUNTER__))))
#endif
