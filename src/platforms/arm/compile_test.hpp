#pragma once

// IWYU pragma: private

#define FASTLED_INTERNAL  
#include "FastLED.h"
#include "fl/stl/static_assert.h"

namespace fl {
static void arm_compile_tests() {
#ifndef FL_IS_ARM
#error "FL_IS_ARM should be defined for ARM platforms"
#endif

#if FASTLED_USE_PROGMEM != 0 && FASTLED_USE_PROGMEM != 1
#error "FASTLED_USE_PROGMEM should be either 0 or 1 for ARM platforms"
#endif

#if !defined(SKETCH_HAS_LARGE_MEMORY_OVERRIDDEN)
#if defined(FL_IS_TEENSY_30) || defined(FL_IS_TEENSY_31) || defined(FL_IS_TEENSY_32)
    // Teensy 3.0/3.1/3.2 have limited memory (16KB-64KB RAM)
    #if SKETCH_HAS_LARGE_MEMORY != 0
    #error "SKETCH_HAS_LARGE_MEMORY should be 0 for Teensy 3.0/3.1/3.2"
    #endif
    #if SKETCH_HAS_HUGE_MEMORY != 0
    #error "SKETCH_HAS_HUGE_MEMORY should be 0 for Teensy 3.0/3.1/3.2"
    #endif
#elif defined(FL_IS_TEENSY_35) || defined(FL_IS_TEENSY_36) || defined(FL_IS_TEENSY_4X)
    // Teensy 3.5/3.6 have 256KB RAM, Teensy 4.x has 1MB RAM - plenty of memory
    #if SKETCH_HAS_LARGE_MEMORY != 1
    #error "SKETCH_HAS_LARGE_MEMORY should be 1 for Teensy 3.5/3.6/4.x"
    #endif
    #if SKETCH_HAS_HUGE_MEMORY != 1
    #error "SKETCH_HAS_HUGE_MEMORY should be 1 for Teensy 3.5/3.6/4.x"
    #endif
#elif defined(FL_IS_TEENSY_LC) || defined(ARDUINO_ARCH_RENESAS_UNO) || defined(STM32F1)
    // Teensy LC, Renesas UNO, and STM32F1 have limited memory
    #if SKETCH_HAS_LARGE_MEMORY != 0
    #error "SKETCH_HAS_LARGE_MEMORY should be 0 for Teensy LC, Renesas UNO, and STM32F1"
    #endif
    #if SKETCH_HAS_HUGE_MEMORY != 0
    #error "SKETCH_HAS_HUGE_MEMORY should be 0 for Teensy LC, Renesas UNO, and STM32F1"
    #endif
#elif defined(ARDUINO_ARCH_RP2040) || defined(PICO_RP2040) || defined(PICO_RP2350) \
   || defined(__SAMD51__) \
   || defined(STM32F4xx) || defined(STM32H7xx) || defined(ARDUINO_GIGA)
    // Huge memory ARM platforms (>= 256KB RAM)
    #if SKETCH_HAS_LARGE_MEMORY != 1
    #error "SKETCH_HAS_LARGE_MEMORY should be 1 for huge ARM platforms"
    #endif
    #if SKETCH_HAS_HUGE_MEMORY != 1
    #error "SKETCH_HAS_HUGE_MEMORY should be 1 for huge ARM platforms"
    #endif
#else
    // Most other ARM platforms have large memory but are not "huge"
    // (e.g., SAMD21, nRF52, generic Cortex-M)
    #if SKETCH_HAS_LARGE_MEMORY != 1
    #error "SKETCH_HAS_LARGE_MEMORY should be 1 for most ARM platforms"
    #endif
    #if SKETCH_HAS_HUGE_MEMORY != 0
    #error "SKETCH_HAS_HUGE_MEMORY should be 0 for generic ARM platforms (high tier, not huge)"
    #endif
#endif
#endif

#if FASTLED_ALLOW_INTERRUPTS != 1 && FASTLED_ALLOW_INTERRUPTS != 0
#error "FASTLED_ALLOW_INTERRUPTS should be either 0 or 1 for ARM platforms"
#endif

// Check that F_CPU is defined
#ifndef F_CPU
#error "F_CPU should be defined for ARM platforms"
#endif

// Specific ARM variant checks
#if defined(ARDUINO_ARCH_STM32) || defined(STM32F1)
    #if FASTLED_ALLOW_INTERRUPTS != 0
    #error "STM32 platforms should have FASTLED_ALLOW_INTERRUPTS set to 0"
    #endif
    #if FASTLED_USE_PROGMEM != 0
    #error "STM32 platforms should have FASTLED_USE_PROGMEM set to 0"
    #endif
#endif

#if defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_RASPBERRY_PI_PICO)
    #if FASTLED_USE_PROGMEM != 0
    #error "RP2040 platforms should have FASTLED_USE_PROGMEM set to 0"
    #endif
    #if FASTLED_ALLOW_INTERRUPTS != 1
    #error "RP2040 platforms should have FASTLED_ALLOW_INTERRUPTS set to 1"
    #endif
    #ifdef FASTLED_FORCE_SOFTWARE_SPI
    // RP2040 forces software SPI - this is expected
    #endif
#endif

#if defined(__MK20DX128__) || defined(__MK20DX256__) || defined(__MK66FX1M0__) || defined(__IMXRT1062__)
    // Teensy platforms that use PROGMEM
    #if FASTLED_USE_PROGMEM != 1
    #error "Teensy K20/K66/MXRT1062 platforms should have FASTLED_USE_PROGMEM set to 1"
    #endif
#endif

#if defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_SAM_DUE)
    #if FASTLED_USE_PROGMEM != 0
    #error "SAMD/SAM platforms should have FASTLED_USE_PROGMEM set to 0"
    #endif
#endif

#if defined(NRF52_SERIES) || defined(ARDUINO_ARCH_NRF52)
    #if FASTLED_USE_PROGMEM != 0
    #error "NRF52 platforms should have FASTLED_USE_PROGMEM set to 0"
    #endif
    #ifndef CLOCKLESS_FREQUENCY
    #error "NRF52 should have CLOCKLESS_FREQUENCY defined"
    #endif
#endif

// STM32F1 specific compile-time size validation
#if defined(STM32F1) || defined(__STM32F1__) || defined(STM32F1xx)
    // Static assert to ensure we're aware of memory constraints
    // STM32F103C8 has only 64KB flash and 20KB RAM
    FL_STATIC_ASSERT(sizeof(void*) == 4, "STM32F1 should be 32-bit platform");
    
    // Compile-time check for sketch memory usage awareness
    #if SKETCH_HAS_LARGE_MEMORY != 0
        // This helps catch cases where large data structures might be used
        #pragma message "STM32F1 Warning: Large memory structures may not fit in 20KB RAM"
    #endif
    

#endif
}
}  // namespace fl
