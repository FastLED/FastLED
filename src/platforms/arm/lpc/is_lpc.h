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
///
/// **Macro naming convention** (FastLED #2999): every platform / subtype
/// detection macro starts with `FL_IS_` and is nested under its family
/// roll-up — e.g. `FL_IS_ARM_LPC_845` lives under `FL_IS_ARM_LPC`. Bare
/// `FL_LPC*` names are not used. The `__LPC*` / `LPC*` / `CPU_LPC*`
/// inputs above come from the toolchain / vendor headers and are NOT
/// renamed (we read them, we don't define them).

// LPC845 (ARM Cortex-M0+, 30 MHz, 64KB Flash, 16KB SRAM)
#if defined(__LPC845__) || defined(LPC845) || defined(LPC845M301) || \
    defined(CPU_LPC845M301JBD48) || defined(CPU_LPC845M301JBD64) || \
    defined(CPU_LPC845M301JHI33) || defined(CPU_LPC845M301JHI48)
#define FL_IS_ARM_LPC_845
#endif

// LPC804 (ARM Cortex-M0+, 15 MHz, 32KB Flash, 4KB SRAM, integrated PLU)
#if defined(__LPC804__) || defined(LPC804) || defined(LPC804M101) || \
    defined(CPU_LPC804M101JDH20) || defined(CPU_LPC804M101JDH24) || \
    defined(CPU_LPC804M101JHI33)
#define FL_IS_ARM_LPC_804
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
// Sub-family split:
//   FL_IS_ARM_LPC_11_USB    — LPC11U24 / LPC11U35 ("USB-capable" M0
//     parts). Modern GPIO controller at 0xA0000000 with the same DIR/
//     MASK/PIN/SET/CLR/NOT layout as LPC8xx per UM10462 §9. Can reuse
//     the LPC8xx fastpin + M0 ASM clockless path (NOT M0+).
//   FL_IS_ARM_LPC_11_LEGACY — LPC1110 / LPC1112 / LPC1114 / LPC1115.
//     Legacy GPIO controller at 0x50000000 with 12-bit "masked access"
//     semantics per UM10398 §12. Different fastpin layout from LPC8xx;
//     driver wiring deferred — emit a clear `#error` so users on these
//     chips know.
#if defined(__LPC11Uxx__) || defined(LPC11Uxx) || \
    defined(CPU_LPC11U24FBD48) || defined(CPU_LPC11U24FHI33) || \
    defined(CPU_LPC11U35FBD48) || defined(CPU_LPC11U35FHI33) || \
    defined(CPU_LPC11U35FHN33)
#define FL_IS_ARM_LPC_11_USB
#endif

#if defined(__LPC11xx__) || defined(LPC11xx) || \
    defined(CPU_LPC1114FBD48) || defined(CPU_LPC1114FHN33) || \
    defined(CPU_LPC1115FBD48)
#define FL_IS_ARM_LPC_11_LEGACY
#endif

// Roll-up
#if defined(FL_IS_ARM_LPC_11_USB) || defined(FL_IS_ARM_LPC_11_LEGACY)
#define FL_IS_ARM_LPC_11
#endif

// LPC15xx family (ARM Cortex-M3, up to 72 MHz, SCT/PLU on some variants:
// LPC1517, LPC1518, LPC1519, LPC1547, LPC1548, LPC1549). This is the
// family detection portion of the FastLED/FastLED#2845 Stage 4 LPC15xx
// port scaffold — driver wiring is a follow-up.
//
// IMPORTANT: LPC15xx is Cortex-M3, NOT M0 / M0+. The M3 has a barrel
// shifter and the full Thumb-2 instruction set, so the clockless driver
// can target tighter cycle counts than the M0 / M0+ ASM template
// permits. Do not gate this family on FL_IS_ARM_M0 or FL_IS_ARM_M0_PLUS
// — the M0 template's load/store offset encoding restrictions do not
// apply. The follow-up driver will key on FL_IS_ARM_LPC + an M3-class
// path. The shared M3 clockless template (src/platforms/arm/common/
// m3_clockless_asm.h if present, or a new file alongside it) is the
// right entry point.
#if defined(__LPC15xx__) || defined(LPC15xx) || \
    defined(CPU_LPC1517JBD48) || defined(CPU_LPC1517JBD64) || \
    defined(CPU_LPC1518JBD48) || defined(CPU_LPC1518JBD64) || \
    defined(CPU_LPC1518JBD100) || \
    defined(CPU_LPC1519JBD48) || defined(CPU_LPC1519JBD64) || \
    defined(CPU_LPC1519JBD100) || \
    defined(CPU_LPC1547JBD48) || defined(CPU_LPC1547JBD64) || \
    defined(CPU_LPC1547JBD100) || \
    defined(CPU_LPC1548JBD48) || defined(CPU_LPC1548JBD64) || \
    defined(CPU_LPC1548JBD100) || \
    defined(CPU_LPC1549JBD48) || defined(CPU_LPC1549JBD64) || \
    defined(CPU_LPC1549JBD100)
#define FL_IS_ARM_LPC_15
#endif

// General LPC family roll-up (any LPC variant supported by this port)
#if defined(FL_IS_ARM_LPC_845) || defined(FL_IS_ARM_LPC_804) || \
    defined(FL_IS_ARM_LPC_11) || defined(FL_IS_ARM_LPC_15)
#define FL_IS_ARM_LPC
#endif
