#pragma once

// IWYU pragma: private

// ok no namespace fl

/// @file is_lpc.h
/// @brief NXP LPC platform detection macros for FastLED
///
/// Detects NXP LPC families:
///   - LPC8xx (LPC845, LPC804) — ARM Cortex-M0+
///   - LPC11xx (LPC1114, LPC1115, LPC11U24, LPC11U35) — ARM Cortex-M0
///     (NOT M0+; the M0 ASM template applies instead of M0+)
///
/// Detection uses MCUXpresso / CMSIS device-header defines (e.g.
/// CPU_LPC845M301JBD48, CPU_LPC1115FBD48) as well as bare project-level defines
/// that fbuild / PlatformIO configurations are expected to provide
/// (__LPC845__, __LPC804__, __LPC11xx__).

// LPC845 (ARM Cortex-M0+, 30 MHz, 64KB Flash, 16KB SRAM)
#if defined(__LPC845__) || defined(LPC845) || defined(LPC845M301) || \
    defined(CPU_LPC845M301JBD48) || defined(CPU_LPC845M301JBD64) || \
    defined(CPU_LPC845M301JHI33) || defined(CPU_LPC845M301JHI48)
#define FL_LPC845
#endif

// LPC804 (ARM Cortex-M0+, 15 MHz, 32KB Flash, 4KB SRAM, integrated PLU)
#if defined(__LPC804__) || defined(LPC804) || defined(LPC804M101) || \
    defined(CPU_LPC804M101JDH20) || defined(CPU_LPC804M101JDH24) || \
    defined(CPU_LPC804M101JHI33)
#define FL_LPC804
#endif

// LPC11xx family (ARM Cortex-M0, up to 50 MHz, classic mbed-era hobbyist
// parts: LPC1114, LPC1115, LPC11U24, LPC11U35). This is the family
// detection portion of the FastLED/FastLED#2845 Stage 4 LPC11xx port
// scaffold — driver wiring is a follow-up. The clockless path will reuse
// the shared M0 ASM template (src/platforms/arm/common/m0clockless_asm.h,
// also used by nRF51).
//
// IMPORTANT: LPC11xx is Cortex-M0, NOT M0+. Do not let it land in any
// code path gated on FL_IS_ARM_M0_PLUS — that would mis-encode load/store
// offsets on parts that lack the M0+ instruction extensions. The led
// sysdefs follow-up will set FL_IS_ARM_M0 (not M0+) for this family.
#if defined(__LPC11xx__) || defined(LPC11xx) || \
    defined(CPU_LPC1114FBD48) || defined(CPU_LPC1114FHN33) || \
    defined(CPU_LPC1115FBD48) || \
    defined(CPU_LPC11U24FBD48) || defined(CPU_LPC11U24FHI33) || \
    defined(CPU_LPC11U35FBD48) || defined(CPU_LPC11U35FHI33) || \
    defined(CPU_LPC11U35FHN33)
#define FL_LPC11
#endif

// General LPC family roll-up (any LPC variant supported by this port)
#if defined(FL_LPC845) || defined(FL_LPC804) || defined(FL_LPC11)
#define FL_IS_ARM_LPC
#endif
