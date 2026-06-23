/// @file fl/chipsets/spi.h
/// @brief SPI encoder configuration for clocked LED chipsets
///
/// Defines encoder parameters for SPI-based LED protocols (APA102, SK9822, etc.)
/// Unlike clockless chipsets which use nanosecond-precise T1/T2/T3 timing,
/// SPI chipsets use clock-synchronized data transmission.

#pragma once

#include "fl/stl/stdint.h"
#include "fl/chipsets/spi_chipsets.h"
#include "fl/stl/noexcept.h"

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
    u32 clock_hz;           ///< SPI clock frequency in Hz (e.g., 6000000 for 6MHz)

    /// @brief Create APA102 encoder configuration
    /// @param clock_hz Clock frequency (default 6MHz)
    static inline SpiEncoder apa102(u32 clock_hz = 6000000) FL_NOEXCEPT {
        SpiEncoder config = {SpiChipset::APA102, clock_hz};
        return config;
    }

    /// @brief Create APA102HD encoder configuration (per-LED brightness via HD gamma)
    /// @param clock_hz Clock frequency (default 6MHz, same as APA102)
    static inline SpiEncoder apa102HD(u32 clock_hz = 6000000) FL_NOEXCEPT {
        SpiEncoder config = {SpiChipset::APA102HD, clock_hz};
        return config;
    }

    /// @brief Create SK9822 encoder configuration
    /// @param clock_hz Clock frequency (default 12MHz)
    static inline SpiEncoder sk9822(u32 clock_hz = 12000000) FL_NOEXCEPT {
        SpiEncoder config = {SpiChipset::SK9822, clock_hz};
        return config;
    }

    /// @brief Create SK9822HD encoder configuration
    /// @param clock_hz Clock frequency (default 12MHz)
    static inline SpiEncoder sk9822HD(u32 clock_hz = 12000000) FL_NOEXCEPT {
        SpiEncoder config = {SpiChipset::SK9822HD, clock_hz};
        return config;
    }

    /// @brief Create DOTSTAR encoder configuration (alias for APA102)
    /// @param clock_hz Clock frequency (default 6MHz)
    static inline SpiEncoder dotstar(u32 clock_hz = 6000000) FL_NOEXCEPT {
        SpiEncoder config = {SpiChipset::DOTSTAR, clock_hz};
        return config;
    }

    /// @brief Create DOTSTARHD encoder configuration (alias for APA102HD)
    /// @param clock_hz Clock frequency (default 6MHz)
    static inline SpiEncoder dotstarHD(u32 clock_hz = 6000000) FL_NOEXCEPT {
        SpiEncoder config = {SpiChipset::DOTSTARHD, clock_hz};
        return config;
    }

    /// @brief Create HD107 encoder configuration
    /// @param clock_hz Clock frequency (default 40MHz)
    static inline SpiEncoder hd107(u32 clock_hz = 40000000) FL_NOEXCEPT {
        SpiEncoder config = {SpiChipset::HD107, clock_hz};
        return config;
    }

    /// @brief Create HD107HD encoder configuration
    /// @param clock_hz Clock frequency (default 40MHz)
    static inline SpiEncoder hd107HD(u32 clock_hz = 40000000) FL_NOEXCEPT {
        SpiEncoder config = {SpiChipset::HD107HD, clock_hz};
        return config;
    }

    /// @brief Create HD108 encoder configuration
    /// @param clock_hz Clock frequency (default 25MHz)
    static inline SpiEncoder hd108(u32 clock_hz = 25000000) FL_NOEXCEPT {
        SpiEncoder config = {SpiChipset::HD108, clock_hz};
        return config;
    }

    /// @brief Create WS2801 encoder configuration
    /// @param clock_hz Clock frequency (default 1MHz)
    static inline SpiEncoder ws2801(u32 clock_hz = 1000000) FL_NOEXCEPT {
        SpiEncoder config = {SpiChipset::WS2801, clock_hz};
        return config;
    }

    /// @brief Create WS2803 encoder configuration
    /// @param clock_hz Clock frequency (default 25MHz)
    static inline SpiEncoder ws2803(u32 clock_hz = 25000000) FL_NOEXCEPT {
        SpiEncoder config = {SpiChipset::WS2803, clock_hz};
        return config;
    }

    /// @brief Create P9813 encoder configuration
    /// @param clock_hz Clock frequency (default 10MHz)
    static inline SpiEncoder p9813(u32 clock_hz = 10000000) FL_NOEXCEPT {
        SpiEncoder config = {SpiChipset::P9813, clock_hz};
        return config;
    }

    /// @brief Create LPD8806 encoder configuration
    /// @param clock_hz Clock frequency (default 12MHz)
    static inline SpiEncoder lpd8806(u32 clock_hz = 12000000) FL_NOEXCEPT {
        SpiEncoder config = {SpiChipset::LPD8806, clock_hz};
        return config;
    }

    /// @brief Create LPD6803 encoder configuration
    /// @param clock_hz Clock frequency (default 12MHz)
    static inline SpiEncoder lpd6803(u32 clock_hz = 12000000) FL_NOEXCEPT {
        SpiEncoder config = {SpiChipset::LPD6803, clock_hz};
        return config;
    }

    /// @brief Create SM16716 encoder configuration
    /// @param clock_hz Clock frequency (default 16MHz)
    static inline SpiEncoder sm16716(u32 clock_hz = 16000000) FL_NOEXCEPT {
        SpiEncoder config = {SpiChipset::SM16716, clock_hz};
        return config;
    }

    /// @brief Look up the default SpiEncoder for a given chipset
    /// @param chipset The SPI chipset enum value
    /// @param speed_hz_override If nonzero, overrides the chipset's default clock speed
    /// @return SpiEncoder with correct chipset and clock speed
    static inline SpiEncoder spiEncoderForChipset(SpiChipset chipset, u32 speed_hz_override = 0) FL_NOEXCEPT {
        SpiEncoder enc = {chipset, 6000000}; // fallback
        switch (chipset) {
            case SpiChipset::APA102:    enc = apa102(); break;
            case SpiChipset::APA102HD:  enc = apa102HD(); break;
            case SpiChipset::DOTSTAR:   enc = dotstar(); break;
            case SpiChipset::DOTSTARHD: enc = dotstarHD(); break;
            case SpiChipset::SK9822:    enc = sk9822(); break;
            case SpiChipset::SK9822HD:  enc = sk9822HD(); break;
            case SpiChipset::HD107:     enc = hd107(); break;
            case SpiChipset::HD107HD:   enc = hd107HD(); break;
            case SpiChipset::HD108:     enc = hd108(); break;
            case SpiChipset::WS2801:    enc = ws2801(); break;
            case SpiChipset::WS2803:    enc = ws2803(); break;
            case SpiChipset::P9813:     enc = p9813(); break;
            case SpiChipset::LPD8806:   enc = lpd8806(); break;
            case SpiChipset::LPD6803:   enc = lpd6803(); break;
            case SpiChipset::SM16716:   enc = sm16716(); break;
            default: break; // unknown chipset: keep fallback
        }
        if (speed_hz_override != 0) {
            enc.clock_hz = speed_hz_override;
        }
        return enc;
    }

    /// @brief Equality operator (required for hash map key)
    constexpr bool operator==(const SpiEncoder& other) const FL_NOEXCEPT {
        return chipset == other.chipset &&
               clock_hz == other.clock_hz;
    }

    /// @brief Inequality operator
    constexpr bool operator!=(const SpiEncoder& other) const FL_NOEXCEPT {
        return !(*this == other);
    }
};

} // namespace fl
