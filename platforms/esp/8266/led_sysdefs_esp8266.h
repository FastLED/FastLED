#pragma once

#ifndef ESP8266
#define ESP8266
#endif

#define FASTLED_ESP8266

// Use system millis timer
#define FASTLED_HAS_MILLIS

typedef volatile uint32_t RoReg;
typedef volatile uint32_t RwReg;
typedef uint32_t prog_uint32_t;
typedef uint8_t boolean;

// Default to NOT using PROGMEM here
#ifndef FASTLED_USE_PROGMEM
#define FASTLED_USE_PROGMEM 0
#endif

#ifndef FASTLED_ALLOW_INTERRUPTS
#define FASTLED_ALLOW_INTERRUPTS 1
#define INTERRUPT_THRESHOLD 1
#endif

#define NEED_CXX_BITS

// #define cli() os_intr_lock();
// #define sei() os_intr_lock();
