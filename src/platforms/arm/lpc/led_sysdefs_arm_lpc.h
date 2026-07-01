// IWYU pragma: private

// ok no namespace fl
#ifndef __INC_LED_SYSDEFS_ARM_LPC_H
#define __INC_LED_SYSDEFS_ARM_LPC_H

#include "platforms/arm/lpc/is_lpc.h"
#include "platforms/arm/is_arm.h"

#ifndef FL_IS_ARM
#error "FL_IS_ARM must be defined before including this header. Ensure platforms/arm/is_arm.h is included first."
#endif

// CPU class per family:
//   LPC8xx        — Cortex-M0+
//   LPC11Uxx      — Cortex-M0   (NOT M0+)
//   LPC15xx       — Cortex-M3   (different ISA, has DWT cycle counter)
#if defined(FL_IS_ARM_LPC_845) || defined(FL_IS_ARM_LPC_804)
#define FL_IS_ARM_M0_PLUS
#elif defined(FL_IS_ARM_LPC_11_USB)
#define FL_IS_ARM_M0
#elif defined(FL_IS_ARM_LPC_15)
#define FL_IS_ARM_M3
#endif

// The "modern" LPC GPIO controller (LPC8xx / LPC11Uxx / LPC15xx) exposes
// SET[port] and CLR[port] at byte offsets 0x2200 / 0x2280 from the controller
// base — far beyond the 5-bit imm5*4 encoding the M0/M0+ STR-immediate-offset
// instruction supports. The shared inline-assembly clockless driver assumes
// both offsets fit a single str-with-immediate; LPC cannot satisfy that, so
// the M0/M0+ paths route through the portable C++ implementation which
// performs an indexed store instead. (M3 isn't constrained by encoding —
// full Thumb-2 accepts arbitrary offsets — but the same path applies
// uniformly to keep the driver layout consistent across the LPC family.)
#ifndef FASTLED_M0_USE_C_IMPLEMENTATION
#define FASTLED_M0_USE_C_IMPLEMENTATION
#endif

#ifndef F_CPU
#if defined(FL_IS_ARM_LPC_845)
// LPC845 native FRO is 24 MHz (post-/2 divider direct path per UM11029
// sec. 4.1). 30 MHz is only reachable via the PLL, which neither the
// Arduino core (zackees/ArduinoCore-LPC8xx) nor fbuild programs by
// default. Both sources of truth (Arduino boards.txt + fbuild
// lpc845brk.json) declare f_cpu=24000000L. The previous 30 MHz default
// in this header introduced a ~25%% cycle-count drift on the bit-bang
// and SCT+DMA clockless drivers. Users who actually program the PLL
// can override via -DF_CPU=30000000UL.
#define F_CPU 24000000UL
#elif defined(FL_IS_ARM_LPC_804)
#define F_CPU 15000000UL
#elif defined(FL_IS_ARM_LPC_11_USB)
// LPC11U24 / LPC11U35 — IRC at boot is 12 MHz. PLL boost to 48 MHz is
// possible; default kept conservative since SystemInit is the user's
// responsibility on bare-metal builds.
#define F_CPU 12000000UL
#elif defined(FL_IS_ARM_LPC_15)
// LPC15xx — IRC at boot is 12 MHz. PLL boost to 72 MHz on most variants.
// Default kept conservative; user code can override F_CPU once a PLL is
// brought up.
#define F_CPU 12000000UL
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

#ifndef FASTLED_NO_PINMAP
#define FASTLED_NO_PINMAP
#endif

#define FASTLED_SPI_BYTE_ONLY

#include "fl/stl/stdint.h"

typedef volatile fl::u32 RoReg;
typedef volatile fl::u32 RwReg;
typedef fl::u32          prog_uint32_t;
#if !defined(ARDUINO)
typedef fl::u8           boolean;
#endif

#define PROGMEM
#define NO_PROGMEM
#define NEED_CXX_BITS

