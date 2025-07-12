#ifndef __INC_LED_SYSDEFS_ARM_SAM_H
#define __INC_LED_SYSDEFS_ARM_SAM_H

#if defined(STM32F10X_MD) || defined(STM32F2XX)

#include <application.h>
#include "fl/stdint.h"

#include "fl/namespace.h"

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

#elif defined(__STM32F4__)
// For STM32F4xx family (including F401), use CMSIS interrupt macro
#include <stdint.h> // for standard types


#define cli() nvic_globalirq_disable()
#define sei() nvic_globalirq_enable()

#elif defined(STM32F401xE) || defined(STM32F401xC) || defined(STM32F401) || defined(STM32F4)
#include "fl/stdint.h"

#include "fl/namespace.h"

#define cli() noInterrupts()
#define sei() interrupts()

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
#define F_CPU 120000000
// *** F401/F4 ADD SUPPORT START ***
#elif defined(STM32F401xE) || defined(STM32F401xC) || defined(STM32F401) || defined(__STM32F4__) || defined(STM32F4)
// STM32F401 typically runs at 84MHz by default; set F_CPU accordingly
#undef F_CPU
#define F_CPU 84000000
// *** F401/F4 ADD SUPPORT END ***
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

#endif // defined(STM32F10X_MD) || defined(STM32F2XX)

#endif