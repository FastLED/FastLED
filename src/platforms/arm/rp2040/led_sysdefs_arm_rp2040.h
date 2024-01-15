#ifndef __INC_LED_SYSDEFS_ARM_RP2040_H
#define __INC_LED_SYSDEFS_ARM_RP2040_H

#include "hardware/sync.h"

// Explicitly include Arduino.h here so any framework-specific defines take
// priority.
#ifdef ARDUINO
#include <Arduino.h>
#endif

#define FASTLED_ARM
#define FASTLED_ARM_M0_PLUS

// TODO: PORT SPI TO HW
//#define FASTLED_SPI_BYTE_ONLY
#define FASTLED_FORCE_SOFTWARE_SPI
// Force FAST_SPI_INTERRUPTS_WRITE_PINS on becuase two cores running
// simultaneously could lead to data races on GPIO.
// This could potentially be optimised by adding a mask to FastPin's set and
// fastset, but for now it's probably safe to call that out of scope.
#ifndef FAST_SPI_INTERRUPTS_WRITE_PINS
#define FAST_SPI_INTERRUPTS_WRITE_PINS 1
#endif

#define FASTLED_NO_PINMAP

typedef volatile uint32_t RoReg;
typedef volatile uint32_t RwReg;

// #define F_CPU clock_get_hz(clk_sys) // can't use runtime function call
// is the boot-time value in another var already for any platforms?
// it doesn't seem to be, so hardcode the sdk default of 125 MHz
#ifndef F_CPU
#ifdef VARIANT_MCK
#define F_CPU VARIANT_MCK
#else
#define F_CPU 125000000
#endif
#endif

#ifndef VARIANT_MCK
#define VARIANT_MCK F_CPU
#endif

// 8MHz for PIO
// #define CLOCKLESS_FREQUENCY 8000000
#define CLOCKLESS_FREQUENCY F_CPU

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

// Default to non-blocking PIO-based implemnetation
#ifndef FASTLED_RP2040_CLOCKLESS_PIO
#define FASTLED_RP2040_CLOCKLESS_PIO 1
#endif

// Default to shared interrupt handler for clockless DMA
#ifndef FASTLED_RP2040_CLOCKLESS_IRQ_SHARED
#define FASTLED_RP2040_CLOCKLESS_IRQ_SHARED 1
#endif

// Default to disabling M0 assembly clockless implementation
#ifndef FASTLED_RP2040_CLOCKLESS_M0_FALLBACK
#define FASTLED_RP2040_CLOCKLESS_M0_FALLBACK 1
#endif

// SPI pin defs for old SDK ver
#ifndef PICO_DEFAULT_SPI
#define PICO_DEFAULT_SPI 0
#endif
#ifndef PICO_DEFAULT_SPI_SCK_PIN
#define PICO_DEFAULT_SPI_SCK_PIN 18
#endif
#ifndef PICO_DEFAULT_SPI_TX_PIN
#define PICO_DEFAULT_SPI_TX_PIN 19
#endif
#ifndef PICO_DEFAULT_SPI_RX_PIN
#define PICO_DEFAULT_SPI_RX_PIN 16
#endif
#ifndef PICO_DEFAULT_SPI_CSN_PIN
#define PICO_DEFAULT_SPI_CSN_PIN 17
#endif

#if !defined(cli) && !defined(sei)
static uint32_t saved_interrupt_status;
#define cli() (saved_interrupt_status = save_and_disable_interrupts())
#define sei() (restore_interrupts(saved_interrupt_status))
#endif

#endif // __INC_LED_SYSDEFS_ARM_RP2040_H
