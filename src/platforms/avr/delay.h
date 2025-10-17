#pragma once

#ifndef __INC_FASTLED_PLATFORMS_AVR_DELAY_H
#define __INC_FASTLED_PLATFORMS_AVR_DELAY_H

#include "fl/types.h"
#include "fl/force_inline.h"

/// @file platforms/avr/delay.h
/// AVR platform-specific nanosecond-precision delay utilities

/// Convert nanoseconds to CPU cycles for AVR
/// This is a compile-time calculation approach
constexpr fl::u32 cycles_from_ns_avr(fl::u32 ns, fl::u32 cpu_hz) {
  // Round up: cycles = ceil(ns * cpu_hz / 1e9)
  // = (ns * cpu_hz + 999'999'999) / 1'000'000'000
  return ((fl::u64)ns * (fl::u64)cpu_hz + 999999999UL) / 1000000000UL;
}

/// Simple NOP-based delay for very small counts
inline void delay_cycles_avr_nop(fl::u32 cycles) {
  // Unroll small counts as inline NOPs to avoid function call overhead
  while (cycles > 0) {
    __asm__ __volatile__("nop\n");
    cycles--;
  }
}

#endif // __INC_FASTLED_PLATFORMS_AVR_DELAY_H
