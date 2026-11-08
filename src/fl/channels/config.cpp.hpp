/// @file config.cpp
/// @brief Implementation of ChannelConfig and MultiChannelConfig

#include "fl/channels/config.h"

namespace fl {

namespace detail {
ChannelConfigMembers::ChannelConfigMembers(fl::span<CRGB> leds, EOrder order) FL_NO_EXCEPT
    : mLeds(leds), rgb_order(order) {}
ChannelConfigMembers::ChannelConfigMembers(fl::span<CRGB> leds, EOrder order,
                                         const ChannelOptions& opts) FL_NO_EXCEPT
    : mLeds(leds), rgb_order(order), options(opts) {}
ChannelConfigMembers::ChannelConfigMembers(const fl::string& name, fl::span<CRGB> leds,
                                         EOrder order) FL_NO_EXCEPT
    : mLeds(leds), rgb_order(order), mName(name) {}
ChannelConfigMembers::ChannelConfigMembers(const fl::string& name, fl::span<CRGB> leds,
                                         EOrder order, const ChannelOptions& opts) FL_NO_EXCEPT
    : mLeds(leds), rgb_order(order), options(opts), mName(name) {}
ChannelConfigMembers::ChannelConfigMembers(const ChannelConfigMembers&) FL_NO_EXCEPT = default;
ChannelConfigMembers::ChannelConfigMembers(ChannelConfigMembers&&) FL_NO_EXCEPT = default;
ChannelConfigMembers& ChannelConfigMembers::operator=(const ChannelConfigMembers&) FL_NO_EXCEPT = default;
ChannelConfigMembers& ChannelConfigMembers::operator=(ChannelConfigMembers&&) FL_NO_EXCEPT = default;
ChannelConfigMembers::~ChannelConfigMembers() FL_NO_EXCEPT = default;
} // namespace detail

// ========== New Variant-Based Constructors ==========

ChannelConfig::ChannelConfig(const fl::string& name, const ChipsetVariant& chipset,
                              fl::span<CRGB> leds, EOrder rgbOrder, const ChannelOptions& options) FL_NO_EXCEPT
    : ChannelConfig(chipset, leds, rgbOrder, options) {
    mName = name;
}

// ========== Backwards-Compatible Constructors ==========

ChannelConfig::ChannelConfig(const ChannelConfig& other) FL_NO_EXCEPT
    : ChannelConfigMembers(other), chipset(other.chipset) {}

ChannelConfig::ChannelConfig(ChannelConfig&& other) FL_NO_EXCEPT
    : ChannelConfigMembers(fl::move(other)), chipset(fl::move(other.chipset)) {}

ChannelConfig::~ChannelConfig() FL_NO_EXCEPT = default;
ChannelConfig& ChannelConfig::operator=(const ChannelConfig&) FL_NO_EXCEPT = default;
ChannelConfig& ChannelConfig::operator=(ChannelConfig&&) FL_NO_EXCEPT = default;

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
