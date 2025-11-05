/// @file timing_conversion.h
/// @brief Timing conversion utilities for clockless LED protocols
///
/// This module provides timing format conversions between standard LED datasheet
/// timing specifications (T0H, T0L, T1H, T1L) and the compact three-phase timing
/// protocol (T1, T2, T3).

#pragma once

#include "fl/stdint.h"
#include "fl/math_macros.h"
#include "fl/compiler_control.h"

namespace fl {

// ============================================================================
// Timing Format Conversion
// ============================================================================

/// @brief Datasheet timing format (standard LED specification)
///
/// Most LED datasheets specify timing in four values:
/// - T0H: Time HIGH when sending a '0' bit (nanoseconds)
/// - T0L: Time LOW when sending a '0' bit (nanoseconds)
/// - T1H: Time HIGH when sending a '1' bit (nanoseconds)
/// - T1L: Time LOW when sending a '1' bit (nanoseconds)
struct DatasheetTiming {
    uint32_t T0H;  ///< High time for '0' bit (ns)
    uint32_t T0L;  ///< Low time for '0' bit (ns)
    uint32_t T1H;  ///< High time for '1' bit (ns)
    uint32_t T1L;  ///< Low time for '1' bit (ns)

    /// @brief Get total cycle time for '0' bit
    constexpr uint32_t cycle_0() const { return T0H + T0L; }

    /// @brief Get total cycle time for '1' bit
    constexpr uint32_t cycle_1() const { return T1H + T1L; }

    /// @brief Get maximum cycle duration
    constexpr uint32_t duration() const {
        return FL_MAX(cycle_0(), cycle_1());
    }
};

/// @brief Three-phase timing format (compact 3-parameter representation)
///
/// Three-phase timing protocol:
/// - At T=0        : Line goes HIGH (start of bit)
/// - At T=T1       : Line goes LOW (for '0' bit)
/// - At T=T1+T2    : Line goes LOW (for '1' bit)
/// - At T=T1+T2+T3 : Cycle complete (ready for next bit)
struct Timing3Phase {
    uint32_t T1;  ///< High time for '0' bit (ns)
    uint32_t T2;  ///< Additional high time for '1' bit (ns)
    uint32_t T3;  ///< Low tail duration (ns)

    /// @brief Get total cycle duration
    constexpr uint32_t duration() const { return T1 + T2 + T3; }

    /// @brief Get high time for '0' bit
    constexpr uint32_t high_time_0() const { return T1; }

    /// @brief Get high time for '1' bit
    constexpr uint32_t high_time_1() const { return T1 + T2; }
};

/// @brief Convert datasheet timing to 3-phase timing format
///
/// This is the CORRECTED algorithm that fixes bugs in the original
/// chipsets.h embedded Python script (issue #1806).
///
/// Algorithm:
///   duration = max(T0H + T0L, T1H + T1L)
///   T1 = T0H              // High time for '0' bit
///   T2 = T1H - T0H        // Additional time for '1' bit (CORRECTED)
///   T3 = duration - T1H   // Tail time after '1' bit (CORRECTED)
///
/// Example (WS2812B):
///   Input:  T0H=400, T0L=850, T1H=850, T1L=400
///   Output: T1=400, T2=450, T3=400
///
/// @param ds Datasheet timing values
/// @return 3-phase timing values
FL_CONSTEXPR14 Timing3Phase datasheet_to_phase3(const DatasheetTiming& ds) {
    const uint32_t duration = FL_MAX(ds.T0H + ds.T0L, ds.T1H + ds.T1L);

    return Timing3Phase{
        ds.T0H,              // T1: High time for '0' bit
        ds.T1H - ds.T0H,     // T2: Additional time for '1' bit
        duration - ds.T1H    // T3: Tail time after '1' bit
    };
}

/// @brief Convert 3-phase timing to datasheet timing format
///
/// This conversion is UNDERDETERMINED because:
///   - We have 3 input values (T1, T2, T3)
///   - We need 4 output values (T0H, T0L, T1H, T1L)
///
/// Assumption: Symmetric cycle times (T0H+T0L = T1H+T1L = duration)
/// This gives us: T0L = duration - T0H, T1L = duration - T1H
///
/// Note: The inverse conversion may not perfectly reconstruct the original
/// datasheet values if the LED chipset has asymmetric cycle times.
///
/// Example (WS2812):
///   Input:  T1=250, T2=625, T3=375
///   Output: T0H=250, T0L=1000, T1H=875, T1L=375
///
/// @param phase3 3-phase timing values
/// @return Datasheet timing values (with symmetric cycle assumption)
FL_CONSTEXPR14 DatasheetTiming phase3_to_datasheet(const Timing3Phase& phase3) {
    const uint32_t duration = phase3.T1 + phase3.T2 + phase3.T3;
    const uint32_t T0H = phase3.T1;
    const uint32_t T1H = phase3.T1 + phase3.T2;

    return DatasheetTiming{
        T0H,                // T0H: High time for '0' bit
        duration - T0H,     // T0L: Low time for '0' bit
        T1H,                // T1H: High time for '1' bit
        duration - T1H      // T1L: Low time for '1' bit
    };
}

}  // namespace fl
