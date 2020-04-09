#ifndef __INC_LED_SYSDEFS_APOLLO3_H
#define __INC_LED_SYSDEFS_APOLLO3_H

#define FASTLED_APOLLO3

#ifndef INTERRUPT_THRESHOLD
#define INTERRUPT_THRESHOLD 1
#endif

// Default to allowing interrupts
#ifndef FASTLED_ALLOW_INTERRUPTS
#define FASTLED_ALLOW_INTERRUPTS 1
#endif

#if FASTLED_ALLOW_INTERRUPTS == 1
#define FASTLED_ACCURATE_CLOCK
#endif

#ifndef F_CPU
#define F_CPU 48000000
#endif

// Default to NOT using PROGMEM
#ifndef FASTLED_USE_PROGMEM
#define FASTLED_USE_PROGMEM 0
#endif

// data type defs
typedef volatile uint8_t RoReg; /**< Read only 8-bit register (volatile const unsigned int) */
typedef volatile uint8_t RwReg; /**< Read-Write 8-bit register (volatile unsigned int) */

#define FASTLED_NO_PINMAP

// reusing/abusing cli/sei defs for due
// These should be fine for the Apollo3. It has its own defines in cmsis_gcc.h
#define cli() __disable_irq();  //__disable_fault_irq();
#define sei() __enable_irq();  //__enable_fault_irq();

#endif
