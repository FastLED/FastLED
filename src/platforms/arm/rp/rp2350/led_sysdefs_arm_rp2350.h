// ok no namespace fl
#ifndef __INC_LED_SYSDEFS_ARM_RP2350_H
#define __INC_LED_SYSDEFS_ARM_RP2350_H

// RP2350-specific system defines
// RP2350 default clock: 150 MHz

// #define F_CPU clock_get_hz(clk_sys) // can't use runtime function call
// is the boot-time value in another var already for any platforms?
// it doesn't seem to be, so hardcode the sdk default of 150 MHz
#ifndef F_CPU
#ifdef VARIANT_MCK
#define F_CPU VARIANT_MCK
#else
#define F_CPU 150000000  // RP2350 default: 150 MHz
#endif
#endif

// Include common RP platform definitions
#include "platforms/arm/rp/rpcommon/led_sysdefs_rp_common.h"

#endif // __INC_LED_SYSDEFS_ARM_RP2350_H
