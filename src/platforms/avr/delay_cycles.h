#pragma once

#ifndef __INC_FASTLED_PLATFORMS_AVR_DELAY_CYCLES_H
#define __INC_FASTLED_PLATFORMS_AVR_DELAY_CYCLES_H

#include "platforms/cycle_type.h"
#include "fl/force_inline.h"

/// @file platforms/avr/delay_cycles.h
/// AVR platform-specific cycle-accurate delay utilities

// Note: This file is intended to be included inside namespace fl
// namespace fl {

// ============================================================================
// NOP macro for AVR
// ============================================================================

/// Single no-operation instruction for delay (AVR version)
#define FL_NOP __asm__ __volatile__("cp r0,r0\n");
/// Double no-operation instruction for delay (AVR version)
#define FL_NOP2 __asm__ __volatile__("rjmp .+0");

// ============================================================================
// AVR delaycycles implementation
// ============================================================================

/// Forward declaration
template<fl::cycle_t CYCLES>
inline void delaycycles();

/// AVR-specific worker template for cycle delays
/// This template uses a loop to generate the desired cycle count
/// @tparam LOOP number of loop iterations (each iteration is 3 cycles)
/// @tparam PAD additional padding cycles (0-2)
template<int LOOP, fl::cycle_t PAD>
inline void _delaycycles_avr() {
  delaycycles<PAD>();
  // The loop below generates 3 cycles * LOOP:
  // - LDI (1 cycle) loads the counter
  // - DEC (1 cycle) decrements the counter
  // - BRNE (2 cycles if looping, 1 cycle on exit)
  __asm__ __volatile__(
      "    LDI R16, %0\n"
      "L_%=: DEC R16\n"
      "    BRNE L_%=\n"
      : /* no outputs */
      : "M"(LOOP)
      : "r16");
}

/// Delay for N clock cycles (AVR implementation)
/// The base case specializations (0, 1, 2) are defined in fl/delay.cpp
/// to ensure they are compiled once and available to the linker.
template<fl::cycle_t CYCLES>
FASTLED_FORCE_INLINE void delaycycles() {
  _delaycycles_avr<CYCLES / 3, CYCLES % 3>();
}

// }  // namespace fl (closed by including file)

#endif  // __INC_FASTLED_PLATFORMS_AVR_DELAY_CYCLES_H
