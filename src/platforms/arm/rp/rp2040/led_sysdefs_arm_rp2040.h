// ok no namespace fl
#ifndef __INC_LED_SYSDEFS_ARM_RP2040_H
#define __INC_LED_SYSDEFS_ARM_RP2040_H

// RP2040-specific system defines
// RP2040 default clock: 125 MHz

// #define F_CPU clock_get_hz(clk_sys) // can't use runtime function call
// is the boot-time value in another var already for any platforms?
// it doesn't seem to be, so hardcode the sdk default of 125 MHz
#ifndef F_CPU
#ifdef VARIANT_MCK
#define F_CPU VARIANT_MCK
#else
#define F_CPU 125000000  // RP2040 default: 125 MHz
#endif
#endif

// Include common RP platform definitions
#include "platforms/arm/rp/rpcommon/led_sysdefs_rp_common.h"

#endif // __INC_LED_SYSDEFS_ARM_RP2040_H
