#ifndef __INC_LED_SYSDEFS_ARM_SAM_H
#define __INC_LED_SYSDEFS_ARM_SAM_H

#if defined(STM32F10X_MD) || defined (__STM32F1__)
#include "variants/defs/stm32f103_legacy.h"

#elif defined(STM32F1xx)
#include "variants/defs/stm32f103.h"

#else
 #error "Platform not supported"
#endif

#define FASTLED_ARM
#define FASTLED_NO_PINMAP

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
#define pgm_read_dword(addr) (*(const unsigned long *)(addr))
#define pgm_read_dword_near(addr) pgm_read_dword(addr)

// Default to NOT using PROGMEM here
#ifndef FASTLED_USE_PROGMEM
#define FASTLED_USE_PROGMEM 0
#endif

// data type defs
typedef volatile       uint8_t RoReg; /**< Read only 8-bit register (volatile const unsigned int) */
typedef volatile       uint8_t RwReg; /**< Read-Write 8-bit register (volatile unsigned int) */

#endif // __INC_LED_SYSDEFS_ARM_SAM_H
