// ok no namespace fl
#pragma once

/// @file led_sysdefs_arm_stm32.h
/// LED system definitions trampoline for STM32 platforms
///
/// This is a dispatch header that routes to core-specific implementations:
/// - led_sysdefs_stm32_particle.h  - Particle Photon/Electron (STM32F2)
/// - led_sysdefs_stm32_libmaple.h  - Arduino_STM32 (Roger Clark libmaple)
/// - led_sysdefs_stm32duino.h      - Official STM32duino core
///
/// Each core-specific header defines:
/// - F_CPU clock frequency
///
/// Interrupt control (cli/sei) is handled separately by interrupts_stm32.h
/// to avoid collisions when including arduino.h

#include "platforms/is_platform.h"
#include "fl/stl/stdint.h"

#ifndef FL_IS_ARM
#error "This is not an arm board."
#endif

// ============================================================================
// Core Detection and Dispatch
// ============================================================================

#if defined(FL_IS_STM32_PARTICLE)
    #include "platforms/arm/stm32/led_sysdefs/led_sysdefs_stm32_particle.h"
#elif defined(FL_IS_STM32_LIBMAPLE)
    #include "platforms/arm/stm32/led_sysdefs/led_sysdefs_stm32_libmaple.h"
#elif defined(FL_IS_STM32_STMDUINO)
    #include "platforms/arm/stm32/led_sysdefs/led_sysdefs_stm32duino.h"
#else
    #error "Unknown STM32 core - cannot configure LED system definitions"
#endif

// ============================================================================
// Interrupt Control (cli/sei)
// ============================================================================
// Separate from led_sysdefs to avoid collisions with arduino.h
#include "platforms/arm/stm32/interrupts_stm32.h"

// ============================================================================
// Common Definitions (all STM32 cores)
// ============================================================================

// Interrupt threshold
#ifndef INTERRUPT_THRESHOLD
#define INTERRUPT_THRESHOLD 1
#endif

// Default to NOT allowing interrupts
#ifndef FASTLED_ALLOW_INTERRUPTS
#define FASTLED_ALLOW_INTERRUPTS 0
#endif

#if FASTLED_ALLOW_INTERRUPTS == 1
#define FASTLED_ACCURATE_CLOCK
#endif

// Register type definitions
typedef volatile fl::u8 RoReg; /**< Read only 8-bit register (volatile const unsigned int) */
typedef volatile fl::u8 RwReg; /**< Read-Write 8-bit register (volatile unsigned int) */

// Pin map - STM32 uses dynamic pin mapping
#define FASTLED_NO_PINMAP

// ============================================================================
// FL_IRAM - Fast RAM Placement
// ============================================================================
// Places code in fast RAM section (.text_ram) for time-critical functions
// Uses __COUNTER__ to generate unique section names for better debugging
#ifndef FL_IRAM
  // Helper macros for stringification
  #ifndef _FL_IRAM_STRINGIFY2
    #define _FL_IRAM_STRINGIFY2(x) #x
    #define _FL_IRAM_STRINGIFY(x) _FL_IRAM_STRINGIFY2(x)
  #endif

  // Generate unique section name using __COUNTER__ (e.g., .text_ram.0, .text_ram.1)
  #define _FL_IRAM_SECTION_NAME(counter) ".text_ram." _FL_IRAM_STRINGIFY(counter)
  #define FL_IRAM __attribute__((section(_FL_IRAM_SECTION_NAME(__COUNTER__))))
#endif
