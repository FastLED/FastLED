#pragma once

// IWYU pragma: private

#ifndef __INC_FASTLED_PLATFORMS_DELAY_CYCLES_GENERIC_H
#define __INC_FASTLED_PLATFORMS_DELAY_CYCLES_GENERIC_H

#include "platforms/cycle_type.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"

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
inline void delaycycles() FL_NOEXCEPT;

// Base case specializations for non-positive cycles (to prevent infinite recursion)
// These provide the minimum base cases to stop the recursive template from infinitely recursing.
// All specializations (positive and non-positive cycles) are provided in fl/delay.cpp and linked at compile time.
// Use #ifndef guard to allow delay.cpp to override these with its definitions
#ifndef FL_DELAY_CPP_SPECIALIZATIONS
template<> FASTLED_FORCE_INLINE void delaycycles<-10>() FL_NOEXCEPT {}
template<> FASTLED_FORCE_INLINE void delaycycles<-9>() FL_NOEXCEPT {}
template<> FASTLED_FORCE_INLINE void delaycycles<-8>() FL_NOEXCEPT {}
template<> FASTLED_FORCE_INLINE void delaycycles<-7>() FL_NOEXCEPT {}
template<> FASTLED_FORCE_INLINE void delaycycles<-6>() FL_NOEXCEPT {}
template<> FASTLED_FORCE_INLINE void delaycycles<-5>() FL_NOEXCEPT {}
template<> FASTLED_FORCE_INLINE void delaycycles<-4>() FL_NOEXCEPT {}
template<> FASTLED_FORCE_INLINE void delaycycles<-3>() FL_NOEXCEPT {}
template<> FASTLED_FORCE_INLINE void delaycycles<-2>() FL_NOEXCEPT {}
template<> FASTLED_FORCE_INLINE void delaycycles<-1>() FL_NOEXCEPT {}
template<> FASTLED_FORCE_INLINE void delaycycles<0>() FL_NOEXCEPT {}
#endif

/// Delay for N clock cycles (generic NOP-based implementation)
/// Uses binary splitting to minimize template instantiation depth
/// Recursively splits the delay into two halves:
/// delaycycles<N>() → delaycycles<N/2>() + delaycycles<N - N/2>()
/// Base cases (0-50) are specialized in fl/delay.cpp for efficiency
template<fl::cycle_t CYCLES>
FASTLED_FORCE_INLINE void delaycycles() FL_NOEXCEPT {
  constexpr fl::cycle_t HALF = CYCLES / 2;
  constexpr fl::cycle_t REMAINDER = CYCLES - HALF;
  delaycycles<HALF>();
  delaycycles<REMAINDER>();
}

// }  // namespace fl (closed by including file)

#endif  // __INC_FASTLED_PLATFORMS_DELAY_CYCLES_GENERIC_H
