#pragma once

/// @file fl/clockless/peripheral_tags.h
/// @brief Peripheral type tags and traits for BulkClockless controllers
///
/// This header defines peripheral type markers (LCD_I80, RMT, I2S, SPI_BULK, OFLED)
/// and the PeripheralName trait used by the BulkClockless API.
///
/// @note Cross-Platform Availability and Fallback Behavior
/// All peripheral tags are available on every platform, regardless of hardware support.
/// When a peripheral is not supported by the platform, the BulkClockless API automatically
/// falls back to a generic CPU-based bulk clockless controller implementation.
///
/// The fallback behavior includes warning messages to inform users:
/// - Warnings are emitted either once per draw frame OR once every 250ms (throttled)
/// - Example: Using RMT peripheral on Teensy will trigger CPU fallback with warnings
/// - This ensures consistent API while providing feedback about performance implications

#include "fl/chipsets/timing_traits.h"
#include "fl/chipsets/led_timing.h"
#include "fl/eorder.h"
#include "fl/clockless/chipset.h"

namespace fl {

/// Peripheral type tags
/// These are empty structs used as template parameters to select peripheral implementations
/// All peripherals are available on every platform. On unsupported platforms, the implementation
/// automatically falls back to CPU-based bulk clockless control with periodic warnings.

struct LCD_I80 {};     ///< LCD I80 parallel interface peripheral (ESP32-S3, ESP32-P4). Falls back to CPU on unsupported platforms.
struct RMT {};         ///< Remote Control Transceiver peripheral (ESP32, ESP32-S3, ESP32-C3/C6/H2). Falls back to CPU on unsupported platforms.
#ifdef I2S
#pragma push_macro("I2S")
#undef I2S
#endif
struct I2S {};         ///< I2S audio interface repurposed for LED output (ESP32, ESP32-S3). Falls back to CPU on unsupported platforms.
#ifdef I2S
#pragma pop_macro("I2S")
#endif
struct SPI_BULK {};    ///< SPI peripheral for bulk LED output. Falls back to CPU on unsupported platforms.
struct OFLED {};  ///< OFLED DMA-based parallel output (Teensy 4.x). Falls back to CPU on unsupported platforms.

/// @brief Extract timing struct from chipset template class
/// @tparam CHIPSET Chipset template class (e.g., WS2812B, SK6812)
///
/// This trait maps existing FastLED chipset classes to their timing definitions,
/// allowing BulkClockless API to reuse the same chipset names as traditional addLeds().
///
/// **Why Template Template Parameters:**
/// Existing FastLED chipsets are template classes that take DATA_PIN and RGB_ORDER:
/// @code
/// template<int DATA_PIN, EOrder RGB_ORDER>
/// class WS2812B : public WS2812Controller800Khz<DATA_PIN, RGB_ORDER> {};
/// @endcode
///
/// BulkClockless doesn't use a single DATA_PIN (it has multiple pins), and RGB_ORDER
/// doesn't affect timing. But to accept WS2812B as a parameter, we need to match its
/// template signature using "template template parameters":
/// @code
/// template <template <int, EOrder> class CHIPSET>  // Matches WS2812B's signature
/// struct TimingHelper;
/// @endcode
///
/// The `int` and `EOrder` parameters are just pattern matching - we never instantiate
/// the chipset with actual values. We just map the chipset class to its timing struct.
///
/// **User API:**
/// @code
/// // User writes the same chipset names as traditional addLeds:
/// auto& bulk = FastLED.addBulkLeds<WS2812B, OFLED>({
///     {2, strip1, 100, ScreenMap()},
///     {5, strip2, 100, ScreenMap()}
/// });
/// @endcode
///
/// **Internal Mapping:**
/// @code
/// // TimingHelper extracts timing from WS2812B:
/// using Timing = typename TimingHelper<WS2812B>::VALUE;  // TIMING_WS2812_800KHZ
///
/// // Then ChipsetTraits provides timing values:
/// uint32_t t1 = ChipsetTraits<Timing>::T1;  // 250ns
/// uint32_t t2 = ChipsetTraits<Timing>::T2;  // 625ns
/// uint32_t t3 = ChipsetTraits<Timing>::T3;  // 375ns
/// @endcode
///
/// This design allows users to write `WS2812B` instead of `TIMING_WS2812_800KHZ`,
/// maintaining consistency with the traditional FastLED.addLeds<WS2812B, PIN>() API.
template <template <int, EOrder> class CHIPSET>
struct TimingHelper;

// Forward declarations for chipset classes (actual definitions in chipsets.h)
template <int, EOrder> class WS2812Controller800Khz;
template <int, EOrder> class SK6812Controller;
template <int, EOrder> class WS2811Controller400Khz;
template <int, EOrder> class WS2813Controller;
template <int, EOrder> class WS2815Controller;

/// TimingHelper specializations - map chipset classes to timing structs. TimingHelper
/// ignores RGB_ORDER.
template <> struct TimingHelper<WS2812Controller800Khz> { using VALUE = TIMING_WS2812_800KHZ; };
template <> struct TimingHelper<SK6812Controller> { using VALUE = TIMING_SK6812; };
template <> struct TimingHelper<WS2811Controller400Khz> { using VALUE = TIMING_WS2811_400KHZ; };
template <> struct TimingHelper<WS2813Controller> { using VALUE = TIMING_WS2813; };
template <> struct TimingHelper<WS2815Controller> { using VALUE = TIMING_WS2815; };

/// @brief Chipset trait for TIMING structs
/// @tparam TIMING Timing struct (e.g., TIMING_WS2812_800KHZ)
///
/// Provides timing information and chipset metadata for BulkClockless controllers.
/// Works with centralized TIMING_* structs from fl/chipsets/led_timing.h.
template <typename TIMING>
struct ChipsetTraits {
    static constexpr uint32_t T1 = TIMING::T1;
    static constexpr uint32_t T2 = TIMING::T2;
    static constexpr uint32_t T3 = TIMING::T3;
    static constexpr uint32_t RESET = TIMING::RESET;
    static constexpr bool isClockless() { return true; }

