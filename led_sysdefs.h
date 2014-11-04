#ifndef __INC_LED_SYSDEFS_H
#define __INC_LED_SYSDEFS_H

#if defined(__MK20DX128__) || defined(__MK20DX256__)

#define FASTLED_TEENSY3
#define FASTLED_ARM
#define FASTLED_ACCURATE_CLOCK
#ifndef INTERRUPT_THRESHOLD
#define INTERRUPT_THRESHOLD 5
#endif
#if (F_CPU == 96000000)
#define CLK_DBL 1
#endif

#elif defined(__SAM3X8E__)

#define FASTLED_ARM

// Setup DUE timer defines/channels/etc...
#ifndef DUE_TIMER_CHANNEL
#define DUE_TIMER_GROUP 0
#endif

#ifndef DUE_TIMER_CHANNEL
#define DUE_TIMER_CHANNEL 0
#endif

#define DUE_TIMER ((DUE_TIMER_GROUP==0) ? TC0 : ((DUE_TIMER_GROUP==1) ? TC1 : TC2))
#define DUE_TIMER_ID (ID_TC0 + (DUE_TIMER_GROUP*3) + DUE_TIMER_CHANNEL)
#define DUE_TIMER_VAL (DUE_TIMER->TC_CHANNEL[DUE_TIMER_CHANNEL].TC_CV << 1)
#define DUE_TIMER_RUNNING ((DUE_TIMER->TC_CHANNEL[DUE_TIMER_CHANNEL].TC_SR & TC_SR_CLKSTA) != 0)

#else

#define FASTLED_AVR
#define FASTLED_ACCURATE_CLOCK
#ifndef INTERRUPT_THRESHOLD
#define INTERRUPT_THRESHOLD 10
#endif

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

#if defined(ARDUINO) && defined(FASTLED_AVR) && ARDUINO >= 157
#error Arduion versions 1.5.7 and later not yet supported by FastLED for AVR
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

#define CLKS_PER_US (F_CPU/1000000)

#ifndef INTERRUPT_THRESHOLD
#define INTERRUPT_THRESHOLD 5
#endif

#endif
