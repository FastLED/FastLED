/// @file fl/chipsets/chipset_timing_config.h
/// @brief Runtime chipset timing configuration for clockless LED drivers

#pragma once

#include "fl/stl/stdint.h"
#include "fl/chipsets/led_timing.h"

namespace fl {

/// @brief Runtime configuration for chipset timing
///
/// This struct captures the essential timing information from compile-time
/// chipset definitions and makes it available at runtime for driver configuration.
struct ChipsetTimingConfig {
    constexpr ChipsetTimingConfig(uint32_t t1, uint32_t t2, uint32_t t3, uint32_t reset, const char* name = "UNNAMED CHIPSET")
        : t1_ns(t1), t2_ns(t2), t3_ns(t3), reset_us(reset), name(name) {}
    uint32_t t1_ns;      ///< T0H: High time for bit 0 (nanoseconds)
    uint32_t t2_ns;      ///< T1H-T0H: Additional high time for bit 1 (nanoseconds)
    uint32_t t3_ns;      ///< T0L: Low tail duration (nanoseconds)
    uint32_t reset_us;   ///< Reset/latch time (microseconds)
    const char* name;    ///< Human-readable chipset name

    /// @brief Get total bit period (T1 + T2 + T3)
    constexpr uint32_t total_period_ns() const {
        return t1_ns + t2_ns + t3_ns;
    }

    /// @brief Equality operator for chipset grouping
    /// @note Ignores name field - only timing parameters matter for grouping
    constexpr bool operator==(const ChipsetTimingConfig& other) const {
        return t1_ns == other.t1_ns &&
               t2_ns == other.t2_ns &&
               t3_ns == other.t3_ns &&
               reset_us == other.reset_us;
    }

    /// @brief Inequality operator
    constexpr bool operator!=(const ChipsetTimingConfig& other) const {
        return !(*this == other);
    }
};

/// @brief Convert compile-time CHIPSET type to runtime timing config
///
/// This helper function bridges the gap between template-based chipset definitions
/// and runtime configuration. It extracts timing values at compile time and
/// packages them into a runtime-accessible struct.
///
/// @tparam CHIPSET Chipset timing trait (e.g., TIMING_WS2812_800KHZ)
/// @return Runtime timing configuration for the chipset
///
/// @example
/// ```cpp
/// auto ws2812_config = makeTimingConfig<TIMING_WS2812_800KHZ>();
/// auto sk6812_config = makeTimingConfig<TIMING_SK6812>();
/// ```
template <typename CHIPSET>
constexpr ChipsetTimingConfig makeTimingConfig() {
    return {
        CHIPSET::T1,      // t1_ns
        CHIPSET::T2,      // t2_ns
        CHIPSET::T3,      // t3_ns
        CHIPSET::RESET,   // reset_us
        "CHIPSET"         // name (generic placeholder)
    };
}

} // namespace fl
