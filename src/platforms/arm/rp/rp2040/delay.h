#pragma once

#ifndef __INC_FASTLED_PLATFORMS_RP2040_DELAY_H
#define __INC_FASTLED_PLATFORMS_RP2040_DELAY_H

#include "platforms/cycle_type.h"
#include "fl/force_inline.h"

/// @file platforms/arm/rp/rp2040/delay.h
/// RP2040 platform-specific nanosecond-precision delay utilities

namespace fl {

/// Convert nanoseconds to CPU cycles
/// @param ns Number of nanoseconds
/// @param hz CPU frequency in Hz
/// @return Number of cycles (rounded up)
constexpr u32 cycles_from_ns_pico(u32 ns, u32 hz) {
  // Round up: cycles = ceil(ns * hz / 1e9)
  // Using: (ns * hz + 999'999'999) / 1'000'000'000
  return ((u64)ns * (u64)hz + 999999999UL) / 1000000000UL;
}

/// Platform-specific implementation of nanosecond delay with runtime frequency (RP2040)
/// @param ns Number of nanoseconds
/// @param hz CPU frequency in Hz
FASTLED_FORCE_INLINE void delayNanoseconds_impl(u32 ns, u32 hz) {
  u32 cycles = cycles_from_ns_pico(ns, hz);
  if (cycles == 0) return;
  delay_cycles_pico(cycles);
}

/// Platform-specific implementation of nanosecond delay with auto-detected frequency (RP2040)
/// @param ns Number of nanoseconds
FASTLED_FORCE_INLINE void delayNanoseconds_impl(u32 ns) {
  // RP2040 clock is fixed at 125 MHz in normal mode
  constexpr u32 hz = 125000000UL;
  delayNanoseconds_impl(ns, hz);
}

}  // namespace fl

#endif // __INC_FASTLED_PLATFORMS_RP2040_DELAY_H
