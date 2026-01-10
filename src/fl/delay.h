#pragma once

#ifndef __INC_FL_DELAY_H
#define __INC_FL_DELAY_H

/// @file fl/delay.h
/// Delay utilities for FastLED
/// Includes nanosecond-precision delays, cycle counting, and microsecond delays

#include "platforms/cycle_type.h"
#include "fl/force_inline.h"
#include "fl/int.h"

// ============================================================================
// Platform-specific includes via trampoline header
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

// ============================================================================
// CPU frequency macro (must be before detail namespace for use in constexpr)
// ============================================================================

// Helper macro to get compile-time CPU frequency
// Note: Uses preprocessor for compatibility with constexpr functions in C++11
#if defined(STM32F2XX)
  // STM32F2: 120 MHz
  #define GET_CPU_FREQUENCY() 120000000UL
#elif defined(STM32F4)
  // STM32F4: 100 MHz
  #define GET_CPU_FREQUENCY() 100000000UL
#elif defined(STM32F1) || defined(__STM32F1__) || defined(STM32F10X_MD)
  // STM32F1: 72 MHz
  #define GET_CPU_FREQUENCY() 72000000UL
#elif defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)
  // Other ARM Cortex-M3/M4: check F_CPU or use conservative 72 MHz default
  #if defined(F_CPU) && (F_CPU > 0)
    #define GET_CPU_FREQUENCY() F_CPU
  #else
    #define GET_CPU_FREQUENCY() 72000000UL
  #endif
#elif defined(F_CPU)
  #define GET_CPU_FREQUENCY() F_CPU
#elif defined(CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ)
  // ESP-IDF style (can be tuned via menuconfig)
  #define GET_CPU_FREQUENCY() ((fl::u32)CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ * 1000000UL)
#elif defined(ESP32)
  // Fallback for ESP32/ESP32-C3/ESP32-C6: assume 160 MHz (conservative default)
  #define GET_CPU_FREQUENCY() 160000000UL
#elif defined(ARDUINO_ARCH_RP2040)
  // RP2040: standard 125 MHz
  #define GET_CPU_FREQUENCY() 125000000UL
#elif defined(NRF52_SERIES)
  // nRF52: standard 64 MHz
  #define GET_CPU_FREQUENCY() 64000000UL
#elif defined(ARDUINO_ARCH_SAMD)
  // SAMD21: default 48 MHz, SAMD51: 120 MHz (use conservative default)
  #define GET_CPU_FREQUENCY() 48000000UL
#else
  // Fallback: assume 16 MHz (common Arduino)
  #define GET_CPU_FREQUENCY() 16000000UL
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

/// Compute cycles using default CPU frequency (compile-time)
/// @param ns Number of nanoseconds
/// @return Number of cycles using GET_CPU_FREQUENCY()
constexpr fl::u32 cycles_from_ns_default(fl::u32 ns) {
  return cycles_from_ns(ns, GET_CPU_FREQUENCY());
}

} // namespace detail

/// Delay for a compile-time constant number of nanoseconds
/// @tparam NS Number of nanoseconds (at compile-time)
template<fl::u32 NS>
FASTLED_FORCE_INLINE void delayNanoseconds() {
  // Compile-time calculation of required cycles using default CPU frequency
  constexpr fl::u32 cycles = detail::cycles_from_ns_default(NS);

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
void delayMillis(u32 ms);

/// Delay for a given number of microseconds
/// @param us Microseconds to delay
void delayMicroseconds(u32 us);

/// Alias for delayMicroseconds (shorter name for convenience)
/// @param us Microseconds to delay
inline void delayMicros(u32 us) {
  delayMicroseconds(us);
}

/// Shorter alias for delayMillis
/// @param ms Milliseconds to delay
inline void delayMs(u32 ms) {
  delayMillis(ms);
}

/// Shorter alias for delayMicroseconds
/// @param us Microseconds to delay
inline void delayUs(u32 us) {
  delayMicroseconds(us);
}

/// Shorter alias for delayNanoseconds (template version)
/// @tparam NS Number of nanoseconds (at compile-time)
template<u32 NS> inline void delayNs() {
  delayNanoseconds<NS>();
}

/// Shorter alias for delayNanoseconds (runtime version)
/// @param ns Number of nanoseconds (runtime value)
inline void delayNs(u32 ns) {
  delayNanoseconds(ns);
}

/// Shorter alias for delayNanoseconds with explicit clock frequency
/// @param ns Number of nanoseconds
/// @param hz CPU frequency in Hz
inline void delayNs(u32 ns, u32 hz) {
  delayNanoseconds(ns, hz);
}

}  // namespace fl

#endif // __INC_FL_DELAY_H
