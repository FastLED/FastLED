// IWYU pragma: private

// ok no namespace fl
#ifndef __INC_LED_SYSDEFS_ARM_LPC_H
#define __INC_LED_SYSDEFS_ARM_LPC_H

#include "platforms/arm/lpc/is_lpc.h"
#include "platforms/arm/is_arm.h"

#ifndef FL_IS_ARM
#error "FL_IS_ARM must be defined before including this header. Ensure platforms/arm/is_arm.h is included first."
#endif

#define FL_IS_ARM_M0_PLUS

// The LPC8xx GPIO controller exposes SET[port] and CLR[port] at byte offsets
// 0x2200 / 0x2280 from the controller base - far beyond the 5-bit imm5*4
// encoding the M0/M0+ STR-immediate-offset instruction supports. The shared
// inline-assembly clockless driver assumes both offsets fit a single
// str-with-immediate; LPC cannot satisfy that, so route through the portable
// C++ implementation which performs an indexed store instead.
#ifndef FASTLED_M0_USE_C_IMPLEMENTATION
#define FASTLED_M0_USE_C_IMPLEMENTATION
#endif

#ifndef F_CPU
#if defined(FL_LPC845)
#define F_CPU 30000000UL
#elif defined(FL_LPC804)
#define F_CPU 15000000UL
#endif
#endif

#ifndef VARIANT_MCK
#define VARIANT_MCK F_CPU
#endif

#ifndef FASTLED_ALLOW_INTERRUPTS
#define FASTLED_ALLOW_INTERRUPTS 1
#endif

#ifndef FASTLED_USE_PROGMEM
#define FASTLED_USE_PROGMEM 0
#endif

#define FASTLED_SPI_BYTE_ONLY

#include "fl/stl/stdint.h"

typedef volatile fl::u32 RoReg;
typedef volatile fl::u32 RwReg;
typedef fl::u32          prog_uint32_t;
typedef fl::u8           boolean;

#define PROGMEM
#define NO_PROGMEM
#define NEED_CXX_BITS

// CMSIS device header from MCUXpresso SDK provides __disable_irq / __enable_irq
// via core_cm0plus.h. Some integrations only pull in the SoC header (e.g.
// LPC845.h / LPC804.h), which transitively includes the core header.
// IWYU pragma: begin_keep
#if defined(FL_LPC845)
#include <LPC845.h>
#elif defined(FL_LPC804)
#include <LPC804.h>
#endif
// IWYU pragma: end_keep

#define cli() __disable_irq()
#define sei() __enable_irq()

#endif
