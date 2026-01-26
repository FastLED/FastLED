#pragma once

#ifndef __INC_FASTLED_PLATFORMS_STUB_DELAY_H
#define __INC_FASTLED_PLATFORMS_STUB_DELAY_H

#include "platforms/cycle_type.h"
#include "fl/force_inline.h"

/// @file platforms/stub/delay.h
/// Stub platform-specific nanosecond-precision delay utilities
/// Uses generic delay_cycles for cycle-accurate delays

/// Convert nanoseconds to CPU cycles for stub platform
/// @param ns Number of nanoseconds
/// @param cpu_hz CPU frequency in Hz
/// @return Number of cycles (rounded up)
constexpr fl::u32 cycles_from_ns_stub(fl::u32 ns, fl::u32 cpu_hz) {
  // Round up: cycles = ceil(ns * cpu_hz / 1e9)
  // = (ns * cpu_hz + 999'999'999) / 1'000'000'000
  return ((fl::u64)ns * (fl::u64)cpu_hz + 999999999UL) / 1000000000UL;
}

namespace fl {

/// Platform-specific implementation of nanosecond delay with runtime frequency (stub)
/// @param ns Number of nanoseconds
/// @param hz CPU frequency in Hz
FASTLED_FORCE_INLINE void delayNanoseconds_impl(u32 ns, u32 hz) {
  u32 cycles = cycles_from_ns_stub(ns, hz);
  if (cycles == 0) return;
  delay_cycles_generic(cycles);
}

/// Platform-specific implementation of nanosecond delay with auto-detected frequency (stub)
/// @param ns Number of nanoseconds
FASTLED_FORCE_INLINE void delayNanoseconds_impl(u32 ns) {
  #if defined(F_CPU)
  u32 hz = F_CPU;
  #else
  u32 hz = 16000000UL;  // Common Arduino default
  #endif
  delayNanoseconds_impl(ns, hz);
}

}  // namespace fl

#endif // __INC_FASTLED_PLATFORMS_STUB_DELAY_H
