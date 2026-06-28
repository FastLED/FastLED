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
inline void delaycycles() FL_NO_EXCEPT;

// Base case specializations for non-positive cycles (to prevent infinite recursion)
// These provide the minimum base cases to stop the recursive template from infinitely recursing.
// All specializations (positive and non-positive cycles) are provided in fl/delay.cpp and linked at compile time.
// Use #ifndef guard to allow delay.cpp to override these with its definitions
//
// CRITICAL: the positive base cases <1> and <2> below are what make the
// binary-split recursion terminate. The split is delaycycles<N>() ->
// delaycycles<N/2>() + delaycycles<N - N/2>(). For every N >= 2 both children
// are strictly smaller than N, so the recursion descends. But at N == 1 the
// split degenerates to <0> + <1> (HALF=0, REMAINDER=1) and re-instantiates <1>
// FOREVER. The positive specializations that would stop this live in another
// translation unit (fl/system/delay.cpp.hpp), so any TU that includes only
// these headers -- e.g. a clockless/SPI driver calling delaycycles<N>() -- has
// no <1> base, recurses without bound, and OOMs the compiler (and the
// cpptools/EDG IntelliSense engine, which instantiates the primary template
// speculatively). Defining <1> (and <2>) here gives the split a floor in EVERY
// TU. Guarded by FL_DELAY_CPP_SPECIALIZATIONS so delay.cpp.hpp still provides
// its own out-of-line copies (it #defines that before including this header).
#ifndef FL_DELAY_CPP_SPECIALIZATIONS
template<> FASTLED_FORCE_INLINE void delaycycles<-10>() FL_NO_EXCEPT {}
template<> FASTLED_FORCE_INLINE void delaycycles<-9>() FL_NO_EXCEPT {}
template<> FASTLED_FORCE_INLINE void delaycycles<-8>() FL_NO_EXCEPT {}
template<> FASTLED_FORCE_INLINE void delaycycles<-7>() FL_NO_EXCEPT {}
template<> FASTLED_FORCE_INLINE void delaycycles<-6>() FL_NO_EXCEPT {}
template<> FASTLED_FORCE_INLINE void delaycycles<-5>() FL_NO_EXCEPT {}
template<> FASTLED_FORCE_INLINE void delaycycles<-4>() FL_NO_EXCEPT {}
template<> FASTLED_FORCE_INLINE void delaycycles<-3>() FL_NO_EXCEPT {}
template<> FASTLED_FORCE_INLINE void delaycycles<-2>() FL_NO_EXCEPT {}
template<> FASTLED_FORCE_INLINE void delaycycles<-1>() FL_NO_EXCEPT {}
template<> FASTLED_FORCE_INLINE void delaycycles<0>() FL_NO_EXCEPT {}
template<> FASTLED_FORCE_INLINE void delaycycles<1>() FL_NO_EXCEPT { FL_NOP; }
template<> FASTLED_FORCE_INLINE void delaycycles<2>() FL_NO_EXCEPT { FL_NOP2; }
#endif

/// Delay for N clock cycles (generic NOP-based implementation)
/// Uses binary splitting to minimize template instantiation depth
/// Recursively splits the delay into two halves:
/// delaycycles<N>() → delaycycles<N/2>() + delaycycles<N - N/2>()
/// Terminates at the <0>/<1>/<2> base cases above (present in every TU); larger
/// cases (3-50) also have out-of-line specializations in fl/system/delay.cpp.hpp.
template<fl::cycle_t CYCLES>
FASTLED_FORCE_INLINE void delaycycles() FL_NO_EXCEPT {
  constexpr fl::cycle_t HALF = CYCLES / 2;
  constexpr fl::cycle_t REMAINDER = CYCLES - HALF;
  delaycycles<HALF>();
  delaycycles<REMAINDER>();
}

// }  // namespace fl (closed by including file)

#endif  // __INC_FASTLED_PLATFORMS_DELAY_CYCLES_GENERIC_H
