#pragma once

#ifndef __INC_FL_DELAY_H
#define __INC_FL_DELAY_H

/// @file fl/delay.h
/// Comprehensive delay utilities for FastLED
/// Includes nanosecond-precision delays, cycle counting, and microsecond delays

#include "fl/types.h"
#include "fl/force_inline.h"
#include "fl/compiler_control.h"

// ============================================================================
// Platform-specific includes (must be outside detail namespace)
// ============================================================================

#if defined(ARDUINO_ARCH_AVR)
#include "platforms/avr/delay.h"
#elif defined(ESP32) && !defined(ESP32C3) && !defined(ESP32C6)
#include "platforms/esp/32/delay.h"
#elif defined(ESP32C3) || defined(ESP32C6)
#include "platforms/esp/32/delay_riscv.h"
#elif defined(ARDUINO_ARCH_RP2040)
#include "platforms/arm/rp/rp2040/delay.h"
#elif defined(NRF52_SERIES)
#include "platforms/arm/nrf52/delay.h"
#elif defined(ARDUINO_ARCH_SAMD)
#include "platforms/arm/d21/delay.h"
#elif defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)
#include "platforms/arm/stm32/delay.h"
#else
#include "platforms/delay_generic.h"
#endif

// ============================================================================
// Platform-specific cycle delay includes
// ============================================================================

// ESP32 core has its own definition of NOP, so undef it first
#ifdef ESP32
#undef NOP
#undef NOP2
#endif

// Include platform-specific cycle delay implementations
#if defined(__AVR__)
#include "platforms/avr/delay_cycles.h"
#elif defined(ESP32)
#include "platforms/shared/delay_cycles_generic.h"
#include "platforms/esp/delay_cycles_esp32.h"
#else
#include "platforms/shared/delay_cycles_generic.h"
#endif

