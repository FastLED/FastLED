#pragma once

#ifndef __INC_FASTLED_PLATFORMS_NRF52_DELAY_H
#define __INC_FASTLED_PLATFORMS_NRF52_DELAY_H

#include "platforms/cycle_type.h"
#include "fl/force_inline.h"

/// @file platforms/arm/nrf52/delay.h
/// nRF52 platform-specific nanosecond-precision delay utilities

namespace fl {

/// Convert nanoseconds to CPU cycles
/// @param ns Number of nanoseconds
/// @param hz CPU frequency in Hz
/// @return Number of cycles (rounded up)
constexpr u32 cycles_from_ns_nrf52(u32 ns, u32 hz) {
  // Round up: cycles = ceil(ns * hz / 1e9)
  // Using: (ns * hz + 999'999'999) / 1'000'000'000
  return ((u64)ns * (u64)hz + 999999999UL) / 1000000000UL;
}

/// Platform-specific implementation of nanosecond delay with runtime frequency (nRF52)
/// @param ns Number of nanoseconds
/// @param hz CPU frequency in Hz
FASTLED_FORCE_INLINE void delayNanoseconds_impl(u32 ns, u32 hz) {
  u32 cycles = cycles_from_ns_nrf52(ns, hz);
  if (cycles == 0) return;
  delay_cycles_dwt(cycles);
}

/// Platform-specific implementation of nanosecond delay with auto-detected frequency (nRF52)
/// @param ns Number of nanoseconds
FASTLED_FORCE_INLINE void delayNanoseconds_impl(u32 ns) {
  // nRF52 typically runs at 64 MHz
  constexpr u32 hz = 64000000UL;
  delayNanoseconds_impl(ns, hz);
}

}  // namespace fl

#endif // __INC_FASTLED_PLATFORMS_NRF52_DELAY_H
