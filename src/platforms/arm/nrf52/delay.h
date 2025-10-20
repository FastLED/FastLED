#pragma once

#ifndef __INC_FASTLED_PLATFORMS_NRF52_DELAY_H
#define __INC_FASTLED_PLATFORMS_NRF52_DELAY_H

#include "fl/types.h"
#include "fl/force_inline.h"

/// @file platforms/arm/nrf52/delay.h
/// nRF52 platform-specific nanosecond-precision delay utilities

/// nRF52 (Cortex-M4): Use DWT cycle counter
/// Requires enabling DWT first (done once at init)
constexpr fl::u32 DEMCR_ADDR = 0xE000EDFC;
constexpr fl::u32 DWT_CTRL_ADDR = 0xE0001000;
constexpr fl::u32 DWT_CYCCNT_ADDR = 0xE0001004;

FASTLED_FORCE_INLINE void dwt_enable_cycle_counter() {
  // Enable trace and DWT
  volatile fl::u32* demcr = (volatile fl::u32*)DEMCR_ADDR;
  volatile fl::u32* dwt_ctrl = (volatile fl::u32*)DWT_CTRL_ADDR;

  *demcr |= (1u << 24);  // DEMCR.TRCENA = 1
  *dwt_ctrl |= 1u;       // DWT.CYCCNTENA = 1
}

FASTLED_FORCE_INLINE fl::u32 dwt_cyccnt() {
  volatile fl::u32* cyccnt = (volatile fl::u32*)DWT_CYCCNT_ADDR;
  return *cyccnt;
}

FASTLED_FORCE_INLINE void delay_cycles_dwt(fl::u32 cycles) {
  if (cycles == 0) return;
  fl::u32 start = dwt_cyccnt();
  while ((fl::u32)(dwt_cyccnt() - start) < cycles) { }
}

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