namespace fl {

// ============================================================================
// Forward declarations
// ============================================================================

#if defined(ESP32)
/// Get the current ESP32 CPU frequency at runtime
u32 esp_clk_cpu_freq_impl();
#endif

namespace detail {

// ============================================================================
// Nanosecond to cycle conversion
// ============================================================================

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

/// Delay for a compile-time constant number of nanoseconds
/// @tparam NS Number of nanoseconds (at compile-time)
/// This function is preferred when NS is known at compile-time
/// as it allows for compile-time cycle calculation and optimization
template<fl::u32 NS>
FASTLED_FORCE_INLINE void delayNanoseconds() {
  // Resolve platform clock frequency at compile-time
#if defined(F_CPU)
  constexpr fl::u32 HZ = F_CPU;
#elif defined(CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ)
  // ESP-IDF style (can be tuned via menuconfig)
  constexpr fl::u32 HZ = (fl::u32)CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ * 1000000UL;
#elif defined(ESP32)
  // Fallback for ESP32: assume 160 MHz (conservative default)
  constexpr fl::u32 HZ = 160000000UL;
#elif defined(ARDUINO_ARCH_RP2040)
  // RP2040: standard 125 MHz
  constexpr fl::u32 HZ = 125000000UL;
#elif defined(NRF52_SERIES)
  // nRF52: standard 64 MHz
  constexpr fl::u32 HZ = 64000000UL;
#elif defined(ARDUINO_ARCH_SAMD)
  // SAMD21: default 48 MHz, SAMD51: 120 MHz (use conservative default)
  constexpr fl::u32 HZ = 48000000UL;
#else
  // Fallback: assume 16 MHz (common Arduino)
  constexpr fl::u32 HZ = 16000000UL;
#endif

  constexpr fl::u32 cycles = detail::cycles_from_ns(NS, HZ);

  // If cycles == 0, do nothing (no delay needed)
  // Template specialization handles zero-cycle case separately
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
/// This function is used when the delay duration is not known at compile-time.
/// It will query the current CPU frequency and calculate cycles at runtime.
inline void delayNanoseconds(fl::u32 ns) {
  // Query runtime clock frequency
#if defined(ESP32)
  fl::u32 hz = esp_clk_cpu_freq_impl();
#elif defined(ARDUINO_ARCH_RP2040)
  // RP2040 clock is fixed at 125 MHz in normal mode
  fl::u32 hz = 125000000UL;
#elif defined(NRF52_SERIES)
  // nRF52 typically runs at 64 MHz
  fl::u32 hz = 64000000UL;
#elif defined(ARDUINO_ARCH_SAMD)
  // SAMD uses F_CPU if defined
  #if defined(F_CPU)
  fl::u32 hz = F_CPU;
  #else
  fl::u32 hz = 48000000UL;  // SAMD21 default
  #endif
#else
  // Fallback to F_CPU (most platforms)
  #if defined(F_CPU)
  fl::u32 hz = F_CPU;
  #else
  fl::u32 hz = 16000000UL;
  #endif
#endif

  fl::u32 cycles = detail::cycles_from_ns(ns, hz);

  if (cycles == 0) return;

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

/// Delay for a runtime number of nanoseconds with explicit clock frequency
/// @param ns Number of nanoseconds
/// @param hz CPU frequency in Hz
/// This variant allows you to explicitly specify the clock frequency,
/// useful for testing or when the CPU frequency is not automatically detectable.
inline void delayNanoseconds(fl::u32 ns, fl::u32 hz) {
  fl::u32 cycles = detail::cycles_from_ns(ns, hz);

  if (cycles == 0) return;

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

// ============================================================================
// Clock cycle-counted delay loop (delaycycles)
// ============================================================================

/// Forward declaration of delaycycles template
/// @tparam CYCLES the number of clock cycles to delay
/// @note No delay is applied if CYCLES is less than or equal to zero.
/// Platform-specific implementations are in platforms/*/delay_cycles.h
template<cycle_t CYCLES> inline void delaycycles();

/// A variant of delaycycles that will always delay at least one cycle
/// @tparam CYCLES the number of clock cycles to delay
template<cycle_t CYCLES> inline void delaycycles_min1() {
  delaycycles<1>();
  delaycycles<CYCLES - 1>();
}

// ============================================================================
// Template specializations for delaycycles
// ============================================================================
// These pre-instantiated specializations optimize compilation time and code size
// for common cycle counts.
//
// To reduce compilation time in projects that don't use small cycle delays,
// define FASTLED_NO_DELAY_SPECIALIZATIONS before including FastLED.h:
//
// @code
// #define FASTLED_NO_DELAY_SPECIALIZATIONS
// #include "FastLED.h"
// @endcode

/// @cond DOXYGEN_SKIP

// Specializations for small negative and zero cycles
template<> FASTLED_FORCE_INLINE void delaycycles<-10>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-9>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-8>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-7>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-6>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-5>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-4>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-3>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-2>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<-1>() {}
template<> FASTLED_FORCE_INLINE void delaycycles<0>() {}

// Specializations for small positive cycles
template<> FASTLED_FORCE_INLINE void delaycycles<1>() { FL_NOP; }
template<> FASTLED_FORCE_INLINE void delaycycles<2>() { FL_NOP2; }
template<> FASTLED_FORCE_INLINE void delaycycles<3>() {
  FL_NOP;
  FL_NOP2;
}
template<> FASTLED_FORCE_INLINE void delaycycles<4>() {
  FL_NOP2;
  FL_NOP2;
}
template<> FASTLED_FORCE_INLINE void delaycycles<5>() {
  FL_NOP2;
  FL_NOP2;
  FL_NOP;
}

// Specialization for a gigantic amount of cycles on ESP32
#if defined(ESP32)
template<> FASTLED_FORCE_INLINE void delaycycles<4294966398>() {
	// specialization for a gigantic amount of cycles, apparently this is needed
	// or esp32 will blow the stack with cycles = 4294966398.
	const fl::u32 termination = 4294966398 / 10;
	const fl::u32 remainder = 4294966398 % 10;
	for (fl::u32 i = 0; i < termination; i++) {
		FL_NOP; FL_NOP; FL_NOP; FL_NOP; FL_NOP;
		FL_NOP; FL_NOP; FL_NOP; FL_NOP; FL_NOP;
	}

	// remainder
	switch (remainder) {
		case 9: FL_NOP;
		case 8: FL_NOP;
		case 7: FL_NOP;
		case 6: FL_NOP;
		case 5: FL_NOP;
		case 4: FL_NOP;
		case 3: FL_NOP;
		case 2: FL_NOP;
		case 1: FL_NOP;
	}
}
#endif

/// @endcond

// ============================================================================
// Millisecond and Microsecond delay wrappers
// ============================================================================

/// Delay for a given number of milliseconds
/// @param ms Milliseconds to delay
/// @note Uses platform-provided delay() function (from Arduino, led_sysdefs, or platform layer)
/// Implementation provided in delay.cpp
void delay(u32 ms);

/// Delay for a given number of microseconds
/// @param us Microseconds to delay
/// @note Uses platform-provided delayMicroseconds() function (from Arduino, led_sysdefs, or platform layer)
/// Implementation provided in delay.cpp
void delayMicroseconds(u32 us);

/// Alias for delayMicroseconds (shorter name for convenience)
/// @param us Microseconds to delay
inline void delayMicros(u32 us) {
  delayMicroseconds(us);
}

}  // namespace fl

#endif // __INC_FL_DELAY_H
