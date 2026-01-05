/*
  FastLED — RP Platform ISR Trampoline Header
  --------------------------------------------
  Dispatches to RP variant-specific ISR implementations (RP2040 or RP2350).

  This trampoline uses the FL_IS_RP2040 and FL_IS_RP2350 macros to select
  the appropriate implementation at compile time.

  License: MIT (FastLED)
*/

#pragma once

// ok no namespace fl

#include "platforms/is_platform.h"

// Dispatch to RP variant-specific implementation
#if defined(FL_IS_RP2350)
    #include "platforms/arm/rp/isr_rp2350.hpp"
#elif defined(FL_IS_RP2040)
    #include "platforms/arm/rp/isr_rp2040.hpp"
#else
    #error "RP ISR: Unknown RP variant - FL_IS_RP is defined but neither FL_IS_RP2040 nor FL_IS_RP2350"
#endif

// =============================================================================
// Global Interrupt Control (noInterrupts/interrupts) - All RP variants
// =============================================================================

#ifdef FL_IS_RP

namespace fl {

/// Disable interrupts on ARM Cortex-M (RP2040/RP2350)
inline void interruptsDisable() {
    __asm__ __volatile__("cpsid i" ::: "memory");
}

/// Enable interrupts on ARM Cortex-M (RP2040/RP2350)
inline void interruptsEnable() {
    __asm__ __volatile__("cpsie i" ::: "memory");
}

} // namespace fl

#endif // FL_IS_RP
