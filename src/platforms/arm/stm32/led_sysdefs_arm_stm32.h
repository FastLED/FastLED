// ok no namespace fl
#ifndef __INC_LED_SYSDEFS_ARM_SAM_H
#define __INC_LED_SYSDEFS_ARM_SAM_H

#include "platforms/arm/is_arm.h"

#ifndef FASTLED_ARM
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
// F_CPU is already defined on stm32duino, but it's not constant.
#undef F_CPU
#define F_CPU 100000000
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

#endif
