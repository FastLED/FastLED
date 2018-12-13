#ifndef __INC_LED_SYSDEFS_UNIX_H
#define __INC_LED_SYSDEFS_UNIX_H

#include <time.h>

#define FASTLED_UNIX

#ifndef INTERRUPT_THRESHOLD
#define INTERRUPT_THRESHOLD 2
#endif

// No fast SPI or pin support.
#define FASTLED_SPI_BYTE_ONLY
#define FASTLED_NO_PINMAP

// Arbitrary CPU frequency.
#define F_CPU 72000000

// Define the register types
typedef volatile uint8_t RoReg; /**< Read only 8-bit register (volatile const unsigned int) */
typedef volatile uint8_t RwReg; /**< Read-Write 8-bit register (volatile unsigned int) */

// Default to allowing interrupts
#ifndef FASTLED_ALLOW_INTERRUPTS
#define FASTLED_ALLOW_INTERRUPTS 1
#endif

#if FASTLED_ALLOW_INTERRUPTS == 1
#define FASTLED_ACCURATE_CLOCK
#endif

// Default to not using PROGMEM - it doesn't exist on Unix.
#ifndef FASTLED_USE_PROGMEM
#define FASTLED_USE_PROGMEM 0
#endif

extern "C" {
/// Offset (in milliseconds) to apply to millis() and micros().
extern volatile unsigned long timer0_millis_offset;
#define MS_COUNTER timer0_millis_offset
};

#define FASTLED_NEEDS_YIELD
extern "C" void yield();

#define FASTLED_NEEDS_MICROS
extern "C" unsigned long micros();

#define FASTLED_NEEDS_MILLIS
extern "C" unsigned long millis();

#endif
