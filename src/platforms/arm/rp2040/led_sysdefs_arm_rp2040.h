#ifndef __INC_LED_SYSDEFS_ARM_RP2040_H
#define __INC_LED_SYSDEFS_ARM_RP2040_H

#define FASTLED_ARM
#define FASTLED_ARM_M0_PLUS

// TODO: PORT SPI TO HW
//#define FASTLED_SPI_BYTE_ONLY
#define FASTLED_FORCE_SOFTWARE_SPI

#define FASTLED_NO_PINMAP

typedef volatile uint32_t RoReg;
typedef volatile uint32_t RwReg;

// #define F_CPU clock_get_hz(clk_sys) // can't use runtime function call
// is the boot-time value in another var already for any platforms?
// it doesn't seem to be, so hardcode the sdk default of 125 MHz
#ifndef F_CPU
#define F_CPU 125000000
#endif

// 8MHz for PIO
#define CLOCKLESS_FREQUENCY 8000000

// Default to allowing interrupts
#ifndef FASTLED_ALLOW_INTERRUPTS
#define FASTLED_ALLOW_INTERRUPTS 1
#endif

// not sure if this is wanted? but it probably is
#if FASTLED_ALLOW_INTERRUPTS == 1
#define FASTLED_ACCURATE_CLOCK
#endif

// Default to no PROGMEM
#ifndef FASTLED_USE_PROGMEM
#define FASTLED_USE_PROGMEM 0
#endif

// Default to shared interrupt handler for clockless DMA
#ifndef FASTLED_RP2040_CLOCKLESS_IRQ_SHARED
#define FASTLED_RP2040_CLOCKLESS_IRQ_SHARED 1
#endif

#endif // __INC_LED_SYSDEFS_ARM_RP2040_H
