#pragma once

#include "crgb.h"
#include "fl/rgbw.h"
#include "fl/span.h"
#include "fl/vector.h"
#include "fl/ptr.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "color.h"
#include "dither_mode.h"

namespace fl {

/// @brief Configuration for a single LED channel
///
/// Contains all settings typically configured via FastLED.addLeds<>().set...() methods:
/// - LED data array and count
/// - Chipset timing configuration
/// - Color correction and temperature
/// - Dithering mode
/// - RGBW conversion settings
struct ChannelConfig {
    // Basic constructor with timing, leds, and rgbw
    ChannelConfig(const ChipsetTimingConfig& timing, fl::span<const CRGB> leds,
                  Rgbw rgbw = RgbwInvalid::value());

    // Full constructor with all settings
    ChannelConfig(const ChipsetTimingConfig& timing, fl::span<const CRGB> leds,
                  Rgbw rgbw, CRGB correction = UncorrectedColor,
                  CRGB temperature = UncorrectedTemperature,
                  fl::u8 ditherMode = BINARY_DITHER);

    // Copy constructor (needed because of const member)
    ChannelConfig(const ChannelConfig& other);

    // Move constructor (needed because of const member)
    ChannelConfig(ChannelConfig&& other);

    // Note: Assignment operators deleted because const member mLeds cannot be reassigned
    ChannelConfig& operator=(const ChannelConfig&) = delete;
    ChannelConfig& operator=(ChannelConfig&&) = delete;

    // Chipset timing
    const ChipsetTimingConfig timing;

    // LED data
    const fl::span<const CRGB> mLeds;

    // RGBW conversion
    Rgbw rgbw = RgbwInvalid::value();        ///< RGBW conversion settings (default: RGB mode)

    // Color adjustments (common FastLED.addLeds<>().set...() options)
    CRGB correction = UncorrectedColor;      ///< Color correction (e.g., TypicalLEDStrip, TypicalSMD5050)
    CRGB temperature = UncorrectedTemperature; ///< Color temperature/white point
    fl::u8 ditherMode = BINARY_DITHER;       ///< Dithering mode for smoother color transitions

};

FASTLED_SHARED_PTR_STRUCT(ChannelConfig);

/// @brief Multi-channel LED configuration with builder pattern support
///
/// Stores shared pointers to ChannelConfig objects, allowing external modifications
/// to affect the actual LED strips. All setter methods apply to all channels and
/// return a reference for chaining.
///
/// @example
/// ```cpp
/// MultiChannelConfig config;
/// auto channel1 = fl::make_shared<ChannelConfig>();
/// auto channel2 = fl::make_shared<ChannelConfig>();
/// config.add(channel1);
/// config.add(channel2);
/// config.setCorrection(TypicalLEDStrip)
///       .setTemperature(Tungsten100W)
///       .setDither(BINARY_DITHER);
/// // External modifications also work:
/// channel1->correction = CRGB(255, 200, 200);
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

    /// Set color correction for all channels
    /// @param correction the color correction to apply (e.g., TypicalLEDStrip, TypicalSMD5050)
    /// @returns reference to this object for method chaining
    MultiChannelConfig& setCorrection(CRGB correction);

    /// @copydoc setCorrection(CRGB)
    MultiChannelConfig& setCorrection(LEDColorCorrection correction);

    /// Set color temperature for all channels
    /// @param temperature the color temperature/white point to apply
    /// @returns reference to this object for method chaining
    MultiChannelConfig& setTemperature(CRGB temperature);

    /// @copydoc setTemperature(CRGB)
    MultiChannelConfig& setTemperature(ColorTemperature temperature);

    /// Set dithering mode for all channels
    /// @param ditherMode the dithering mode to apply (default: BINARY_DITHER)
    /// @returns reference to this object for method chaining
    MultiChannelConfig& setDither(fl::u8 ditherMode = BINARY_DITHER);

    /// Set RGBW conversion settings for all channels
    /// @param rgbw the RGBW conversion settings to apply
    /// @returns reference to this object for method chaining
    MultiChannelConfig& setRgbw(const Rgbw& rgbw = RgbwDefault::value());

    /// Vector of shared pointers to channel configurations
    fl::vector<ChannelConfigPtr> mChannels;
};

}
