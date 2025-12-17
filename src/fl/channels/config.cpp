/// @file config.cpp
/// @brief Implementation of ChannelConfig and MultiChannelConfig

#include "fl/channels/config.h"

namespace fl {

// ========== New Variant-Based Constructors ==========

ChannelConfig::ChannelConfig(const ChipsetVariant& chipset, fl::span<CRGB> leds,
                              EOrder rgbOrder, const ChannelOptions& options)
    : chipset(chipset)
    , mLeds(leds)
    , rgb_order(rgbOrder)
    , options(options)
    , pin(getDataPin())  // Initialize deprecated member for backwards compatibility
    , timing(isClockless() ? getClocklessChipset()->timing : ChipsetTimingConfig(0, 0, 0, 0)) {}

ChannelConfig::ChannelConfig(const ClocklessChipset& clockless, fl::span<CRGB> leds,
                              EOrder rgbOrder, const ChannelOptions& options)
    : chipset(clockless)
    , mLeds(leds)
    , rgb_order(rgbOrder)
    , options(options)
    , pin(clockless.pin)
    , timing(clockless.timing) {}

ChannelConfig::ChannelConfig(const SpiChipsetConfig& spi, fl::span<CRGB> leds,
                              EOrder rgbOrder, const ChannelOptions& options)
    : chipset(spi)
    , mLeds(leds)
    , rgb_order(rgbOrder)
    , options(options)
    , pin(spi.dataPin)
    , timing(0, 0, 0, 0) {}  // SPI chipsets don't use T1/T2/T3 timing

// ========== Backwards-Compatible Constructors ==========

ChannelConfig::ChannelConfig(int pin, const ChipsetTimingConfig& timing, fl::span<CRGB> leds,
                              EOrder rgbOrder, const ChannelOptions& options)
    : chipset(ClocklessChipset(pin, timing))
    , mLeds(leds)
    , rgb_order(rgbOrder)
    , options(options)
    , pin(pin)
    , timing(timing) {}

ChannelConfig::ChannelConfig(const ChannelConfig& other)
    : chipset(other.chipset)
    , mLeds(other.mLeds)
    , rgb_order(other.rgb_order)
    , options(other.options)
    , mScreenMap(other.mScreenMap)
    , pin(other.pin)
    , timing(other.timing) {}

ChannelConfig::ChannelConfig(ChannelConfig&& other)
    : chipset(fl::move(other.chipset))
    , mLeds(other.mLeds)
    , rgb_order(other.rgb_order)
    , options(fl::move(other.options))
    , mScreenMap(fl::move(other.mScreenMap))
    , pin(other.pin)
    , timing(other.timing) {}

// ========== Accessor Methods ==========

int ChannelConfig::getDataPin() const {
    if (const ClocklessChipset* clockless = chipset.ptr<ClocklessChipset>()) {
        return clockless->pin;
    } else if (const SpiChipsetConfig* spi = chipset.ptr<SpiChipsetConfig>()) {
        return spi->dataPin;
    }
    return -1;  // Invalid/empty variant
}

int ChannelConfig::getClockPin() const {
    if (const SpiChipsetConfig* spi = chipset.ptr<SpiChipsetConfig>()) {
        return spi->clockPin;
    }
    return -1;  // Clockless chipsets don't have a clock pin
}

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
