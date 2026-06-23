/// @file fl/chipsets/clockless_encoder.h
/// @brief Encoding pipeline selector for clockless LED chipsets
///
/// The bit-period timing (T1/T2/T3/RESET) for clockless chipsets lives in
/// `fl/chipsets/led_timing.h`. This header captures the *byte-level* concern:
/// which encoder pipeline the Channel API should run for a given chipset.
///
/// Most clockless chipsets use plain WS2812-style encoding (raw pixel bytes).
/// Some chipsets (e.g., UCS7604) require special preambles and encoding modes.
/// This enum is the runtime dispatch tag the Channel API uses to select the
/// correct encoder.

#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// @brief Identifies which encoder to use for clockless chipsets in the Channel API
enum class ClocklessEncoder : u8 {
    CLOCKLESS_ENCODER_WS2812 = 0,           ///< Default, no preamble (WS2812 and compatible)
    CLOCKLESS_ENCODER_UCS7604_8BIT,         ///< UCS7604 8-bit 800KHz
    CLOCKLESS_ENCODER_UCS7604_16BIT,        ///< UCS7604 16-bit 800KHz
    CLOCKLESS_ENCODER_UCS7604_16BIT_1600    ///< UCS7604 16-bit 1600KHz
};

/// @brief SFINAE helpers for detecting a static ENCODER member on timing structs
namespace detail {
    template <typename T>
    constexpr auto test_encoder(int) FL_NOEXCEPT -> decltype(T::ENCODER, true) { return true; }
    template <typename T>
    constexpr bool test_encoder(...) FL_NOEXCEPT { return false; }

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

/// @brief Extract the encoder selector from a compile-time TIMING type
///
/// If TIMING has a static `ENCODER` member it is returned; otherwise defaults
/// to `CLOCKLESS_ENCODER_WS2812`.
template <typename TIMING>
constexpr ClocklessEncoder encoder_for() FL_NOEXCEPT {
    return detail::get_encoder<TIMING, has_encoder<TIMING>::value>::value;
}

} // namespace fl
