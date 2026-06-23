#pragma once

// IWYU pragma: private

#ifndef __INC_FASTLED_PLATFORMS_AVR_DELAY_H
#define __INC_FASTLED_PLATFORMS_AVR_DELAY_H

#include "platforms/cycle_type.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"

/// @file platforms/avr/delay.h
/// AVR platform-specific nanosecond-precision delay utilities

/// Convert nanoseconds to CPU cycles for AVR
/// This is a compile-time calculation approach
constexpr fl::u32 cycles_from_ns_avr(fl::u32 ns, fl::u32 cpu_hz) FL_NO_EXCEPT {
  // Round up: cycles = ceil(ns * cpu_hz / 1e9)
  // = (ns * cpu_hz + 999'999'999) / 1'000'000'000
  return ((fl::u64)ns * (fl::u64)cpu_hz + 999999999UL) / 1000000000UL;
}

/// Simple NOP-based delay for very small counts
inline void delay_cycles_avr_nop(fl::u32 cycles) FL_NO_EXCEPT {
  // Unroll small counts as inline NOPs to avoid function call overhead
  while (cycles > 0) {
    __asm__ __volatile__("nop\n") FL_NO_EXCEPT;
    cycles--;
  }
}

namespace fl {

/// Platform-specific implementation of nanosecond delay with runtime frequency (AVR)
/// @param ns Number of nanoseconds
/// @param hz CPU frequency in Hz
FASTLED_FORCE_INLINE void delayNanoseconds_impl(u32 ns, u32 hz) FL_NO_EXCEPT {
  u32 cycles = cycles_from_ns_avr(ns, hz);
  if (cycles == 0) return;
  delay_cycles_avr_nop(cycles);
}

/// Platform-specific implementation of nanosecond delay with auto-detected frequency (AVR)
/// @param ns Number of nanoseconds
FASTLED_FORCE_INLINE void delayNanoseconds_impl(u32 ns) FL_NO_EXCEPT {
  #if defined(F_CPU)
  u32 hz = F_CPU;
  #else
  u32 hz = 16000000UL;  // Default to 16 MHz
  #endif
  delayNanoseconds_impl(ns, hz);
}

}  // namespace fl

#endif // __INC_FASTLED_PLATFORMS_AVR_DELAY_H
