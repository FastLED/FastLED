#ifndef __INC_LED_SYSDEFS_ARM_SAM_H
#define __INC_LED_SYSDEFS_ARM_SAM_H

#if defined(STM32F10X_MD) || defined(STM32F2XX)

#include <application.h>
#include <stdint.h>

#include "namespace.h"

#ifndef FASTLED_NAMESPACE_BEGIN
#define FASTLED_NAMESPACE_BEGIN namespace NSFastLED {
#define FASTLED_NAMESPACE_END }
#define FASTLED_USING_NAMESPACE using namespace NSFastLED;
#else
FASTLED_USING_NAMESPACE
#endif  // FASTLED_NAMESPACE_BEGIN

// reusing/abusing cli/sei defs for due
#define cli()  __disable_irq(); __disable_fault_irq();
#define sei() __enable_irq(); __enable_fault_irq();

#elif defined (__STM32F1__)

#include "cm3_regs.h"

#define cli() nvic_globalirq_disable()
#define sei() nvic_globalirq_enable()

#elif defined(STM32F1)
// stm32duino

#define cli() noInterrupts()
#define sei() interrupts()

#else
#error "Platform not supported"
#endif

#define FASTLED_ARM

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

#if !defined(STM32F1)
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
#define F_CPU 120000000
#elif defined(STM32F1)
// F_CPU is already defined on stm32duino, but it's not constant.
#undef F_CPU
#define F_CPU 72000000
#else
#define F_CPU 72000000
#endif

#if defined(STM32F2XX)
// Photon doesn't provide yield
#define FASTLED_NEEDS_YIELD
extern "C" void yield();
#endif

#endif // defined(STM32F10X_MD) || defined(STM32F2XX)
