/// @file timing_traits.h
/// @brief Compile-time timing extraction from ChipsetTiming structs
///
/// This file provides template utilities to extract timing values (T1, T2, T3)
/// from ChipsetTiming struct parameters at compile-time. This enables controllers
/// to accept a single ChipsetTiming parameter while extracting individual timing
/// values for use in template parameters and constexpr calculations.

#pragma once

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {

// ============================================================================
// TimingTraits - Extract timing values from ChipsetTiming at compile-time
// ============================================================================

/// @brief Compile-time trait to extract timing values from timing types
/// @tparam TIMING Timing type with enum-based T1, T2, T3 values
/// @note Provides constexpr static members for T1, T2, T3 timing values
///
/// Usage:
/// ```cpp
/// template <typename TIMING, EOrder RGB_ORDER>
/// class ClocklessController {
///     static constexpr uint32_t T1 = TimingTraits<TIMING>::T1;
///     static constexpr uint32_t T2 = TimingTraits<TIMING>::T2;
///     static constexpr uint32_t T3 = TimingTraits<TIMING>::T3;
///     // ... use T1, T2, T3 in template instantiations
/// };
/// ```
template <typename TIMING>
struct TimingTraits {
    /// @brief High time for bit 0 (nanoseconds)
    static constexpr u32 T1 = TIMING::T1;

    /// @brief Additional high time for bit 1 (nanoseconds)
    static constexpr u32 T2 = TIMING::T2;

    /// @brief Low tail duration (nanoseconds)
    static constexpr u32 T3 = TIMING::T3;

    /// @brief Reset/latch time (microseconds)
    static constexpr u32 RESET = TIMING::RESET;

    /// @brief Total bit period (T1 + T2 + T3) in nanoseconds
    static constexpr u32 BIT_PERIOD = TIMING::T1 + TIMING::T2 + TIMING::T3;
};

// ============================================================================
// C++11/14 Fallback Helpers (if non-reference template parameters needed)
// ============================================================================

/// @brief Helper to create timing traits from individual timing values
/// Useful for creating custom timing configurations at compile-time
/// @tparam T1_NS High time for bit 0 (nanoseconds)
/// @tparam T2_NS Additional high time for bit 1 (nanoseconds)
/// @tparam T3_NS Low tail duration (nanoseconds)
/// @tparam RESET_US Reset/latch time (microseconds)
template <u32 T1_NS, u32 T2_NS, u32 T3_NS, u32 RESET_US = 280>
struct CustomTimingTraits {
    static constexpr u32 T1 = T1_NS;
    static constexpr u32 T2 = T2_NS;
    static constexpr u32 T3 = T3_NS;
    static constexpr u32 RESET = RESET_US;
    static constexpr u32 BIT_PERIOD = T1_NS + T2_NS + T3_NS;
};

// Conversion from FastLED timings to the type found in datasheets.
// Note: parameter names avoid T0H/T0L/T1H/T1L which are hardware register
// macros on ESP8266 (defined in esp8266_peri.h).
inline void convert_fastled_timings_to_timedeltas(u16 t1_in, u16 t2_in,
                                                  u16 t3_in, u16 *out_t0h,
                                                  u16 *out_t0l, u16 *out_t1h,
                                                  u16 *out_t1l) FL_NOEXCEPT {
    *out_t0h = t1_in;
    *out_t0l = t2_in + t3_in;
    *out_t1h = t1_in + t2_in;
    *out_t1l = t3_in;
}

}  // namespace fl
