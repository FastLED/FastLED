// IWYU pragma: private

// ok no namespace fl
#ifndef __INC_LPC_CPU_DEFAULTS_H
#define __INC_LPC_CPU_DEFAULTS_H

/// @file lpc_cpu_defaults.h
/// @brief Per-family CPU class and default F_CPU for NXP LPC parts.
///
/// Split out of led_sysdefs_arm_lpc.h so it has no vendor or typedef side
/// effects and a host test can check the per-family defaults.

#include "platforms/arm/is_arm.h"
#include "platforms/arm/lpc/is_lpc.h"

// CPU class per family:
//   LPC8xx        — Cortex-M0+
//   LPC11Uxx      — Cortex-M0   (NOT M0+)
//   LPC11xx       — Cortex-M0   (legacy LPC1110/1112/1114/1115, UM10398)
//   LPC15xx       — Cortex-M3   (different ISA, has DWT cycle counter)
#if defined(FL_IS_ARM_LPC_845) || defined(FL_IS_ARM_LPC_804)
#define FL_IS_ARM_M0_PLUS
#elif defined(FL_IS_ARM_LPC_11_USB) || defined(FL_IS_ARM_LPC_11_LEGACY)
#define FL_IS_ARM_M0
#elif defined(FL_IS_ARM_LPC_15)
#define FL_IS_ARM_M3
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
#elif defined(FL_IS_ARM_LPC_11_LEGACY)
// LPC1110 / LPC1112 / LPC1114 / LPC1115 — boot from the 12 MHz IRC; the
// PLL to 48/50 MHz is the user's SystemInit job, as for LPC11Uxx.
#define F_CPU 12000000UL
#elif defined(FL_IS_ARM_LPC_15)
// LPC15xx — IRC at boot is 12 MHz. PLL boost to 72 MHz on most variants.
// Default kept conservative; user code can override F_CPU once a PLL is
// brought up.
#define F_CPU 12000000UL
#endif
#endif

#endif  // __INC_LPC_CPU_DEFAULTS_H