    static constexpr ChipsetTiming runtime_timing() {
        return {T1, T2, T3, RESET, "clockless"};
    }
};

/// @brief ChipsetTraits specializations for Chipset type tags
/// Map chipset types to their corresponding timing structs
///
/// Fast-Speed Chipsets (800kHz - 1600kHz range)
template <> struct ChipsetTraits<Chipset::GE8822> : ChipsetTraits<TIMING_GE8822_800KHZ> {};
template <> struct ChipsetTraits<Chipset::WS2812> : ChipsetTraits<TIMING_WS2812_800KHZ> {};
template <> struct ChipsetTraits<Chipset::WS2812_LEGACY> : ChipsetTraits<TIMING_WS2812_800KHZ_LEGACY> {};
template <> struct ChipsetTraits<Chipset::WS2813> : ChipsetTraits<TIMING_WS2813> {};
template <> struct ChipsetTraits<Chipset::SK6812> : ChipsetTraits<TIMING_SK6812> {};
template <> struct ChipsetTraits<Chipset::SK6822> : ChipsetTraits<TIMING_SK6822> {};
template <> struct ChipsetTraits<Chipset::UCS1903B> : ChipsetTraits<TIMING_UCS1903B_800KHZ> {};
template <> struct ChipsetTraits<Chipset::UCS1904> : ChipsetTraits<TIMING_UCS1904_800KHZ> {};
template <> struct ChipsetTraits<Chipset::UCS2903> : ChipsetTraits<TIMING_UCS2903> {};
template <> struct ChipsetTraits<Chipset::TM1809> : ChipsetTraits<TIMING_TM1809_800KHZ> {};
template <> struct ChipsetTraits<Chipset::TM1829_800> : ChipsetTraits<TIMING_TM1829_800KHZ> {};
template <> struct ChipsetTraits<Chipset::TM1829_1600> : ChipsetTraits<TIMING_TM1829_1600KHZ> {};
template <> struct ChipsetTraits<Chipset::LPD1886> : ChipsetTraits<TIMING_LPD1886_1250KHZ> {};
template <> struct ChipsetTraits<Chipset::PL9823> : ChipsetTraits<TIMING_PL9823> {};
template <> struct ChipsetTraits<Chipset::SM16703> : ChipsetTraits<TIMING_SM16703> {};
template <> struct ChipsetTraits<Chipset::SM16824E> : ChipsetTraits<TIMING_SM16824E> {};

/// Medium-Speed Chipsets (400kHz - 600kHz range)
template <> struct ChipsetTraits<Chipset::WS2811_400> : ChipsetTraits<TIMING_WS2811_400KHZ> {};
template <> struct ChipsetTraits<Chipset::WS2815> : ChipsetTraits<TIMING_WS2815> {};
template <> struct ChipsetTraits<Chipset::UCS1903> : ChipsetTraits<TIMING_UCS1903_400KHZ> {};
template <> struct ChipsetTraits<Chipset::DP1903_400> : ChipsetTraits<TIMING_DP1903_400KHZ> {};
template <> struct ChipsetTraits<Chipset::TM1803> : ChipsetTraits<TIMING_TM1803_400KHZ> {};
template <> struct ChipsetTraits<Chipset::GW6205_400> : ChipsetTraits<TIMING_GW6205_400KHZ> {};
template <> struct ChipsetTraits<Chipset::UCS1912> : ChipsetTraits<TIMING_UCS1912> {};

/// Legacy/Special Chipsets
template <> struct ChipsetTraits<Chipset::WS2811_800_LEGACY> : ChipsetTraits<TIMING_WS2811_800KHZ_LEGACY> {};
template <> struct ChipsetTraits<Chipset::GW6205_800> : ChipsetTraits<TIMING_GW6205_800KHZ> {};
template <> struct ChipsetTraits<Chipset::DP1903_800> : ChipsetTraits<TIMING_DP1903_800KHZ> {};

/// RGBW Chipsets
template <> struct ChipsetTraits<Chipset::TM1814> : ChipsetTraits<TIMING_TM1814> {};

/// UCS7604 Special 16-Bit RGBW Controller
template <> struct ChipsetTraits<Chipset::UCS7604_800> : ChipsetTraits<TIMING_UCS7604_800KHZ> {};
template <> struct ChipsetTraits<Chipset::UCS7604_1600> : ChipsetTraits<TIMING_UCS7604_1600KHZ> {};

/// Trait to get human-readable peripheral name for diagnostic messages
///
/// This trait is used by the CPU fallback implementation to display the specific
/// peripheral name in FL_WARN messages when the peripheral is unavailable.
///
/// Example: "LCD_I80 peripheral not available on this platform, using CPU fallback"
template <typename PERIPHERAL> struct PeripheralName;

template <> struct PeripheralName<LCD_I80> {
    static const char* name() { return "LCD_I80"; }
};

template <> struct PeripheralName<RMT> {
    static const char* name() { return "RMT"; }
};

template <> struct PeripheralName<I2S> {
    static const char* name() { return "I2S"; }
};

template <> struct PeripheralName<SPI_BULK> {
    static const char* name() { return "SPI"; }
};

template <> struct PeripheralName<OFLED> {
    static const char* name() { return "OFLED"; }
};

} // namespace fl
