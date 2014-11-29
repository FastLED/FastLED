#ifndef __INC_LED_SYSDEFS_AVR_H
#define __INC_LED_SYSDEFS_AVR_H

#define FASTLED_AVR

#ifndef INTERRUPT_THRESHOLD
#define INTERRUPT_THRESHOLD 2
#endif

#include <avr/io.h>
#include <avr/interrupt.h> // for cli/se definitions

// Define the rgister types
#if defined(ARDUINO) // && ARDUINO < 150
typedef volatile       uint8_t RoReg; /**< Read only 8-bit register (volatile const unsigned int) */
typedef volatile       uint8_t RwReg; /**< Read-Write 8-bit register (volatile unsigned int) */
#endif


// Default to disallowing interrupts (may want to gate this on teensy2 vs. other arm platforms, since the
// teensy2 has a good, fast millis interrupt implementation)
#ifndef FASTLED_ALLOW_INTERRUPTS
#define FASTLED_ALLOW_INTERRUPTS 0
#endif

#if FASTLED_ALLOW_INTERRUPTS == 1
#define FASTLED_ACCURATE_CLOCK
#endif

#  if defined(CORE_TEENSY)
extern volatile unsigned long timer0_millis_count;
#    define MS_COUNTER timer0_millis_count
#  else
extern volatile unsigned long timer0_millis;
#    define MS_COUNTER timer0_millis
#  endif

#endif
