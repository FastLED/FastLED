//NOTE: for consistency, will use the pin definitions from CC3200 (non module form) when defining pins
//because that is how the hardware API libraries & examples function.

#ifndef __INC_LED_SYSDEFS_ARM_CC3200_H
#define __INC_LED_SYSDEFS_ARM_CC3200_H

#define FASTLED_CC3200
#define FASTLED_ARM
#define FASTLED_ARM_M4
#define FASTLED_NO_PINMAP

#define FASTLED_SPI_BYTE_ONLY //for SPI, only send 1 byte at a time
#define FASTLED_ALL_PINS_HARDWARE_SPI

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
#define F_CPU 80000000
#endif


//#include <avr/io.h>
//#include <avr/interrupt.h> // for cli/se definitions


// Define the register types
#if defined(ENERGIA) || !defined(__TI_COMPILER_VERSION__) || defined(ccs) // && ARDUINO < 150
typedef volatile uint32_t RoReg; /**< Read only 32-bit register (volatile const unsigned int) */
typedef volatile uint32_t RwReg; /**< Read-Write 32-bit register (volatile unsigned int) */
#endif

#if !defined(ARDUINO)
#include "arduino_compat_cc3200.h"	// Get Arduino compatibility system include files
#endif

extern volatile uint32_t systick_millis_count;
#define MS_COUNTER systick_millis_count

//TODO: Not sure what this is for, might not be necessary/accurate
// Default to not using PROGMEM since ARM doesn't provide it
#ifndef FASTLED_USE_PROGMEM
#define FASTLED_USE_PROGMEM 0
#endif

#endif
