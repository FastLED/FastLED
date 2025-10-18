#pragma once

#ifndef __INC_FASTLED_PLATFORMS_DELAY_GENERIC_H
#define __INC_FASTLED_PLATFORMS_DELAY_GENERIC_H

#include "fl/types.h"
#include "fl/force_inline.h"

/// @file platforms/delay_generic.h
/// Generic fallback nanosecond-precision delay utilities for unsupported platforms

/// Generic fallback: use tight NOP loop
FASTLED_FORCE_INLINE void delay_cycles_generic(fl::u32 cycles) {
  // Simple loop - not ideal but works on any platform
  while (cycles > 0) {
    __asm__ __volatile__("nop\n");
    cycles--;
  }
}

namespace fl {

/// Convert nanoseconds to CPU cycles
/// @param ns Number of nanoseconds
/// @param hz CPU frequency in Hz
/// @return Number of cycles (rounded up)
constexpr fl::u32 cycles_from_ns_generic(fl::u32 ns, fl::u32 hz) {
  // Round up: cycles = ceil(ns * hz / 1e9)
  // Using: (ns * hz + 999'999'999) / 1'000'000'000
  return ((fl::u64)ns * (fl::u64)hz + 999999999UL) / 1000000000UL;
}

/// Platform-specific implementation of nanosecond delay with runtime frequency (generic)
/// @param ns Number of nanoseconds
/// @param hz CPU frequency in Hz
FASTLED_FORCE_INLINE void delayNanoseconds_impl(fl::u32 ns, fl::u32 hz) {
  fl::u32 cycles = cycles_from_ns_generic(ns, hz);
  if (cycles == 0) return;
  delay_cycles_generic(cycles);
}

/// Platform-specific implementation of nanosecond delay with auto-detected frequency (generic)
/// @param ns Number of nanoseconds
FASTLED_FORCE_INLINE void delayNanoseconds_impl(fl::u32 ns) {
  #if defined(F_CPU)
  fl::u32 hz = F_CPU;
  #else
  fl::u32 hz = 16000000UL;  // Common Arduino default
  #endif
  delayNanoseconds_impl(ns, hz);
}

}  // namespace fl

#endif // __INC_FASTLED_PLATFORMS_DELAY_GENERIC_H
