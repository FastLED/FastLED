#pragma once

/// @file fl/clockless/chipset.h
/// @brief Chipset type tags for FastLED controllers
///
/// This file defines the Chipset namespace containing type tags for LED chipsets.
/// These tags are used in FastLED's bulk controller API. Currently contains clockless
/// chipset types, but may be expanded in the future to include clocked (SPI) chipsets.

namespace fl {

/// @brief Chipset type tags for BulkClockless API
///
/// This namespace provides simple type tags to specify chipset types for bulk controllers
/// without template parameter ambiguity. Each tag type maps to a specific timing
/// configuration defined in fl/chipsets/led_timing.h.
///
/// @note Currently this namespace contains only clockless chipset type tags. In the future,
/// this may be expanded to include clocked (SPI-based) chipsets as well, making it
/// a unified chipset specification system for all FastLED controllers.
///
/// Usage:
/// @code
/// auto& bulk = FastLED.addBulkLeds<Chipset::WS2812, RMT>({
///     {2, strip1, 100, screenmap1},
///     {4, strip2, 100, screenmap2}
/// });
/// @endcode
namespace Chipset {
    // ============================================================================
    // Fast-Speed Chipsets (800kHz - 1600kHz range)
    // ============================================================================

    struct GE8822 {};             ///< GE8822 @ 800kHz - RGB controller
    struct WS2812 {};             ///< WS2812/WS2812B @ 800kHz - most common addressable RGB LED
    struct WS2812_LEGACY {};      ///< WS2812 @ 800kHz - legacy timing variant
    struct WS2812B_V5 {};         ///< WS2812B-V5 @ 800kHz - newer variant with tighter timing (220/580ns)
    struct WS2812B_MINI_V3 {};    ///< WS2812B-Mini-V3 @ 800kHz - newer variant (same timing as V5)
    struct WS2813 {};             ///< WS2813 @ 800kHz - similar to WS2812 with backup data line
    struct SK6812 {};             ///< SK6812 @ 800kHz - supports RGBW
    struct SK6822 {};             ///< SK6822 @ 800kHz - RGB controller
    struct UCS1903B {};           ///< UCS1903B @ 800kHz
    struct UCS1904 {};            ///< UCS1904 @ 800kHz
    struct UCS2903 {};            ///< UCS2903 @ 800kHz
    struct TM1809 {};             ///< TM1809 @ 800kHz - RGB controller
    struct TM1829_800 {};         ///< TM1829 @ 800kHz - RGB controller
    struct TM1829_1600 {};        ///< TM1829 @ 1600kHz - high-speed variant
    struct LPD1886 {};            ///< LPD1886 @ 1250kHz
    struct PL9823 {};             ///< PL9823 @ 800kHz - RGB controller
    struct SM16703 {};            ///< SM16703 @ 800kHz - RGB controller
    struct SM16824E {};           ///< SM16824E - high-speed RGB controller

    // ============================================================================
    // Medium-Speed Chipsets (400kHz - 600kHz range)
    // ============================================================================

    struct WS2811_400 {};         ///< WS2811 @ 400kHz - older protocol, slower variant
    struct WS2815 {};             ///< WS2815 @ 400kHz - 12V variant (can be overclocked to 800kHz)
    struct UCS1903 {};            ///< UCS1903 @ 400kHz
    struct DP1903_400 {};         ///< DP1903 @ 400kHz
    struct TM1803 {};             ///< TM1803 @ 400kHz
    struct GW6205_400 {};         ///< GW6205 @ 400kHz
    struct UCS1912 {};            ///< UCS1912 @ 800kHz

    // ============================================================================
    // Legacy/Special Chipsets
    // ============================================================================

    struct WS2811_800_LEGACY {};  ///< WS2811 @ 800kHz - legacy fast variant
    struct GW6205_800 {};         ///< GW6205 @ 800kHz - fast variant
    struct DP1903_800 {};         ///< DP1903 @ 800kHz - fast variant

    // ============================================================================
    // RGBW Chipsets (16-bit color depth variants)
    // ============================================================================

    struct TM1814 {};             ///< TM1814 @ 800kHz - RGBW controller

    // ============================================================================
    // UCS7604 Special 16-Bit RGBW Controller
    // ============================================================================

    struct UCS7604_800 {};        ///< UCS7604 @ 800kHz - 16-bit RGBW with WS2812-compatible timing
    struct UCS7604_1600 {};       ///< UCS7604 @ 1600kHz - 16-bit RGBW high-speed variant
}

} // namespace fl
