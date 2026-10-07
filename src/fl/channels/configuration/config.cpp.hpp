// ok no header - public declarations remain in fl/channels/config.h

/// @file config.cpp
/// @brief Implementation of ChannelConfig and MultiChannelConfig

#include "fl/channels/config.h"

namespace fl {

// ========== New Variant-Based Constructors ==========

ChannelConfig::ChannelConfig(const fl::string& name, const ChipsetVariant& chipset,
                              fl::span<CRGB> leds, EOrder rgbOrder, const ChannelOptions& options) FL_NO_EXCEPT
    : ChannelConfig(chipset, leds, rgbOrder, options) {
    mName = name;
}

ChannelConfig::ChannelConfig(const ChipsetVariant& chipset, fl::span<CRGB> leds,
                              EOrder rgbOrder, const ChannelOptions& options) FL_NO_EXCEPT
    : chipset(chipset)
    , mLeds(leds)
    , rgb_order(rgbOrder)
    , options(options) {}

ChannelConfig::ChannelConfig(const ClocklessChipset& clockless, fl::span<CRGB> leds,
                              EOrder rgbOrder, const ChannelOptions& options) FL_NO_EXCEPT
    : chipset(clockless)
    , mLeds(leds)
    , rgb_order(rgbOrder)
    , options(options) {}

ChannelConfig::ChannelConfig(const SpiChipsetConfig& spi, fl::span<CRGB> leds,
                              EOrder rgbOrder, const ChannelOptions& options) FL_NO_EXCEPT
    : chipset(spi)
    , mLeds(leds)
    , rgb_order(rgbOrder)
    , options(options) {}

// ========== Backwards-Compatible Constructors ==========

ChannelConfig::ChannelConfig(int pin, const ChipsetTimingConfig& timing, fl::span<CRGB> leds,
                              EOrder rgbOrder, const ChannelOptions& options) FL_NO_EXCEPT
    : chipset(ClocklessChipset(pin, timing))
    , mLeds(leds)
    , rgb_order(rgbOrder)
    , options(options) {}

ChannelConfig::ChannelConfig(const ChannelConfig& other) FL_NO_EXCEPT
    : chipset(other.chipset)
    , mLeds(other.mLeds)
    , rgb_order(other.rgb_order)
    , options(other.options)
    , mScreenMap(other.mScreenMap)
    , mName(other.mName) {}

ChannelConfig::ChannelConfig(ChannelConfig&& other) FL_NO_EXCEPT
    : chipset(fl::move(other.chipset))
    , mLeds(other.mLeds)
    , rgb_order(other.rgb_order)
    , options(fl::move(other.options))
    , mScreenMap(fl::move(other.mScreenMap))
    , mName(fl::move(other.mName)) {}

// ========== Accessor Methods ==========

int ChannelConfig::getDataPin() const FL_NO_EXCEPT {
    if (const ClocklessChipset* clockless = chipset.ptr<ClocklessChipset>()) {
        return clockless->pin;
    } else if (const SpiChipsetConfig* spi = chipset.ptr<SpiChipsetConfig>()) {
        return spi->dataPin;
    }
    return -1;  // Invalid/empty variant
}

int ChannelConfig::getClockPin() const FL_NO_EXCEPT {
    if (const SpiChipsetConfig* spi = chipset.ptr<SpiChipsetConfig>()) {
        return spi->clockPin;
    }
    return -1;  // Clockless chipsets don't have a clock pin
}

// MultiChannelConfig constructors

MultiChannelConfig::MultiChannelConfig(fl::span<ChannelConfig> channels) FL_NO_EXCEPT {
    mChannels.reserve(channels.size());
    for (const auto& config : channels) {
        mChannels.push_back(fl::make_shared<ChannelConfig>(config));
    }
}

MultiChannelConfig::MultiChannelConfig(fl::initializer_list<ChannelConfig> channels) FL_NO_EXCEPT {
    mChannels.reserve(channels.size());
    for (const auto& config : channels) {
        mChannels.push_back(fl::make_shared<ChannelConfig>(config));
    }
}

MultiChannelConfig& MultiChannelConfig::add(ChannelConfigPtr channel) FL_NO_EXCEPT {
    mChannels.push_back(channel);
    return *this;
}

} // namespace fl
