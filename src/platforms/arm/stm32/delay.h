#pragma once

#ifndef __INC_FASTLED_PLATFORMS_STM32_DELAY_H
#define __INC_FASTLED_PLATFORMS_STM32_DELAY_H

#include "fl/types.h"
#include "fl/force_inline.h"

/// @file platforms/arm/stm32/delay.h
/// ARM Cortex-M3/M4 (STM32) platform-specific nanosecond-precision delay utilities

/// ARM Cortex-M3/M4 (STM32, Teensy 3.x, etc.): Use DWT cycle counter
constexpr fl::u32 ARM_DEMCR_ADDR = 0xE000EDFC;
constexpr fl::u32 ARM_DWT_CTRL_ADDR = 0xE0001000;
constexpr fl::u32 ARM_DWT_CYCCNT_ADDR = 0xE0001004;

FASTLED_FORCE_INLINE void dwt_enable_cycle_counter() {
  volatile fl::u32* demcr = (volatile fl::u32*)ARM_DEMCR_ADDR;
  volatile fl::u32* dwt_ctrl = (volatile fl::u32*)ARM_DWT_CTRL_ADDR;

  *demcr |= (1u << 24);  // DEMCR.TRCENA
  *dwt_ctrl |= 1u;       // DWT.CYCCNTENA
}

FASTLED_FORCE_INLINE fl::u32 dwt_cyccnt_arm() {
  volatile fl::u32* cyccnt = (volatile fl::u32*)ARM_DWT_CYCCNT_ADDR;
  return *cyccnt;
}

FASTLED_FORCE_INLINE void delay_cycles_dwt_arm(fl::u32 cycles) {
  if (cycles == 0) return;
  fl::u32 start = dwt_cyccnt_arm();
  while ((fl::u32)(dwt_cyccnt_arm() - start) < cycles) { }
}

namespace fl {

/// Convert nanoseconds to CPU cycles
/// @param ns Number of nanoseconds
/// @param hz CPU frequency in Hz
/// @return Number of cycles (rounded up)
constexpr fl::u32 cycles_from_ns_stm32(fl::u32 ns, fl::u32 hz) {
  // Round up: cycles = ceil(ns * hz / 1e9)
  // Using: (ns * hz + 999'999'999) / 1'000'000'000
  return ((fl::u64)ns * (fl::u64)hz + 999999999UL) / 1000000000UL;
}

/// Platform-specific implementation of nanosecond delay with runtime frequency (STM32)
/// @param ns Number of nanoseconds
/// @param hz CPU frequency in Hz
FASTLED_FORCE_INLINE void delayNanoseconds_impl(fl::u32 ns, fl::u32 hz) {
  fl::u32 cycles = cycles_from_ns_stm32(ns, hz);
  if (cycles == 0) return;
  delay_cycles_dwt_arm(cycles);
}

/// Platform-specific implementation of nanosecond delay with auto-detected frequency (STM32)
/// @param ns Number of nanoseconds
FASTLED_FORCE_INLINE void delayNanoseconds_impl(fl::u32 ns) {
  #if defined(F_CPU)
  fl::u32 hz = F_CPU;
  #else
  fl::u32 hz = 16000000UL;  // Default fallback
  #endif
  delayNanoseconds_impl(ns, hz);
}

}  // namespace fl

#endif // __INC_FASTLED_PLATFORMS_STM32_DELAY_H
