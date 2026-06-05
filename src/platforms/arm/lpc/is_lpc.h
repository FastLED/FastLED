#pragma once

// IWYU pragma: private

// ok no namespace fl

/// @file is_lpc.h
/// @brief NXP LPC8xx platform detection macros for FastLED
///
/// Detects NXP LPC845 and LPC804 (ARM Cortex-M0+) platforms. Detection uses
/// the MCUXpresso / CMSIS device-header defines (e.g. CPU_LPC845M301JBD48,
/// LPC845, LPC804) as well as bare project-level defines that fbuild/PIO
/// configurations are expected to provide (__LPC845__, __LPC804__).

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

// General LPC8xx platform (any LPC8xx variant supported by this port)
#if defined(FL_LPC845) || defined(FL_LPC804)
#define FL_IS_ARM_LPC
#endif
