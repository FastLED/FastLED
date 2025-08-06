#pragma once

#include "fl/stdint.h"

#ifndef ESP8266
#define ESP8266
#endif

#define FASTLED_ESP8266

// Use system millis timer
#define FASTLED_HAS_MILLIS

typedef volatile uint32_t RoReg;
typedef volatile uint32_t RwReg;
typedef uint32_t prog_uint32_t;


// ESP8266 PROGMEM Support:
// PROGMEM is disabled by default due to alignment issues with DWORD reads.
// The ESP8266 requires 4-byte alignment for 32-bit progmem reads, but the
// TProgmemRGBPalette16 arrays were not properly aligned, causing crashes.
// 
// This has been fixed by adding FL_ALIGN_PROGMEM to the palette typedefs
// in fl/colorutils_misc.h. PROGMEM can now be enabled if needed, but is
// kept disabled for backwards compatibility and safety.
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
