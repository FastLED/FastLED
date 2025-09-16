#pragma once

/// @file led_sysdefs_arm_mgm240.h
/// @brief System definitions for Silicon Labs MGM240 ARM Cortex-M33 microcontroller
///
/// This file provides platform-specific definitions for the MGM240SD22VNA
/// microcontroller used in the SparkFun Thing Plus Matter board.
///
/// Key specifications:
/// - ARM Cortex-M33 @ 39MHz
/// - 256KB RAM, 1.5MB Flash
/// - Silicon Labs EFM32/EFR32 GPIO architecture
/// - FreeRTOS compatibility with automatic detection

#include <stdint.h>

// Include Silicon Labs EMLIB GPIO for direct register access

#ifndef INTERRUPT_THRESHOLD
#define INTERRUPT_THRESHOLD 1
#endif

/// ARM platform identification
#ifndef FASTLED_ARM
#error "FASTLED_ARM must be defined before including this header. Ensure platforms/arm/is_arm.h is included first."
#endif
/// Use ARM Cortex-M3 compatibility mode for FastLED
/// (Cortex-M33 is backward compatible with M3 instruction set)
#define FASTLED_ARM_M3

/// Enable interrupt-aware timing for accurate LED control
#ifndef FASTLED_ALLOW_INTERRUPTS
#define FASTLED_ALLOW_INTERRUPTS 1
#endif

#if FASTLED_ALLOW_INTERRUPTS == 1
#define FASTLED_ACCURATE_CLOCK
#endif

/// @brief FreeRTOS-compatible critical section macros
/// Automatically detects FreeRTOS presence and uses task-safe critical sections.
/// Falls back to bare metal interrupt disable/enable when FreeRTOS is not available.
#ifdef __has_include
  #if __has_include("FreeRTOS.h")
    #include "FreeRTOS.h"
    /// Enter critical section (FreeRTOS task-safe)
    #define cli()  taskENTER_CRITICAL()
    /// Exit critical section (FreeRTOS task-safe)
    #define sei()  taskEXIT_CRITICAL()
  #else
    /// Enter critical section (bare metal)
    #define cli()  __disable_irq()
    /// Exit critical section (bare metal)
    #define sei()  __enable_irq()
  #endif
#else
  // Fallback for compilers without __has_include
  #define cli()  __disable_irq()
  #define sei()  __enable_irq()
#endif

/// @brief CPU frequency for MGM240SD22VNA
/// Default system clock frequency used for timing calculations.
/// This value is critical for accurate LED protocol timing.
#ifndef F_CPU
#define F_CPU 39000000L  ///< 39 MHz default system clock
#endif

/// ARM platforms don't use PROGMEM (flash storage macros)
#ifndef FASTLED_USE_PROGMEM
#define FASTLED_USE_PROGMEM 0
#endif

/// @brief Type definitions for ARM register access
typedef volatile uint32_t RwReg;  ///< Read-write register (32-bit)
typedef volatile uint32_t RoReg;  ///< Read-only register (32-bit)

/// @brief Arduino compatibility macros
/// These provide fallback pin manipulation functions when hardware FastPin is not available.
/// The MGM240 platform uses hardware FastPin, so these are rarely used.
#ifndef digitalPinToBitMask
#define digitalPinToBitMask(P) (1 << (P))
#endif

#ifndef digitalPinToPort
#define digitalPinToPort(P) ((P) / 8)
#endif

/// @brief Legacy Arduino port register emulation
/// These provide generic ARM memory-mapped register access for Arduino compatibility.
/// The MGM240 platform primarily uses Silicon Labs EMLIB GPIO functions instead.
#ifndef portOutputRegister
#define portOutputRegister(P) ((volatile uint32_t*)(0x40000000 + (P) * 0x400))
#endif

#ifndef portInputRegister
#define portInputRegister(P) ((volatile uint32_t*)(0x40000000 + (P) * 0x400))
#endif
