#pragma once

#ifndef __INC_FL_DELAY_H
#define __INC_FL_DELAY_H

/// @file fl/system/delay.h
/// Delay utilities for FastLED
/// Includes nanosecond-precision delays, cycle counting, and microsecond delays

#include "platforms/cycle_type.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/int.h"
#include "fl/stl/type_traits.h"
// ============================================================================
// Platform-specific includes via trampoline header
// ============================================================================

#include "platforms/delay.h"  // IWYU pragma: keep
#include "fl/stl/noexcept.h"

namespace fl {

namespace detail {

/// Convert nanoseconds to CPU cycles
/// @param ns Number of nanoseconds
/// @param hz CPU frequency in Hz
/// @return Number of cycles (rounded up)
constexpr fl::u32 cycles_from_ns(fl::u32 ns, fl::u32 hz) FL_NOEXCEPT {
  // Round up: cycles = ceil(ns * hz / 1e9)
  // Using: (ns * hz + 999'999'999) / 1'000'000'000
  return ((fl::u64)ns * (fl::u64)hz + 999999999UL) / 1000000000UL;
}

/// Compute cycles using default CPU frequency (compile-time)
/// @param ns Number of nanoseconds
/// @return Number of cycles using GET_CPU_FREQUENCY()
constexpr fl::u32 cycles_from_ns_default(fl::u32 ns) FL_NOEXCEPT {
  return cycles_from_ns(ns, GET_CPU_FREQUENCY());
}

} // namespace detail

/// Delay for a compile-time constant number of nanoseconds
/// @tparam NS Number of nanoseconds (at compile-time)
template<fl::u32 NS>
FASTLED_FORCE_INLINE void delayNanoseconds() FL_NOEXCEPT {
  // Delegate to platform-specific implementation with compile-time constant
  // Platform-specific delayNanoseconds_impl() functions are provided by platforms/delay.h
  delayNanoseconds_impl(NS);
}

/// Delay for a runtime number of nanoseconds
/// @param ns Number of nanoseconds (runtime value)
void delayNanoseconds(fl::u32 ns) FL_NOEXCEPT;

/// Delay for a runtime number of nanoseconds with explicit clock frequency
/// @param ns Number of nanoseconds
/// @param hz CPU frequency in Hz
void delayNanoseconds(fl::u32 ns, fl::u32 hz) FL_NOEXCEPT;

// ============================================================================
// Clock cycle-counted delay loop (delaycycles)
// ============================================================================

/// Forward declaration of delaycycles template
/// @tparam CYCLES the number of clock cycles to delay
/// @note No delay is applied if CYCLES is less than or equal to zero.
/// Specializations for small cycle counts and platform-specific optimizations
/// are defined in delay.cpp
template<cycle_t CYCLES> void delaycycles() FL_NOEXCEPT;

/// A variant of delaycycles that will always delay at least one cycle
/// @tparam CYCLES the number of clock cycles to delay
template<cycle_t CYCLES> inline void delaycycles_min1() FL_NOEXCEPT {
  delaycycles<1>();
  delaycycles<CYCLES - 1>();
}

// ============================================================================
// Millisecond and Microsecond delay wrappers
// ============================================================================

namespace detail {

/// Internal delay implementation used by the public fl::delay wrapper
/// @param ms Milliseconds to delay
/// @param run_async If true, pump async tasks during delay (only on platforms with SKETCH_HAS_LARGE_MEMORY==1)
void delay_impl(u32 ms, bool run_async = true) FL_NOEXCEPT;

}  // namespace detail

/// Public delay wrapper that keeps bare Arduino delay() preferred after
/// `using fl::delay;` while still allowing explicit fl::delay() calls.
/// @param ms Milliseconds to delay
/// @param run_async If true, pump async tasks during delay (only on platforms with SKETCH_HAS_LARGE_MEMORY==1)
template<int Dummy = 0>
inline void delay(u32 ms, bool run_async = true) FL_NOEXCEPT {
    (void)Dummy;
    detail::delay_impl(ms, run_async);
}

/// Delay for a given number of milliseconds (legacy - no async pumping)
/// @param ms Milliseconds to delay
void delayMillis(u32 ms) FL_NOEXCEPT;

/// Delay for a given number of microseconds
/// @param us Microseconds to delay
void delayMicroseconds(u32 us) FL_NOEXCEPT;

/// Shorter alias for delayMicroseconds
/// @param us Microseconds to delay
inline void delayUs(u32 us) FL_NOEXCEPT {
  delayMicroseconds(us);
}

/// Shorter alias for delay with optional async task pumping
/// @param ms Milliseconds to delay
/// @param run_async If true, pump async tasks during delay (only on platforms with SKETCH_HAS_LARGE_MEMORY==1)
inline void delayMs(u32 ms, bool run_async = true) FL_NOEXCEPT {
  detail::delay_impl(ms, run_async);
}

/// Shorter alias for delayNanoseconds (template version)
/// @tparam NS Number of nanoseconds (at compile-time)
template<u32 NS> inline void delayNs() FL_NOEXCEPT {
  delayNanoseconds<NS>();
}

/// Shorter alias for delayNanoseconds (runtime version)
/// @param ns Number of nanoseconds (runtime value)
inline void delayNs(u32 ns) FL_NOEXCEPT {
  delayNanoseconds(ns);
}

/// Shorter alias for delayNanoseconds with explicit clock frequency
/// @param ns Number of nanoseconds
/// @param hz CPU frequency in Hz
inline void delayNs(u32 ns, u32 hz) FL_NOEXCEPT {
  delayNanoseconds(ns, hz);
}

}  // namespace fl

#endif // __INC_FL_DELAY_H
