#ifndef __INC_LED_SYSDEFS_H
#define __INC_LED_SYSDEFS_H

#if defined(__MK20DX128__) || defined(__MK20DX256__)
#define FASTLED_TEENSY3
#define FASTLED_ARM
#if (F_CPU == 96000000)
#define CLK_DBL 1
#endif
#elif defined(__SAM3X8E__)
#define FASTLED_ARM
#else
#define FASTLED_AVR
#endif

#ifndef CLK_DBL
#define CLK_DBL 0
#endif

#if defined(FASTLED_AVR) || defined(FASTLED_TEENSY3)
#include <avr/io.h>
#include <avr/interrupt.h> // for cli/se definitions

// Define the rgister types
#if defined(ARDUINO) // && ARDUINO < 150
typedef volatile       uint8_t RoReg; /**< Read only 8-bit register (volatile const unsigned int) */
typedef volatile       uint8_t RwReg; /**< Read-Write 8-bit register (volatile unsigned int) */
#endif

#else
// reuseing/abusing cli/sei defs for due
#define cli()  __disable_irq(); __disable_fault_irq();
#define sei() __enable_irq(); __enable_fault_irq();

#endif

#if 0
#if defined(ARDUINO) && defined(FASTLED_AVR) && ARDUINO >= 157
#error Arduion versions 1.5.7 and later not yet supported by FastLED for AVR
#endif

#if defined(ARDUINO) && defined (FASTLED_AVR) && (__GNUC__ == 4) && (__GNUC_MINOR__ > 7)
#error gcc versions 4.8 and above are not yet supported by FastLED for AVR
#endif
#endif

// Arduino.h needed for convinience functions digitalPinToPort/BitMask/portOutputRegister and the pinMode methods.
#include<Arduino.h>

// Scaling macro choice
#if defined(LIB8_ATTINY)
#  define INLINE_SCALE(B, SCALE) delaycycles<3>()
#  warning "No hardware multiply, inline brightness scaling disabled"
#else
#   define INLINE_SCALE(B, SCALE) B = scale8_video(B, SCALE)
#endif

#endif
