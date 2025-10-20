#pragma once

#ifndef __INC_FL_DELAY_H
#define __INC_FL_DELAY_H

/// @file fl/delay.h
/// Delay utilities for FastLED
/// Includes nanosecond-precision delays, cycle counting, and microsecond delays

#include "fl/types.h"
#include "fl/force_inline.h"
#include "fl/compiler_control.h"

// ============================================================================
// Platform-specific includes (must be outside detail namespace)
// ============================================================================

#include "platforms/delay.h"

namespace fl {

// ============================================================================
// Forward declarations
// ============================================================================

#if defined(ESP32)
/// Get the current ESP32 CPU frequency at runtime
u32 esp_clk_cpu_freq_impl();
#endif

namespace detail {

/// Convert nanoseconds to CPU cycles
/// @param ns Number of nanoseconds
/// @param hz CPU frequency in Hz
/// @return Number of cycles (rounded up)
constexpr fl::u32 cycles_from_ns(fl::u32 ns, fl::u32 hz) {
  // Round up: cycles = ceil(ns * hz / 1e9)
  // Using: (ns * hz + 999'999'999) / 1'000'000'000
  return ((fl::u64)ns * (fl::u64)hz + 999999999UL) / 1000000000UL;
}

} // namespace detail

// ============================================================================
// Public API: delayNanoseconds
// ============================================================================

// Helper function to get compile-time CPU frequency
constexpr fl::u32 get_cpu_frequency() {
#if defined(F_CPU)
  return F_CPU;
#elif defined(CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ)
  // ESP-IDF style (can be tuned via menuconfig)
  return (fl::u32)CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ * 1000000UL;
#elif defined(ESP32)
  // Fallback for ESP32/ESP32-C3/ESP32-C6: assume 160 MHz (conservative default)
  return 160000000UL;
#elif defined(ARDUINO_ARCH_RP2040)
  // RP2040: standard 125 MHz
  return 125000000UL;
#elif defined(NRF52_SERIES)
  // nRF52: standard 64 MHz
  return 64000000UL;
#elif defined(ARDUINO_ARCH_SAMD)
  // SAMD21: default 48 MHz, SAMD51: 120 MHz (use conservative default)
  return 48000000UL;
#else
  // Fallback: assume 16 MHz (common Arduino)
  return 16000000UL;
#endif
}

/// Delay for a compile-time constant number of nanoseconds
/// @tparam NS Number of nanoseconds (at compile-time)
template<fl::u32 NS>
FASTLED_FORCE_INLINE void delayNanoseconds() {
  constexpr fl::u32 HZ = get_cpu_frequency();
  constexpr fl::u32 cycles = detail::cycles_from_ns(NS, HZ);

  if (cycles == 0) {
    return;
  }

  // Platform-specific implementation
#if defined(ARDUINO_ARCH_AVR)
  delay_cycles_avr_nop(cycles);
#elif defined(ESP32) && !defined(ESP32C3) && !defined(ESP32C6)
  delay_cycles_ccount(cycles);
#elif defined(ESP32C3) || defined(ESP32C6)
  delay_cycles_mcycle(cycles);
#elif defined(ARDUINO_ARCH_RP2040)
  delay_cycles_pico(cycles);
#elif defined(NRF52_SERIES)
  delay_cycles_dwt(cycles);
#elif defined(ARDUINO_ARCH_SAMD)
  delay_cycles_dwt_samd(cycles);
#elif defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)
  delay_cycles_dwt_arm(cycles);
#else
  delay_cycles_generic(cycles);
#endif
}

/// Delay for a runtime number of nanoseconds
/// @param ns Number of nanoseconds (runtime value)
void delayNanoseconds(fl::u32 ns);

/// Delay for a runtime number of nanoseconds with explicit clock frequency
/// @param ns Number of nanoseconds
/// @param hz CPU frequency in Hz
void delayNanoseconds(fl::u32 ns, fl::u32 hz);

// ============================================================================
// Clock cycle-counted delay loop (delaycycles)
// ============================================================================

/// Forward declaration of delaycycles template
/// @tparam CYCLES the number of clock cycles to delay
/// @note No delay is applied if CYCLES is less than or equal to zero.
/// Specializations for small cycle counts and platform-specific optimizations
/// are defined in delay.cpp
template<cycle_t CYCLES> void delaycycles();

/// A variant of delaycycles that will always delay at least one cycle
/// @tparam CYCLES the number of clock cycles to delay
template<cycle_t CYCLES> inline void delaycycles_min1() {
  delaycycles<1>();
  delaycycles<CYCLES - 1>();
}

// ============================================================================
// Millisecond and Microsecond delay wrappers
// ============================================================================

/// Delay for a given number of milliseconds
/// @param ms Milliseconds to delay
void delay(u32 ms);

/// Delay for a given number of microseconds
/// @param us Microseconds to delay
void delayMicroseconds(u32 us);

/// Alias for delayMicroseconds (shorter name for convenience)
/// @param us Microseconds to delay
inline void delayMicros(u32 us) {
  delayMicroseconds(us);
}

}  // namespace fl

#endif // __INC_FL_DELAY_H
