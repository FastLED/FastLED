// ok no namespace fl
#pragma once

// IWYU pragma: public

// Platform-specific memory barrier operations
// This file dispatches to the appropriate platform-specific memory_barrier.h file
// which defines FL_PLATFORMS_MEMORY_BARRIER, then defines FL_MEMORY_BARRIER from it.
//
// Uses platform-specific hardware barrier (memw, fence, dmb) when available,
// falls back to compiler barrier on generic platforms.
//
// Pattern follows platforms/int.h and platforms/atomic.h

// Platform and compiler detection
#include "platforms/is_platform.h"
// ARM platform detection
#include "platforms/arm/is_arm.h"

#if defined(FL_IS_ESP32)
    // ESP32 family (ESP32, ESP32-S2, ESP32-S3, ESP32-C3, ESP32-C6, ESP32-H2, ESP32-P4)
    // Provides FL_PLATFORMS_MEMORY_BARRIER for Xtensa and RISC-V architectures
    #include "platforms/esp/32/core/memory_barrier.h"
#elif defined(FL_IS_ESP8266)
    // ESP8266 (Xtensa LX106 architecture)
    // Provides FL_PLATFORMS_MEMORY_BARRIER using memw instruction
    #include "platforms/esp/8266/memory_barrier.h"
#elif defined(FL_IS_AVR)
    // AVR platforms currently have no memory barrier implementation
    // For ISR synchronization on AVR, use volatile variables
    #define FL_PLATFORMS_MEMORY_BARRIER ((void)0)
#elif defined(FL_IS_ARM)
    // ARM platforms (Teensy, SAMD, RP2040, STM32, nRF52, etc.)
    // Provides FL_PLATFORMS_MEMORY_BARRIER for ARM Cortex-M architectures
    #include "platforms/arm/memory_barrier.h"
#elif defined(FL_IS_WASM)
    // WebAssembly / Emscripten
    // WASM is single-threaded, no memory barrier needed
    #define FL_PLATFORMS_MEMORY_BARRIER ((void)0)
#else
    // Default platform (desktop/generic)
    // Use compiler barrier for host builds
    #define FL_PLATFORMS_MEMORY_BARRIER __asm__ __volatile__ ("" ::: "memory")
#endif

// Define FL_MEMORY_BARRIER from platform-specific macro
#ifndef FL_MEMORY_BARRIER
  #ifdef FL_PLATFORMS_MEMORY_BARRIER
    #define FL_MEMORY_BARRIER FL_PLATFORMS_MEMORY_BARRIER
  #elif defined(FL_IS_GCC) || defined(FL_IS_CLANG)
    #define FL_MEMORY_BARRIER __asm__ __volatile__("" ::: "memory")
  #else
    #define FL_MEMORY_BARRIER ((void)0)
  #endif
#endif

