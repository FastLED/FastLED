/// @file channel_config.cpp
/// @brief Implementation of MultiChannelConfig builder pattern methods

#include "channel_config.h"

namespace fl {

// ChannelConfig constructors

ChannelConfig::ChannelConfig(const ChipsetTimingConfig& timing, fl::span<const CRGB> leds,
                              Rgbw rgbw)
    : timing(timing), mLeds(leds), rgbw(rgbw) {}

ChannelConfig::ChannelConfig(const ChipsetTimingConfig& timing, fl::span<const CRGB> leds,
                              Rgbw rgbw, CRGB correction, CRGB temperature,
                              fl::u8 ditherMode)
    : timing(timing), mLeds(leds), rgbw(rgbw), correction(correction),
      temperature(temperature), ditherMode(ditherMode) {}

ChannelConfig::ChannelConfig(const ChannelConfig& other)
    : timing(other.timing), mLeds(other.mLeds), rgbw(other.rgbw), correction(other.correction),
      temperature(other.temperature), ditherMode(other.ditherMode) {}

ChannelConfig::ChannelConfig(ChannelConfig&& other)
    : timing(fl::move(other.timing)), mLeds(other.mLeds), rgbw(fl::move(other.rgbw)),
      correction(other.correction), temperature(other.temperature), ditherMode(other.ditherMode) {}

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

MultiChannelConfig& MultiChannelConfig::setCorrection(CRGB correction) {
    for (auto& channel : mChannels) {
        if (channel) {
            channel->correction = correction;
        }
    }
    return *this;
}

MultiChannelConfig& MultiChannelConfig::setCorrection(LEDColorCorrection correction) {
    return setCorrection(CRGB(correction));
}

MultiChannelConfig& MultiChannelConfig::setTemperature(CRGB temperature) {
    for (auto& channel : mChannels) {
        if (channel) {
            channel->temperature = temperature;
        }
    }
    return *this;
}

MultiChannelConfig& MultiChannelConfig::setTemperature(ColorTemperature temperature) {
    return setTemperature(CRGB(temperature));
}

MultiChannelConfig& MultiChannelConfig::setDither(fl::u8 ditherMode) {
    for (auto& channel : mChannels) {
        if (channel) {
            channel->ditherMode = ditherMode;
        }
    }
    return *this;
}

MultiChannelConfig& MultiChannelConfig::setRgbw(const Rgbw& rgbw) {
    for (auto& channel : mChannels) {
        if (channel) {
            channel->rgbw = rgbw;
        }
    }
    return *this;
}

} // namespace fl
