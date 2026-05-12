/// @file fl/chipsets/chipset_timing_config.h
/// @brief Runtime chipset timing configuration for clockless LED drivers
///
/// `ChipsetTimingConfig` carries the bit-period timing (T1/T2/T3/RESET) for a
/// clockless chipset. The encoding pipeline selector (`ClocklessEncoder`) is
/// a peer concern and lives on `ClocklessChipset` (in `fl/channels/config.h`),
/// not on this type. See issue #2467.

#pragma once

#include "fl/stl/stdint.h"
#include "fl/chipsets/led_timing.h"
#include "fl/chipsets/clockless_encoder.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// @brief Runtime bit-period timing for a clockless chipset
///
/// Pure timing — no encoder field. The byte-level encoder pipeline lives on
/// `ClocklessChipset` as a peer field.
struct ChipsetTimingConfig {
    constexpr ChipsetTimingConfig() FL_NOEXCEPT
        : t1_ns(0), t2_ns(0), t3_ns(0), reset_us(0), name("UNSET") {}
    constexpr ChipsetTimingConfig(u32 t1, u32 t2, u32 t3, u32 reset,
                                  const char* name = "UNNAMED CHIPSET") FL_NOEXCEPT
        : t1_ns(t1), t2_ns(t2), t3_ns(t3), reset_us(reset), name(name) {}

    u32 t1_ns;           ///< T0H: High time for bit 0 (nanoseconds)
    u32 t2_ns;           ///< T1H-T0H: Additional high time for bit 1 (nanoseconds)
    u32 t3_ns;           ///< T0L: Low tail duration (nanoseconds)
    u32 reset_us;        ///< Reset/latch time (microseconds)
    const char* name;    ///< Human-readable chipset name

    /// @brief Get total bit period (T1 + T2 + T3)
    constexpr u32 total_period_ns() const FL_NOEXCEPT {
        return t1_ns + t2_ns + t3_ns;
    }

    /// @brief Equality operator (all timing fields participate; name ignored)
    constexpr bool operator==(const ChipsetTimingConfig& other) const FL_NOEXCEPT {
        return t1_ns == other.t1_ns &&
               t2_ns == other.t2_ns &&
               t3_ns == other.t3_ns &&
               reset_us == other.reset_us;
    }

    /// @brief Inequality operator
    constexpr bool operator!=(const ChipsetTimingConfig& other) const FL_NOEXCEPT {
        return !(*this == other);
    }
};

/// @brief Convert compile-time CHIPSET type to runtime timing config
///
/// Extracts T1/T2/T3/RESET from a compile-time chipset traits type.
/// The encoder selector is extracted separately via `encoder_for<TIMING>()`
/// (see `clockless_encoder.h`) and stored on `ClocklessChipset`, not here.
///
/// @tparam CHIPSET Chipset timing trait (e.g., TIMING_WS2812_800KHZ)
/// @return Runtime timing configuration for the chipset
template <typename CHIPSET>
constexpr ChipsetTimingConfig makeTimingConfig() FL_NOEXCEPT {
    return {
        CHIPSET::T1,      // t1_ns
        CHIPSET::T2,      // t2_ns
        CHIPSET::T3,      // t3_ns
        CHIPSET::RESET,   // reset_us
        "CHIPSET"         // name (generic placeholder)
    };
}

} // namespace fl
