#pragma once

#include "platforms/cycle_type.h"
#include "fl/force_inline.h"
#include "platforms/arm/stm32/core_detection.h"

/// @file platforms/arm/stm32/delay.h
/// ARM Cortex-M3/M4 (STM32) platform-specific nanosecond-precision delay utilities

namespace fl {

/// Convert nanoseconds to CPU cycles
/// @param ns Number of nanoseconds
/// @param hz CPU frequency in Hz
/// @return Number of cycles (rounded up)
constexpr u32 cycles_from_ns_stm32(u32 ns, u32 hz) {
  // Round up: cycles = ceil(ns * hz / 1e9)
  // Using: (ns * hz + 999'999'999) / 1'000'000'000
  return ((u64)ns * (u64)hz + 999999999UL) / 1000000000UL;
}

/// Platform-specific implementation of nanosecond delay with runtime frequency (STM32)
/// @param ns Number of nanoseconds
/// @param hz CPU frequency in Hz
FASTLED_FORCE_INLINE void delayNanoseconds_impl(u32 ns, u32 hz) {
  u32 cycles = cycles_from_ns_stm32(ns, hz);
  if (cycles == 0) return;
  delay_cycles_dwt_arm(cycles);
}

/// Platform-specific implementation of nanosecond delay with auto-detected frequency (STM32)
/// @param ns Number of nanoseconds
FASTLED_FORCE_INLINE void delayNanoseconds_impl(u32 ns) {
  // F_CPU is runtime SystemCoreClock on STM32duino, compile-time constant elsewhere
  u32 hz = F_CPU;
  delayNanoseconds_impl(ns, hz);
}

}  // namespace fl
