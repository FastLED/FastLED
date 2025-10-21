/// @file timing_traits.h
/// @brief Compile-time timing extraction from ChipsetTiming structs
///
/// This file provides template utilities to extract timing values (T1, T2, T3)
/// from ChipsetTiming struct parameters at compile-time. This enables controllers
/// to accept a single ChipsetTiming parameter while extracting individual timing
/// values for use in template parameters and constexpr calculations.

#pragma once

#include "led_timing.h"
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
    static constexpr uint32_t T1 = TIMING::T1;

    /// @brief Additional high time for bit 1 (nanoseconds)
    static constexpr uint32_t T2 = TIMING::T2;

    /// @brief Low tail duration (nanoseconds)
    static constexpr uint32_t T3 = TIMING::T3;

    /// @brief Reset/latch time (microseconds)
    static constexpr uint32_t RESET = TIMING::RESET;

    /// @brief Total bit period (T1 + T2 + T3) in nanoseconds
    static constexpr uint32_t BIT_PERIOD = TIMING::T1 + TIMING::T2 + TIMING::T3;
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
template <uint32_t T1_NS, uint32_t T2_NS, uint32_t T3_NS, uint32_t RESET_US = 280>
struct CustomTimingTraits {
    static constexpr uint32_t T1 = T1_NS;
    static constexpr uint32_t T2 = T2_NS;
    static constexpr uint32_t T3 = T3_NS;
    static constexpr uint32_t RESET = RESET_US;
    static constexpr uint32_t BIT_PERIOD = T1_NS + T2_NS + T3_NS;
};

}  // namespace fl
