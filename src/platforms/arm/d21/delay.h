#pragma once

#ifndef __INC_FASTLED_PLATFORMS_SAMD_DELAY_H
#define __INC_FASTLED_PLATFORMS_SAMD_DELAY_H

#include "fl/types.h"
#include "fl/force_inline.h"

/// @file platforms/arm/d21/delay.h
/// SAMD platform-specific nanosecond-precision delay utilities
/// Used by both SAMD21 (d21) and SAMD51 (d51)

/// SAMD (Cortex-M0+): Use DWT cycle counter (similar to nRF52)
constexpr fl::u32 SAMD_DEMCR_ADDR = 0xE000EDFC;
constexpr fl::u32 SAMD_DWT_CTRL_ADDR = 0xE0001000;
constexpr fl::u32 SAMD_DWT_CYCCNT_ADDR = 0xE0001004;

FASTLED_FORCE_INLINE void dwt_enable_cycle_counter() {
  volatile fl::u32* demcr = (volatile fl::u32*)SAMD_DEMCR_ADDR;
  volatile fl::u32* dwt_ctrl = (volatile fl::u32*)SAMD_DWT_CTRL_ADDR;

  *demcr |= (1u << 24);
  *dwt_ctrl |= 1u;
}

FASTLED_FORCE_INLINE fl::u32 dwt_cyccnt() {
  volatile fl::u32* cyccnt = (volatile fl::u32*)SAMD_DWT_CYCCNT_ADDR;
  return *cyccnt;
}

FASTLED_FORCE_INLINE void delay_cycles_dwt_samd(fl::u32 cycles) {
  if (cycles == 0) return;
  fl::u32 start = dwt_cyccnt();
  while ((fl::u32)(dwt_cyccnt() - start) < cycles) { }
}

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
