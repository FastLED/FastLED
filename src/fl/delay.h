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
  // Delegate to platform-specific implementation with compile-time constant
  // Platform-specific delayNanoseconds_impl() functions are provided by platforms/delay.h
  delayNanoseconds_impl(NS);
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

/// Delay for a given number of milliseconds with optional async task pumping
/// @param ms Milliseconds to delay
/// @param run_async If true, pump async tasks during delay (only on platforms with SKETCH_HAS_LOTS_OF_MEMORY==1)
void delay(u32 ms, bool run_async = true);

/// Overload for signed int to avoid ambiguity with Arduino's delay(unsigned long)
/// @param ms Milliseconds to delay
inline void delay(int ms) {
    delay(static_cast<u32>(ms), true);
}

/// Overload for signed long to avoid ambiguity with Arduino's delay(unsigned long)
/// @param ms Milliseconds to delay
inline void delay(long ms) {
    delay(static_cast<u32>(ms), true);
}

#if !defined(__AVR__) && !defined(FL_IS_ARM) && !defined(__arm__) && !defined(_WIN32) && !defined(__EMSCRIPTEN__)
/// Overload for unsigned long to match Arduino's delay(unsigned long) exactly
/// Only enabled on platforms where unsigned long != u32 (e.g., ESP32 where u32 is unsigned int)
/// @param ms Milliseconds to delay
inline void delay(unsigned long ms) {
    delay(static_cast<u32>(ms), true);
}
#endif

/// Overload for signed int with async flag
/// @param ms Milliseconds to delay
/// @param run_async If true, pump async tasks during delay
inline void delay(int ms, bool run_async) {
    delay(static_cast<u32>(ms), run_async);
}

/// Overload for signed long with async flag
/// @param ms Milliseconds to delay
/// @param run_async If true, pump async tasks during delay
inline void delay(long ms, bool run_async) {
    delay(static_cast<u32>(ms), run_async);
}

#if !defined(__AVR__) && !defined(FL_IS_ARM) && !defined(__arm__)
/// Overload for unsigned long with async flag
/// Only enabled on platforms where unsigned long != u32
/// @param ms Milliseconds to delay
/// @param run_async If true, pump async tasks during delay
inline void delay(unsigned long ms, bool run_async) {
    delay(static_cast<u32>(ms), run_async);
}
#endif

/// Delay for a given number of milliseconds (legacy - no async pumping)
/// @param ms Milliseconds to delay
void delayMillis(u32 ms);

/// Delay for a given number of microseconds
/// @param us Microseconds to delay
void delayMicroseconds(u32 us);

/// Shorter alias for delayMicroseconds
/// @param us Microseconds to delay
inline void delayUs(u32 us) {
  delayMicroseconds(us);
}

/// Shorter alias for delay with optional async task pumping
/// @param ms Milliseconds to delay
/// @param run_async If true, pump async tasks during delay (only on platforms with SKETCH_HAS_LOTS_OF_MEMORY==1)
inline void delayMs(u32 ms, bool run_async = true) {
  delay(ms, run_async);
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
