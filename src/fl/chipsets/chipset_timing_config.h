/// @file fl/chipsets/chipset_timing_config.h
/// @brief Runtime chipset timing configuration for clockless LED drivers

#pragma once

#include "fl/stl/stdint.h"
#include "fl/chipsets/led_timing.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// @brief Runtime configuration for chipset timing
///
/// This struct captures the essential timing information from compile-time
/// chipset definitions and makes it available at runtime for driver configuration.
struct ChipsetTimingConfig {
    constexpr ChipsetTimingConfig() FL_NOEXCEPT
        : t1_ns(0), t2_ns(0), t3_ns(0), reset_us(0), name("UNSET"), encoder(ClocklessEncoder::CLOCKLESS_ENCODER_WS2812) {}
    constexpr ChipsetTimingConfig(u32 t1, u32 t2, u32 t3, u32 reset, const char* name = "UNNAMED CHIPSET",
                                  ClocklessEncoder encoder = ClocklessEncoder::CLOCKLESS_ENCODER_WS2812)
        : t1_ns(t1), t2_ns(t2), t3_ns(t3), reset_us(reset), name(name), encoder(encoder) {}
    u32 t1_ns;      ///< T0H: High time for bit 0 (nanoseconds)
    u32 t2_ns;      ///< T1H-T0H: Additional high time for bit 1 (nanoseconds)
    u32 t3_ns;      ///< T0L: Low tail duration (nanoseconds)
    u32 reset_us;   ///< Reset/latch time (microseconds)
    const char* name;    ///< Human-readable chipset name
    ClocklessEncoder encoder;  ///< Encoder to use (default: WS2812)

    /// @brief Get total bit period (T1 + T2 + T3)
    constexpr u32 total_period_ns() const {
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

/// @brief SFINAE helpers for detecting ENCODER member on timing structs
namespace detail {
    template <typename T>
    constexpr auto test_encoder(int) -> decltype(T::ENCODER, true) { return true; }
    template <typename T>
    constexpr bool test_encoder(...) { return false; }

    template <typename CHIPSET, bool HAS_ENCODER>
    struct get_encoder {
        static constexpr ClocklessEncoder value = ClocklessEncoder::CLOCKLESS_ENCODER_WS2812;
    };
    template <typename CHIPSET>
    struct get_encoder<CHIPSET, true> {
        static constexpr ClocklessEncoder value = CHIPSET::ENCODER;
    };
} // namespace detail

template <typename T>
struct has_encoder {
    static constexpr bool value = detail::test_encoder<T>(0);
};

/// @brief Convert compile-time CHIPSET type to runtime timing config
///
/// This helper function bridges the gap between template-based chipset definitions
/// and runtime configuration. It extracts timing values at compile time and
/// packages them into a runtime-accessible struct.
///
/// If the CHIPSET type has a static ENCODER member (e.g., UCS7604 timing structs),
/// it is propagated to the runtime config. Otherwise, ClocklessEncoder::CLOCKLESS_ENCODER_WS2812 is used.
///
/// @tparam CHIPSET Chipset timing trait (e.g., TIMING_WS2812_800KHZ)
/// @return Runtime timing configuration for the chipset
template <typename CHIPSET>
constexpr ChipsetTimingConfig makeTimingConfig() {
    return {
        CHIPSET::T1,      // t1_ns
        CHIPSET::T2,      // t2_ns
        CHIPSET::T3,      // t3_ns
        CHIPSET::RESET,   // reset_us
        "CHIPSET",        // name (generic placeholder)
        detail::get_encoder<CHIPSET, has_encoder<CHIPSET>::value>::value
    };
}

} // namespace fl
