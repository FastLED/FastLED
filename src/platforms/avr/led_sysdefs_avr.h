// IWYU pragma: private

// ok no namespace fl
#ifndef __INC_LED_SYSDEFS_AVR_H
#define __INC_LED_SYSDEFS_AVR_H

#include "is_avr.h"

#define FASTLED_AVR

#ifndef INTERRUPT_THRESHOLD
#define INTERRUPT_THRESHOLD 2
#endif

#define FASTLED_SPI_BYTE_ONLY

// IWYU pragma: begin_keep
#include <avr/io.h>
#include <avr/interrupt.h> // for cli/se definitions
// IWYU pragma: end_keep

// Define the register types
typedef volatile       fl::u8 RoReg; /**< Read only 8-bit register (volatile const unsigned int) */
typedef volatile       fl::u8 RwReg; /**< Read-Write 8-bit register (volatile unsigned int) */


// Default to disallowing interrupts (may want to gate this on teensy2 vs. other arm platforms, since the
// teensy2 has a good, fast millis interrupt implementation)
#ifndef FASTLED_ALLOW_INTERRUPTS
#define FASTLED_ALLOW_INTERRUPTS 0
#endif

#if FASTLED_ALLOW_INTERRUPTS == 1
#define FASTLED_ACCURATE_CLOCK
#endif

// Default to using PROGMEM here
#ifndef FASTLED_USE_PROGMEM
#define FASTLED_USE_PROGMEM 1
#endif

#if defined(ARDUINO_AVR_DIGISPARK) || defined(ARDUINO_AVR_DIGISPARKPRO)
#ifndef NO_CORRECTION
#define NO_CORRECTION 1
#endif
#endif

extern "C" {
#  if defined(CORE_TEENSY) || defined(TEENSYDUINO)
extern volatile unsigned long timer0_millis_count;
#    define MS_COUNTER timer0_millis_count
#  elif defined(ATTINY_CORE)
extern volatile unsigned long millis_timer_millis;
#    define MS_COUNTER millis_timer_millis
#  elif defined(__AVR_ATtinyxy7__) || defined(__AVR_ATtinyxy6__) || defined(__AVR_ATtinyxy4__) || defined(__AVR_ATtinyxy2__) || defined(ARDUINO_attinyxy6)
extern volatile unsigned long timer_millis;
#    define MS_COUNTER timer_millis
#  elif defined(__AVR_ATmega4809__)
extern volatile unsigned long timer_millis;
#    define MS_COUNTER timer_millis
#  else
extern volatile unsigned long timer0_millis;
#    define MS_COUNTER timer0_millis
#  endif
};

// special defs for limited resource AVR environments
#ifdef FL_IS_AVR_LIMITED
#define FASTLED_NEEDS_YIELD

// Memory-saving defines for limited resource AVR platforms
// These platforms have very limited flash memory (typically 4-32KB)
// Disable expensive features to ensure sketches fit within memory constraints
#ifndef NO_CORRECTION
#define NO_CORRECTION 1  // Disable color correction (~196 bytes)
#endif
#ifndef NO_DITHERING
#define NO_DITHERING 1  // Disable temporal dithering (~130 bytes)
#endif
#ifndef NO_CLOCK_CORRECTION
#define NO_CLOCK_CORRECTION 1  // Disable clock correction (~50-100 bytes)
                                // Safe for limited AVR: These platforms typically don't require
                                // precise millis() timing during LED updates. Clock correction
                                // compensates for time spent with interrupts disabled, but
                                // limited AVR applications are usually simple (e.g., Blink) where
                                // slight timing drift is acceptable.
#endif
#ifndef FASTLED_INTERNAL
#define FASTLED_INTERNAL  // Suppress pragma messages
#endif
#endif

#if defined(ARDUINO) && (ARDUINO > 150) && !defined(IS_BEAN) && !defined (ARDUINO_AVR_DIGISPARK) && !defined (LIB8_TINY) && !defined (ARDUINO_AVR_LARDU_328E)
// don't need YIELD defined by the library
#else
#define FASTLED_NEEDS_YIELD
extern "C" void yield();
#endif
#endif
