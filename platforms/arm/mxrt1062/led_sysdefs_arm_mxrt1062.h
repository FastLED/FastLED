#ifndef __INC_LED_SYSDEFS_ARM_MXRT1062_H
#define __INC_LED_SYSDEFS_ARM_MXRT1062_H

#define FASTLED_TEENSY4
#define FASTLED_ARM

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

#if (F_CPU == 96000000)
#define CLK_DBL 1
#endif

// Get some system include files
#include <avr/io.h>
#include <avr/interrupt.h> // for cli/se definitions

// Define the register types
#if defined(ARDUINO) // && ARDUINO < 150
typedef volatile       uint32_t RoReg; /**< Read only 8-bit register (volatile const unsigned int) */
typedef volatile       uint32_t RwReg; /**< Read-Write 8-bit register (volatile unsigned int) */
#endif

// extern volatile uint32_t systick_millis_count;
// #  define MS_COUNTER systick_millis_count

// Teensy4 provides progmem
#ifndef FASTLED_USE_PROGMEM
#define FASTLED_USE_PROGMEM 1
#endif


#endif
