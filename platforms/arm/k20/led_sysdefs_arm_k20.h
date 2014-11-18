#ifndef __INC_LED_SYSDEFS_ARM_K20_H
#define __INC_LED_SYSDEFS_ARM_K20_H

#define FASTLED_TEENSY3
#define FASTLED_ARM
#define FASTLED_ACCURATE_CLOCK

#ifndef INTERRUPT_THRESHOLD
#define INTERRUPT_THRESHOLD 1
#endif

#if (F_CPU == 96000000)
#define CLK_DBL 1
#endif

// Get some system include files
#include <avr/io.h>
#include <avr/interrupt.h> // for cli/se definitions

// Define the rgister types
#if defined(ARDUINO) // && ARDUINO < 150
typedef volatile       uint8_t RoReg; /**< Read only 8-bit register (volatile const unsigned int) */
typedef volatile       uint8_t RwReg; /**< Read-Write 8-bit register (volatile unsigned int) */
#endif

#endif
