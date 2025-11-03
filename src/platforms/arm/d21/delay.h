#pragma once

#ifndef __INC_FASTLED_PLATFORMS_SAMD_DELAY_H
#define __INC_FASTLED_PLATFORMS_SAMD_DELAY_H

#include "platforms/cycle_type.h"
#include "fl/force_inline.h"

/// @file platforms/arm/d21/delay.h
/// SAMD platform-specific nanosecond-precision delay utilities
/// Used by both SAMD21 (d21) and SAMD51 (d51)

namespace fl {

/// Convert nanoseconds to CPU cycles
/// @param ns Number of nanoseconds
/// @param hz CPU frequency in Hz
/// @return Number of cycles (rounded up)
constexpr u32 cycles_from_ns_samd(u32 ns, u32 hz) {
  // Round up: cycles = ceil(ns * hz / 1e9)
  // Using: (ns * hz + 999'999'999) / 1'000'000'000
  return ((u64)ns * (u64)hz + 999999999UL) / 1000000000UL;
}

/// Platform-specific implementation of nanosecond delay with runtime frequency (SAMD)
/// @param ns Number of nanoseconds
/// @param hz CPU frequency in Hz
FASTLED_FORCE_INLINE void delayNanoseconds_impl(u32 ns, u32 hz) {
  u32 cycles = cycles_from_ns_samd(ns, hz);
  if (cycles == 0) return;
  delay_cycles_dwt_samd(cycles);
}

/// Platform-specific implementation of nanosecond delay with auto-detected frequency (SAMD)
/// @param ns Number of nanoseconds
FASTLED_FORCE_INLINE void delayNanoseconds_impl(u32 ns) {
  #if defined(F_CPU)
  u32 hz = F_CPU;
  #else
  u32 hz = 48000000UL;  // SAMD21 default
  #endif
  delayNanoseconds_impl(ns, hz);
}

}  // namespace fl

#endif // __INC_FASTLED_PLATFORMS_SAMD_DELAY_H
