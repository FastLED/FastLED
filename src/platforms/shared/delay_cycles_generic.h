#pragma once

#ifndef __INC_FASTLED_PLATFORMS_DELAY_CYCLES_GENERIC_H
#define __INC_FASTLED_PLATFORMS_DELAY_CYCLES_GENERIC_H

#include "fl/types.h"
#include "fl/force_inline.h"

/// @file platforms/delay_cycles_generic.h
/// Generic fallback cycle-accurate delay utilities for non-AVR platforms

// Note: This file is intended to be included inside namespace fl
// namespace fl {

// ============================================================================
// NOP macros for non-AVR platforms
// ============================================================================

/// Single no-operation instruction for delay
#define FL_NOP __asm__ __volatile__("nop\n");
/// Double no-operation instruction for delay
#define FL_NOP2 __asm__ __volatile__("nop\n\t nop\n");

// ============================================================================
// Generic delaycycles implementation
// ============================================================================

/// Forward declaration
template<fl::cycle_t CYCLES>
inline void delaycycles();

// Base case specializations for non-positive cycles (to prevent infinite recursion)
// These provide the minimum base cases to stop the recursive template from infinitely recursing.
// All specializations (positive and non-positive cycles) are provided in fl/delay.cpp and linked at compile time.
// Use #ifndef guard to allow delay.cpp to override these with its definitions
#ifndef FL_DELAY_CPP_SPECIALIZATIONS
template<> FASTLED_FORCE_INLINE void delaycycles<-10>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-9>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-8>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-7>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-6>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-5>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-4>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-3>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-2>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-1>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<0>() {}
#endif

/// Delay for N clock cycles (generic NOP-based implementation)
/// Uses NOP instructions to create minimal delays
/// Note: This is a naive implementation using NOPs
/// Platform-specific versions can provide more efficient implementations
template<fl::cycle_t CYCLES>
FASTLED_FORCE_INLINE void delaycycles() {
  FL_NOP;
  delaycycles<CYCLES - 1>();
}

// }  // namespace fl (closed by including file)

#endif  // __INC_FASTLED_PLATFORMS_DELAY_CYCLES_GENERIC_H
