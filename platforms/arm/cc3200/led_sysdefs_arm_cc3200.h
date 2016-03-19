#ifndef __INC_LED_SYSDEFS_ARM_CC3200_H
#define __INC_LED_SYSDEFS_ARM_CC3200_H

#define FASTLED_CC3200
#define FASTLED_ARM
#define FASTLED_ARM_M4

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

// Get some system include files
#include <avr/io.h>
#include <avr/interrupt.h> // for cli/se definitions

// Define the register types	//TODO: not sure what to do since no Arduino. probably can leave as is
#if defined(ARDUINO) // && ARDUINO < 150
typedef volatile       uint8_t RoReg; /**< Read only 8-bit register (volatile const unsigned int) */
typedef volatile       uint8_t RwReg; /**< Read-Write 8-bit register (volatile unsigned int) */
#endif

extern volatile uint32_t systick_millis_count;
#  define MS_COUNTER systick_millis_count

//TODO: Not sure what this is for, might not be necessary/accurate
// Default to not using PROGMEM since ARM doesn't provide it
#ifndef FASTLED_USE_PROGMEM
#define FASTLED_USE_PROGMEM 0
#endif

#endif