// Vendor CMSIS PAL header is on the include path via ArduinoCore-LPC8xx's
// variants/<chip>/ directory (zackees/ArduinoCore-LPC8xx#34 ships the full
// NXP CMSIS PAL; FastLED #3437 step 3 consumes it here). The peripheral
// typedefs (SCT_Type, DMA_Type, SYSCON_Type, SPI_Type, etc.) and pointer
// macros (SCT0, DMA0, SYSCON, SPI0, SPI1) come from these headers; the
// FastLED LPC drivers use them directly instead of hand-rolled shims.
// Arduino.h on the LPC8xx core defines INPUT / OUTPUT (and a few other
// names) as preprocessor macros for digitalRead / digitalWrite. The vendor
// SCT_Type / GPIO_Type structs in <LPC845.h> have members named INPUT /
// OUTPUT (UM11029 §16.6.7 / §16.6.10). Push the Arduino macros aside for
// the duration of the vendor-header include so the struct definitions don't
// get clobbered; pop them after so user code using pinMode(pin, OUTPUT)
// still works.
#pragma push_macro("INPUT")
#pragma push_macro("OUTPUT")
#undef INPUT
#undef OUTPUT
// IWYU pragma: begin_keep
#if defined(FL_IS_ARM_LPC_845)
#include <LPC845.h>
#elif defined(FL_IS_ARM_LPC_804)
#include <LPC804.h>
#endif
// IWYU pragma: end_keep
#pragma pop_macro("OUTPUT")
#pragma pop_macro("INPUT")

// Signal to shared ARM headers that this build has the CMSIS PAL
// (`__get_PRIMASK`, `__enable_irq`, `__disable_irq`, `SysTick`,
// etc.) on the include path via the vendor `<LPC845.h>` / `<LPC804.h>`
// includes above (which pull in `core_cm0plus.h`). Without this,
// `src/platforms/arm/common/m0clockless_c.h` re-defines the CMSIS
// intrinsics locally and the vendor + FastLED versions collide with a
// `redefinition of fl::u32 __get_PRIMASK()` error at compile time —
// exactly what `m0clockless_c.h:51` guards against with
// `#if !defined(FASTLED_HAS_CMSIS)`.
#ifndef FASTLED_HAS_CMSIS
#define FASTLED_HAS_CMSIS 1
#endif

// V7 of FastLED #3437: hard-require the vendor PAL. If the toolchain
// failed to put <LPC845.h> / <LPC804.h> on the include path, fail at
// preprocessing time rather than silently fall back on hand-rolled
// shims (the failure mode that motivated #3437).
#if defined(FL_IS_ARM_LPC_845) && !defined(SCT0_BASE)
#error "FL_IS_ARM_LPC_845 set but vendor <LPC845.h> did not put SCT0_BASE on the include path. Ensure the build is using zackees/ArduinoCore-LPC8xx at SHA 50d76e0d or newer (post zackees/ArduinoCore-LPC8xx#34, which ships the full NXP CMSIS PAL in variants/lpc845/)."
#endif
#if defined(FL_IS_ARM_LPC_804) && !defined(PLU_BASE)
#error "FL_IS_ARM_LPC_804 set but vendor <LPC804.h> did not put PLU_BASE on the include path. Ensure the build is using zackees/ArduinoCore-LPC8xx at SHA 50d76e0d or newer (post zackees/ArduinoCore-LPC8xx#34, which ships the full NXP CMSIS PAL in variants/lpc804/)."
#endif

// CMSIS-Core intrinsics — kept local as a fallback in case the toolchain
// CMSIS-Core package is not on the include path. The vendor LPC845.h /
// LPC804.h includes "core_cm0plus.h" which normally defines these.
#ifndef __disable_irq
#define __disable_irq() __asm volatile("cpsid i" ::: "memory")
#endif

#ifndef __enable_irq
#define __enable_irq() __asm volatile("cpsie i" ::: "memory")
#endif

#define cli() __disable_irq()
#define sei() __enable_irq()

#endif
