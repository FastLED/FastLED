// ok no namespace fl
#pragma once

#include "platforms/arm/is_arm.h"

#ifndef FL_IS_ARM
#error "This is not an arm board."
#endif

#if defined(STM32F10X_MD) || defined(STM32F2XX)

#include <application.h>
#include "fl/stl/stdint.h"

// reusing/abusing cli/sei defs for due
#define cli()  __disable_irq(); __disable_fault_irq();
#define sei() __enable_irq(); __enable_fault_irq();

#elif defined (__STM32F1__)

#include "cm3_regs.h"

#define cli() nvic_globalirq_disable()
#define sei() nvic_globalirq_enable()

#elif defined(STM32F1) || defined(STM32F4)
// stm32duino

#define cli() noInterrupts()
#define sei() interrupts()

#else
#error "Platform not supported"
#endif

#ifndef INTERRUPT_THRESHOLD
#define INTERRUPT_THRESHOLD 1
#endif

// Default to allowing interrupts
#ifndef FASTLED_ALLOW_INTERRUPTS
#define FASTLED_ALLOW_INTERRUPTS 0
#endif

#if FASTLED_ALLOW_INTERRUPTS == 1
#define FASTLED_ACCURATE_CLOCK
#endif

// pgmspace definitions
#define PROGMEM

#if !defined(STM32F1) && !defined(STM32F4)
// The stm32duino core already defines these
#define pgm_read_dword(addr) (*(const unsigned long *)(addr))
#define pgm_read_dword_near(addr) pgm_read_dword(addr)
#endif

// Default to NOT using PROGMEM here
#ifndef FASTLED_USE_PROGMEM
#define FASTLED_USE_PROGMEM 0
#endif

// data type defs
typedef volatile       uint8_t RoReg; /**< Read only 8-bit register (volatile const unsigned int) */
typedef volatile       uint8_t RwReg; /**< Read-Write 8-bit register (volatile unsigned int) */

#define FASTLED_NO_PINMAP

#if defined(STM32F2XX)
#ifndef F_CPU
#define F_CPU 120000000
#endif
#elif defined(STM32F1)
// F_CPU is already defined on stm32duino, but it's not constant.
#undef F_CPU
#define F_CPU 72000000
#elif defined(STM32F4)
// STM32F4 F_CPU Issue (GitHub issue #2163 & #612):
// - STM32duino defines F_CPU as SystemCoreClock (runtime variable, not compile-time constant)
// - FastLED requires compile-time constant for timing calculations (preprocessor expressions)
// - SOLUTION: Override F_CPU with conservative fallback, users can customize via build_opt.h
//
// Example build_opt.h content (place in sketch directory):
//   -UF_CPU
//   -DF_CPU=168000000UL  // STM32F407 Discovery (168 MHz)
//   -DF_CPU=100000000UL  // STM32F411 Black Pill (100 MHz)
//   -DF_CPU=84000000UL   // STM32F401 (84 MHz)
//   -DF_CPU=180000000UL  // STM32F429/F446 (180 MHz)
//
// PlatformIO equivalent in platformio.ini:
//   build_flags = -UF_CPU -DF_CPU=168000000UL
//
// Why this is critical:
// - Incorrect F_CPU breaks WS2812/SK6812 LED timing (see issue #2163)
// - STM32F407 @ 168 MHz with F_CPU=100MHz → timing 68% too fast → protocol violations
// - STM32F401 @ 84 MHz with F_CPU=100MHz → timing 16% too slow → marginal operation
//
// Common STM32F4 speeds ([STM32F4 series](https://www.st.com/stm32f4)):
// - STM32F401: 84 MHz
// - STM32F411: 100 MHz
// - STM32F407: 168 MHz
// - STM32F429/F446: 180 MHz

// Force override F_CPU (STM32duino's SystemCoreClock is not a compile-time constant)
#undef F_CPU
#define F_CPU 100000000UL
#warning "STM32F4: F_CPU set to 100 MHz fallback. LED timing may be incorrect for other clock speeds!"
#warning "For accurate timing: Add build_opt.h with: -UF_CPU -DF_CPU=<your_board_mhz>UL"
#warning "Or in platformio.ini: build_flags = -UF_CPU -DF_CPU=<your_board_mhz>UL"
#else
#ifndef F_CPU
#define F_CPU 72000000
#endif
#endif

#if defined(STM32F2XX)
// Photon doesn't provide yield
#define FASTLED_NEEDS_YIELD
extern "C" void yield();
#endif

// Platform-specific IRAM attribute for STM32
// STM32: Places code in fast RAM section (.text_ram) for time-critical functions
// Uses __COUNTER__ to generate unique section names (.text_ram.0, .text_ram.1, etc.)
// for better debugging and linker control
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
