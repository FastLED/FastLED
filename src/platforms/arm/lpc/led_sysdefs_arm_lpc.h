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

// Pull in the CMSIS Peripheral Access Layer (device header) when it is on the
// include path. It supplies the IRQ intrinsics, SysTick definitions, and the
// peripheral register maps (MRT/SYSCON, used by the cycle counter). When present
// we define FASTLED_HAS_CMSIS and suppress the hand-rolled fallbacks below (and
// in arm/common/m0clockless_c.h) so they don't clash with CMSIS's own
// declarations -- CMSIS provides __enable_irq/__disable_irq/__get_PRIMASK as
// functions, which the #ifndef guards cannot detect.
#if defined(__has_include)
#  if defined(FL_IS_ARM_LPC_845) && __has_include("LPC845.h")
#    include "LPC845.h"
#    define FASTLED_HAS_CMSIS 1
#  elif defined(FL_IS_ARM_LPC_804) && __has_include("LPC804.h")
#    include "LPC804.h"
#    define FASTLED_HAS_CMSIS 1
#  endif
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

// Default to MODE 1 (interrupts disabled during output) on LPC. The only working
// LPC clockless driver today is the shared C++ bit-banger (arm/common/
// m0clockless_c.h). Its MODE 2 path (FASTLED_ALLOW_INTERRUPTS=1) re-enables
// interrupts between pixels and -- when entered with interrupts already disabled,
// as ClocklessController::showPixels() does via cli() -- leaves them enabled for
// the rest of the frame, so the SysTick millis ISR fires mid-bit and stretches
// and jitters pulses. Interrupts-off gives clean WS2812 timing. Override to 1 if
// you need interrupts serviced mid-frame and can tolerate the timing impact.
#ifndef FASTLED_ALLOW_INTERRUPTS
#define FASTLED_ALLOW_INTERRUPTS 0
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

// When CMSIS is present (FASTLED_HAS_CMSIS) it provides the IRQ intrinsics and
// SysTick CTRL masks; defining them here too would clash (CMSIS declares the IRQ
// helpers as functions, and -Werror rejects redefining the masks). Only supply
// these fallbacks for builds without a reachable CMSIS device header -- e.g.
// ArduinoCore-LPC8xx's PlatformIO binding, which exposes only the active variant
// directory so board aliases like lpc845brk may not make LPC845.h includable.
#if !defined(FASTLED_HAS_CMSIS)

#ifndef __disable_irq
#define __disable_irq() __asm volatile("cpsid i" ::: "memory")
#endif

#ifndef __enable_irq
#define __enable_irq() __asm volatile("cpsie i" ::: "memory")
#endif

#ifndef __get_PRIMASK
#define __get_PRIMASK()                                                        \
    (__extension__({                                                           \
        unsigned int __pm;                                                     \
        __asm volatile("MRS %0, primask" : "=r"(__pm)::"memory");              \
        __pm;                                                                  \
    }))
#endif

// SysTick CTRL bit masks are architectural Cortex-M constants. CMSIS device
// headers (core_cm0plus.h) provide them; supply fallbacks for builds where no
// CMSIS device header is reachable from the platform translation units.
#ifndef SysTick_CTRL_ENABLE_Msk
#define SysTick_CTRL_ENABLE_Msk (1UL << 0)
#endif
#ifndef SysTick_CTRL_TICKINT_Msk
#define SysTick_CTRL_TICKINT_Msk (1UL << 1)
#endif
#ifndef SysTick_CTRL_CLKSOURCE_Msk
#define SysTick_CTRL_CLKSOURCE_Msk (1UL << 2)
#endif

#endif  // !FASTLED_HAS_CMSIS

#define cli() __disable_irq()
#define sei() __enable_irq()

#endif
