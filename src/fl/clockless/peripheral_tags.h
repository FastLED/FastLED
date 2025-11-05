#pragma once

/// @file fl/clockless/peripheral_tags.h
/// @brief Peripheral type tags and traits for BulkClockless controllers
///
/// This header is the single source of truth for peripheral type markers (LCD_I80, RMT, I2S, SPI_BULK),
/// chipset types (WS2812_CHIPSET), and associated traits (PeripheralName, ChipsetTraits)
/// used by the BulkClockless API.

#include "fl/chipsets/timing_traits.h"
#include "fl/chipsets/led_timing.h"

namespace fl {

/// Peripheral type tags
/// These are empty structs used as template parameters to select peripheral implementations

struct LCD_I80 {};  ///< LCD I80 parallel interface peripheral (ESP32-S3, ESP32-P4)
struct RMT {};      ///< Remote Control Transceiver peripheral (ESP32, ESP32-S3, ESP32-C3/C6/H2)
struct I2S {};      ///< I2S audio interface repurposed for LED output (ESP32, ESP32-S3)
struct SPI_BULK {}; ///< SPI peripheral for bulk LED output

/// Chipset type definitions
/// These define the LED chipset timing characteristics

/// Chipset trait for WS2812 (extract timing information)
template <typename CHIPSET> struct ChipsetTraits;

/// Specialization for WS2812 timing
template <> struct ChipsetTraits<struct WS2812_CHIPSET> {
    using TimingType = TIMING_WS2812_800KHZ;
    static constexpr uint32_t T1 = TimingType::T1;
    static constexpr uint32_t T2 = TimingType::T2;
    static constexpr uint32_t T3 = TimingType::T3;
    static constexpr uint32_t RESET = TimingType::RESET;

    static constexpr ChipsetTiming runtime_timing() {
        return {T1, T2, T3, RESET, "WS2812"};
    }
};

/// WS2812 chipset type for BulkClockless
struct WS2812_CHIPSET {
    using Timing = TIMING_WS2812_800KHZ;
};

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

} // namespace fl
