#pragma once

#ifndef __INC_FASTLED_PLATFORMS_ESP32_DELAY_H
#define __INC_FASTLED_PLATFORMS_ESP32_DELAY_H

#include "platforms/cycle_type.h"
#include "fl/force_inline.h"

/// @file platforms/esp/32/delay.h
/// ESP32 (Xtensa) platform-specific nanosecond-precision delay utilities

namespace fl {

/// Convert nanoseconds to CPU cycles
/// @param ns Number of nanoseconds
/// @param hz CPU frequency in Hz
/// @return Number of cycles (rounded up)
constexpr u32 cycles_from_ns_esp32(u32 ns, u32 hz) {
  // Round up: cycles = ceil(ns * hz / 1e9)
  // Using: (ns * hz + 999'999'999) / 1'000'000'000
  return ((u64)ns * (u64)hz + 999999999UL) / 1000000000UL;
}

/// Forward declaration for runtime frequency query
extern u32 esp_clk_cpu_freq_impl();

/// Platform-specific implementation of nanosecond delay with runtime frequency (ESP32)
/// @param ns Number of nanoseconds
/// @param hz CPU frequency in Hz
FASTLED_FORCE_INLINE void delayNanoseconds_impl(u32 ns, u32 hz) {
  u32 cycles = cycles_from_ns_esp32(ns, hz);
  if (cycles == 0) return;
  delay_cycles_ccount(cycles);
}

/// Platform-specific implementation of nanosecond delay with auto-detected frequency (ESP32)
/// @param ns Number of nanoseconds
FASTLED_FORCE_INLINE void delayNanoseconds_impl(u32 ns) {
  u32 hz = esp_clk_cpu_freq_impl();
  delayNanoseconds_impl(ns, hz);
}

}  // namespace fl

#endif // __INC_FASTLED_PLATFORMS_ESP32_DELAY_H
