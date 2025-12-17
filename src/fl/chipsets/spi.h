/// @file fl/chipsets/spi.h
/// @brief SPI encoder configuration for clocked LED chipsets
///
/// Defines encoder parameters for SPI-based LED protocols (APA102, SK9822, etc.)
/// Unlike clockless chipsets which use nanosecond-precise T1/T2/T3 timing,
/// SPI chipsets use clock-synchronized data transmission.

#pragma once

#include "fl/stl/stdint.h"
#include "fl/chipsets/spi_chipsets.h"

namespace fl {

/// @brief SPI encoder configuration for LED protocols
///
/// Unlike clockless protocols (WS2812, SK6812) which use precise nanosecond
/// timing phases, SPI protocols are clock-based with explicit data and clock lines.
///
/// This configuration contains only the essential parameters - protocol type determines
/// all encoding details (frame structure, brightness support, byte ordering, etc.).
///
/// **Native SPI protocols** (APA102, SK9822, HD108, WS2801, LPD6803, etc.)
/// - True clocked protocols with data + clock pins
/// - Direct byte transmission (no bit-pattern encoding)
/// - Typical clock rates: 1-40 MHz
///
/// **Supported chipsets:**
/// - APA102/DotStar - 4-wire SPI with global brightness per LED (default 6 MHz)
/// - SK9822 - Similar to APA102 with different end frame (default 12 MHz)
/// - HD108 - High-definition 16-bit SPI chipset (default 25 MHz)
/// - WS2801 - 3-wire SPI protocol (default 1 MHz)
/// - WS2803 - Variant of WS2801 (default 25 MHz)
/// - LPD6803 - Older 16-bit 5-5-5 RGB protocol (default 12 MHz)
/// - LPD8806 - 7-bit color depth SPI protocol (default 12 MHz)
/// - P9813 - SPI protocol with checksum header (default 10 MHz)
struct SpiEncoder {
    SpiChipset chipset;          ///< LED chipset type (determines all encoding behavior)
    uint32_t clock_hz;           ///< SPI clock frequency in Hz (e.g., 6000000 for 6MHz)

    /// @brief Create APA102 encoder configuration
    /// @param clock_hz Clock frequency (default 6MHz)
    static inline SpiEncoder apa102(uint32_t clock_hz = 6000000) {
        SpiEncoder config = {SpiChipset::APA102, clock_hz};
        return config;
    }

    /// @brief Create SK9822 encoder configuration
    /// @param clock_hz Clock frequency (default 12MHz)
    static inline SpiEncoder sk9822(uint32_t clock_hz = 12000000) {
        SpiEncoder config = {SpiChipset::SK9822, clock_hz};
        return config;
    }

    /// @brief Equality operator (required for hash map key)
    constexpr bool operator==(const SpiEncoder& other) const {
        return chipset == other.chipset &&
               clock_hz == other.clock_hz;
    }

    /// @brief Inequality operator
    constexpr bool operator!=(const SpiEncoder& other) const {
        return !(*this == other);
    }
};

} // namespace fl
