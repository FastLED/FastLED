#pragma once

#include "crgb.h"
#include "fl/rgbw.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/variant.h"
#include "fl/screenmap.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/chipsets/spi.h"
#include "fl/eorder.h"
#include "fl/channels/options.h"
#include "color.h"
#include "dither_mode.h"

namespace fl {

/// @brief Clockless chipset configuration (single data pin)
///
/// Used for timing-sensitive LED protocols like WS2812, SK6812, APA106, etc.
/// These chipsets encode data using precise nanosecond timing on a single data line.
struct ClocklessChipset {
    int pin;                        ///< GPIO data pin
    ChipsetTimingConfig timing;     ///< T1/T2/T3 timing parameters

    /// @brief Constructor
    ClocklessChipset(int pin, const ChipsetTimingConfig& timing)
        : pin(pin), timing(timing) {}

    /// @brief Default constructor
    ClocklessChipset() : pin(-1), timing(0, 0, 0, 0) {}

    /// @brief Copy constructor
    ClocklessChipset(const ClocklessChipset&) = default;

    /// @brief Move constructor
    ClocklessChipset(ClocklessChipset&&) = default;

    /// @brief Copy assignment
    ClocklessChipset& operator=(const ClocklessChipset&) = default;

    /// @brief Move assignment
    ClocklessChipset& operator=(ClocklessChipset&&) = default;

    /// @brief Equality operator
    bool operator==(const ClocklessChipset& other) const {
        return pin == other.pin &&
               timing.t1_ns == other.timing.t1_ns &&
               timing.t2_ns == other.timing.t2_ns &&
               timing.t3_ns == other.timing.t3_ns &&
               timing.reset_us == other.timing.reset_us;
    }

    /// @brief Inequality operator
    bool operator!=(const ClocklessChipset& other) const {
        return !(*this == other);
    }
};

/// @brief SPI chipset configuration (data + clock pins)
///
/// Used for clock-based LED protocols like APA102, SK9822, HD108, WS2801, etc.
/// These chipsets use explicit clock and data lines for synchronous transmission.
struct SpiChipsetConfig {
    int dataPin;                    ///< GPIO data pin (MOSI)
    int clockPin;                   ///< GPIO clock pin (SCK)
    SpiEncoder timing;              ///< SPI encoder configuration

    /// @brief Constructor
    SpiChipsetConfig(int dataPin, int clockPin, const SpiEncoder& timing)
        : dataPin(dataPin), clockPin(clockPin), timing(timing) {}

    /// @brief Default constructor (requires explicit protocol specification)
    /// @note Protocol defaults to APA102 as a reasonable fallback
    SpiChipsetConfig() : dataPin(-1), clockPin(-1), timing{fl::SpiChipset::APA102, 6000000} {}

    /// @brief Copy constructor
    SpiChipsetConfig(const SpiChipsetConfig&) = default;

    /// @brief Move constructor
    SpiChipsetConfig(SpiChipsetConfig&&) = default;

    /// @brief Copy assignment
    SpiChipsetConfig& operator=(const SpiChipsetConfig&) = default;

    /// @brief Move assignment
    SpiChipsetConfig& operator=(SpiChipsetConfig&&) = default;

    /// @brief Equality operator
    bool operator==(const SpiChipsetConfig& other) const {
        return dataPin == other.dataPin &&
               clockPin == other.clockPin &&
               timing == other.timing;
    }

    /// @brief Inequality operator
    bool operator!=(const SpiChipsetConfig& other) const {
        return !(*this == other);
    }
};

/// @brief Variant type that holds either a clockless or SPI chipset configuration
///
/// This allows ChannelConfig to support both chipset types with compile-time type safety
/// and runtime polymorphism via the visitor pattern.
///
/// @example
/// ```cpp
/// // Clockless chipset (WS2812)
/// ChipsetVariant clockless = ClocklessChipset(5, makeTimingConfig<TIMING_WS2812_800KHZ>());
///
/// // SPI chipset (APA102)
/// ChipsetVariant spi = SpiChipsetConfig(23, 18, SpiEncoder::apa102());
/// ```
using ChipsetVariant = fl::variant<ClocklessChipset, SpiChipsetConfig>;

/// @brief Configuration for a single LED channel
///
/// Contains all settings typically configured via FastLED.addLeds<>().set...() methods:
/// - LED data array and count
/// - Chipset configuration (clockless or SPI)
/// - Color correction and temperature
/// - Dithering mode
/// - RGBW conversion settings
struct ChannelConfig {

    // ========== New Variant-Based Constructors ==========

    /// @brief Primary constructor with chipset variant
    /// @param chipset Chipset configuration (clockless or SPI)
    /// @param leds LED data array
    /// @param rgbOrder RGB channel ordering
    /// @param options Channel options (correction, temperature, dither, affinity)
    ChannelConfig(const ChipsetVariant& chipset, fl::span<CRGB> leds,
                  EOrder rgbOrder = RGB, const ChannelOptions& options = ChannelOptions());

    /// @brief Constructor with clockless chipset
    /// @param clockless Clockless chipset configuration
    /// @param leds LED data array
    /// @param rgbOrder RGB channel ordering
    /// @param options Channel options
    ChannelConfig(const ClocklessChipset& clockless, fl::span<CRGB> leds,
                  EOrder rgbOrder = RGB, const ChannelOptions& options = ChannelOptions());

