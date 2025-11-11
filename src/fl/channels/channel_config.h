#pragma once

#include "crgb.h"
#include "fl/rgbw.h"
#include "fl/span.h"
#include "fl/vector.h"
#include "fl/ptr.h"
#include "fl/screenmap.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/eorder.h"
#include "fl/led_settings.h"
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

    // Template constructor with TIMING type
    template<typename TIMING>
    ChannelConfig(int pin, fl::span<CRGB> leds, EOrder rgbOrder = RGB,
                  const LEDSettings& settings = LEDSettings())
        : ChannelConfig(pin, makeTimingConfig<TIMING>(), leds, rgbOrder, settings) {}

    // Basic constructor with timing, leds, rgb_order, and LEDSettings
    ChannelConfig(int pin, const ChipsetTimingConfig& timing, fl::span<CRGB> leds,
                  EOrder rgbOrder = RGB, const LEDSettings& settings = LEDSettings());

    // Copy constructor (needed because of const members)
    ChannelConfig(const ChannelConfig& other);

    // Move constructor (needed because of const members)
    ChannelConfig(ChannelConfig&& other);

    // Note: Assignment operators deleted because const members cannot be reassigned
    ChannelConfig& operator=(const ChannelConfig&) = delete;
    ChannelConfig& operator=(ChannelConfig&&) = delete;

    // GPIO pin
    const int pin;

    // Chipset timing
    const ChipsetTimingConfig timing;

    // LED data
    fl::span<CRGB> mLeds;

    // RGB channel ordering
    EOrder rgb_order = RGB;

    // LED settings (correction, temperature, dither, rgbw)
    LEDSettings settings;

    // Screen mapping
    fl::ScreenMap mScreenMap;
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
