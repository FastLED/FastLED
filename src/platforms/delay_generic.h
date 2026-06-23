#pragma once

#ifndef __INC_FASTLED_PLATFORMS_DELAY_GENERIC_H
#define __INC_FASTLED_PLATFORMS_DELAY_GENERIC_H

#include "platforms/cycle_type.h"
#include "fl/stl/compiler_control.h"
#include "platforms/delaycycles_generic.h"
#include "fl/stl/noexcept.h"

/// @file platforms/delay_generic.h
/// Generic fallback nanosecond-precision delay utilities for unsupported platforms

namespace fl {

/// Convert nanoseconds to CPU cycles
/// @param ns Number of nanoseconds
/// @param hz CPU frequency in Hz
/// @return Number of cycles (rounded up)
constexpr u32 cycles_from_ns_generic(u32 ns, u32 hz) FL_NOEXCEPT {
  // Round up: cycles = ceil(ns * hz / 1e9)
  // Using: (ns * hz + 999'999'999) / 1'000'000'000
  return ((u64)ns * (u64)hz + 999999999UL) / 1000000000UL;
}

/// Platform-specific implementation of nanosecond delay with runtime frequency (generic)
/// @param ns Number of nanoseconds
/// @param hz CPU frequency in Hz
FASTLED_FORCE_INLINE void delayNanoseconds_impl(u32 ns, u32 hz) FL_NOEXCEPT {
  u32 cycles = cycles_from_ns_generic(ns, hz);
  if (cycles == 0) return;
  delay_cycles_generic(cycles);
}

/// Platform-specific implementation of nanosecond delay with auto-detected frequency (generic)
/// @param ns Number of nanoseconds
FASTLED_FORCE_INLINE void delayNanoseconds_impl(u32 ns) FL_NOEXCEPT {
  #if defined(F_CPU)
  u32 hz = F_CPU;
  #else
  u32 hz = 16000000UL;  // Common Arduino default
  #endif
  delayNanoseconds_impl(ns, hz);
}

}  // namespace fl

#endif // __INC_FASTLED_PLATFORMS_DELAY_GENERIC_H
