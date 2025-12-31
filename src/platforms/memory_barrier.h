// ok no namespace fl
#pragma once

// Platform-specific memory barrier operations
// This file dispatches to the appropriate platform-specific memory_barrier.h file
// Pattern follows platforms/int.h and platforms/atomic.h

// ARM platform detection
#include "platforms/arm/is_arm.h"

#if defined(ESP32)
    // ESP32 family (ESP32, ESP32-S2, ESP32-S3, ESP32-C3, ESP32-C6, ESP32-H2, ESP32-P4)
    // Provides FL_MEMORY_BARRIER for Xtensa and RISC-V architectures
    #include "platforms/esp/32/core/memory_barrier.h"
#elif defined(ESP8266)
    // ESP8266 (Xtensa LX106 architecture)
    // Provides FL_MEMORY_BARRIER using memw instruction for ISR-to-main-thread synchronization
    #include "platforms/esp/8266/memory_barrier.h"
#elif defined(__AVR__)
    // AVR platforms currently have no memory barrier implementation
    // For ISR synchronization on AVR, use volatile variables
    #define FL_MEMORY_BARRIER ((void)0)
#elif defined(FASTLED_ARM)
    // ARM platforms (Teensy, SAMD, RP2040, STM32, nRF52, etc.)
    // Provides FL_MEMORY_BARRIER for ARM Cortex-M architectures
    #include "platforms/arm/memory_barrier.h"
#elif defined(__EMSCRIPTEN__)
    // WebAssembly / Emscripten
    // WASM is single-threaded, no memory barrier needed
    #define FL_MEMORY_BARRIER ((void)0)
#else
    // Default platform (desktop/generic)
    // Use compiler barrier for host builds
    #define FL_MEMORY_BARRIER __asm__ __volatile__ ("" ::: "memory")
#endif
