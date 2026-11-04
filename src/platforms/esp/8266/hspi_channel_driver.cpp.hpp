// IWYU pragma: private

/// @file platforms/esp/8266/hspi_channel_driver.cpp.hpp
/// @brief Implementation of the ESP8266 hardware HSPI channel driver.

#include "platforms/esp/8266/hspi_channel_driver.h"
#include "platforms/is_platform.h"

#if defined(FL_IS_ESP8266)

#include "fl/channels/config.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/noexcept.h"
#include "platforms/esp/8266/hspi_regs.h"
#include "platforms/esp/8266/spi_hspi_clock.h"

namespace fl {

Esp8266HspiChannelDriver::Esp8266HspiChannelDriver() FL_NO_EXCEPT {}
Esp8266HspiChannelDriver::~Esp8266HspiChannelDriver() = default;

bool Esp8266HspiChannelDriver::canHandle(
    const ChannelDataPtr& data) const FL_NO_EXCEPT {
    if (!data || !data->isSpi()) {
        return false;
    }
    // The peripheral pins are fixed: only the HSPI MOSI/SCK pad pair (GPIO13 =
    // data, GPIO14 = clock, i.e. the default raw pin order) is served here.
    // Every other SPI pin pair falls through to the shared bit-bang bus.
    const auto* cfg = data->getChipset().ptr<SpiChipsetConfig>();
    return cfg != nullptr && cfg->dataPin == 13 && cfg->clockPin == 14;
}

void Esp8266HspiChannelDriver::enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT {
    if (channelData) {
        mEnqueuedChannels.push_back(fl::move(channelData));
    }
}

IChannelDriver::DriverState Esp8266HspiChannelDriver::poll() FL_NO_EXCEPT {
    return DriverState(DriverState::READY);
}

fl::string Esp8266HspiChannelDriver::getName() const FL_NO_EXCEPT {
    return fl::string::from_literal("SPI");
}

IChannelDriver::Capabilities Esp8266HspiChannelDriver::getCapabilities()
    const FL_NO_EXCEPT {
    return Capabilities(/*supportsClockless=*/false, /*supportsSpi=*/true);
}

void Esp8266HspiChannelDriver::transmit(
    const ChannelDataPtr& channel) FL_NO_EXCEPT {
    if (!channel) {
        return;
    }
    const auto* spiCfg = channel->getChipset().ptr<SpiChipsetConfig>();
    if (!spiCfg) {
        return;
    }

    esp8266::hspiConfigure(esp8266::hspiClockFor(spiCfg->timing.clock_hz));

    const fl::vector_psram<fl::u8>& bytes = channel->getData();
    const fl::size total = bytes.size();
    fl::size offset = 0;
    while (offset < total) {
        const fl::size remaining = total - offset;
        const fl::u16 chunk = static_cast<fl::u16>(
            remaining < esp8266::kHspiMaxBatchBytes ? remaining : esp8266::kHspiMaxBatchBytes);
        esp8266::hspiWriteChunk(&bytes[offset], chunk);
        offset += chunk;
    }
}

void Esp8266HspiChannelDriver::show() FL_NO_EXCEPT {
    if (mEnqueuedChannels.empty()) {
        return;
    }

    mTransmittingChannels = fl::move(mEnqueuedChannels);
    mEnqueuedChannels.clear();

    for (fl::size i = 0; i < mTransmittingChannels.size(); ++i) {
        if (mTransmittingChannels[i]) {
            mTransmittingChannels[i]->setInUse(true);
        }
    }

    for (fl::size i = 0; i < mTransmittingChannels.size(); ++i) {
        transmit(mTransmittingChannels[i]);
    }

    for (fl::size i = 0; i < mTransmittingChannels.size(); ++i) {
        if (mTransmittingChannels[i]) {
            mTransmittingChannels[i]->setInUse(false);
        }
    }
    mTransmittingChannels.clear();
}

}  // namespace fl

#endif  // FL_IS_ESP8266
