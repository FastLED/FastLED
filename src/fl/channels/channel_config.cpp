/// @file channel_config.cpp
/// @brief Implementation of MultiChannelConfig builder pattern methods

#include "channel_config.h"

namespace fl {

// Constructors
ChannelConfig::ChannelConfig(int pin, const ChipsetTimingConfig& timing, fl::span<CRGB> leds,
                              EOrder rgbOrder, const LEDSettings& settings)
    : pin(pin), timing(timing), mLeds(leds), rgb_order(rgbOrder), settings(settings) {}

ChannelConfig::ChannelConfig(const ChannelConfig& other)
    : pin(other.pin), timing(other.timing), mLeds(other.mLeds), rgb_order(other.rgb_order),
      settings(other.settings) {}

ChannelConfig::ChannelConfig(ChannelConfig&& other)
    : pin(other.pin), timing(fl::move(other.timing)), mLeds(other.mLeds), rgb_order(other.rgb_order),
      settings(fl::move(other.settings)) {}

// MultiChannelConfig constructors

MultiChannelConfig::MultiChannelConfig(fl::span<ChannelConfig> channels) {
    mChannels.reserve(channels.size());
    for (const auto& config : channels) {
        mChannels.push_back(fl::make_shared<ChannelConfig>(config));
    }
}

MultiChannelConfig::MultiChannelConfig(fl::initializer_list<ChannelConfig> channels) {
    mChannels.reserve(channels.size());
    for (const auto& config : channels) {
        mChannels.push_back(fl::make_shared<ChannelConfig>(config));
    }
}

MultiChannelConfig& MultiChannelConfig::add(ChannelConfigPtr channel) {
    mChannels.push_back(channel);
    return *this;
}

} // namespace fl