    /// @brief Constructor with SPI chipset
    /// @param spi SPI chipset configuration
    /// @param leds LED data array
    /// @param rgbOrder RGB channel ordering
    /// @param options Channel options
    ChannelConfig(const SpiChipsetConfig& spi, fl::span<CRGB> leds,
                  EOrder rgbOrder = RGB, const ChannelOptions& options = ChannelOptions());

    // ========== Backwards-Compatible Constructors (Deprecated) ==========

    /// @brief Template constructor with TIMING type (backwards compatibility)
    /// @deprecated Use ClocklessChipset constructor instead
    /// @note This constructor creates a ClocklessChipset internally
    template<typename TIMING>
    ChannelConfig(int pin, fl::span<CRGB> leds, EOrder rgbOrder = RGB,
                  const ChannelOptions& options = ChannelOptions())
        : ChannelConfig(ClocklessChipset(pin, makeTimingConfig<TIMING>()), leds, rgbOrder, options) {}

    /// @brief Basic constructor with timing (backwards compatibility)
    /// @deprecated Use ClocklessChipset constructor instead
    /// @note This constructor creates a ClocklessChipset internally
    ChannelConfig(int pin, const ChipsetTimingConfig& timing, fl::span<CRGB> leds,
                  EOrder rgbOrder = RGB, const ChannelOptions& options = ChannelOptions());

    // Copy constructor (needed because of variant)
    ChannelConfig(const ChannelConfig& other);

    // Move constructor (needed because of variant)
    ChannelConfig(ChannelConfig&& other);

    // Note: Assignment operators deleted because const members cannot be reassigned
    ChannelConfig& operator=(const ChannelConfig&) = delete;
    ChannelConfig& operator=(ChannelConfig&&) = delete;

    // ========== Accessors ==========

    /// @brief Get the chipset configuration variant
    const ChipsetVariant& getChipset() const { return chipset; }

    /// @brief Check if this is a clockless chipset
    bool isClockless() const { return chipset.is<ClocklessChipset>(); }

    /// @brief Check if this is an SPI chipset
    bool isSpi() const { return chipset.is<SpiChipsetConfig>(); }

    /// @brief Get clockless chipset (returns nullptr if not clockless)
    const ClocklessChipset* getClocklessChipset() const { return chipset.ptr<ClocklessChipset>(); }

    /// @brief Get SPI chipset (returns nullptr if not SPI)
    const SpiChipsetConfig* getSpiChipset() const { return chipset.ptr<SpiChipsetConfig>(); }

    /// @brief Get data pin (works for both clockless and SPI)
    int getDataPin() const;

    /// @brief Get clock pin (returns -1 for clockless chipsets)
    int getClockPin() const;

    // ========== Data Members ==========

    /// Chipset configuration (clockless or SPI)
    ChipsetVariant chipset;

    /// LED data array
    fl::span<CRGB> mLeds;

    /// RGB channel ordering
    EOrder rgb_order = RGB;

    /// Optional channel settings (correction, temperature, dither, rgbw, affinity)
    ChannelOptions options;

    /// Screen mapping
    fl::ScreenMap mScreenMap;

    // ========== Deprecated Members (for compatibility during migration) ==========

    /// @deprecated Use getDataPin() instead
    /// @brief GPIO pin (clockless chipsets only)
    /// @note This is maintained for backwards compatibility with existing code
    const int pin;

    /// @deprecated Use getChipset() instead
    /// @brief Chipset timing (clockless chipsets only)
    /// @note This is maintained for backwards compatibility with existing code
    const ChipsetTimingConfig timing;
};

FASTLED_SHARED_PTR_STRUCT(ChannelConfig);

/// @brief Multi-channel LED configuration
///
/// Stores shared pointers to ChannelConfig objects for managing multiple channels.
///
/// @example
/// ```cpp
/// MultiChannelConfig config;
/// auto channel1 = fl::make_shared<ChannelConfig>(pin1, timing);
/// auto channel2 = fl::make_shared<ChannelConfig>(pin2, timing);
/// config.add(channel1);
/// config.add(channel2);
/// ```
struct MultiChannelConfig {
    MultiChannelConfig() = default;
    MultiChannelConfig(const MultiChannelConfig&) = default;
    MultiChannelConfig(MultiChannelConfig&&) = default;

    MultiChannelConfig(fl::span<ChannelConfigPtr> channels) : mChannels(channels.begin(), channels.end()) {}
    MultiChannelConfig(fl::initializer_list<ChannelConfigPtr> channels) : mChannels(channels.begin(), channels.end()) {}

    /// @brief Construct from span of ChannelConfig (copies to shared_ptr internally)
    /// @param channels Span of ChannelConfig objects to copy
    MultiChannelConfig(fl::span<ChannelConfig> channels);

    /// @brief Construct from initializer list of ChannelConfig (copies to shared_ptr internally)
    /// @param channels Initializer list of ChannelConfig objects to copy
    MultiChannelConfig(fl::initializer_list<ChannelConfig> channels);

    MultiChannelConfig& operator=(const MultiChannelConfig&) = default;
    MultiChannelConfig& operator=(MultiChannelConfig&&) = default;
    ~MultiChannelConfig() = default;

    /// Add a channel configuration to the multi-channel config
    /// @param channel shared pointer to the channel configuration to add
    /// @returns reference to this object for method chaining
    MultiChannelConfig& add(ChannelConfigPtr channel);

    /// Vector of shared pointers to channel configurations
    fl::vector<ChannelConfigPtr> mChannels;
};

}
