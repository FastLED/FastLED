// IWYU pragma: private

/// @file platforms/esp/8266/i2s_channel_driver.cpp.hpp
/// @brief Implementation of the ESP8266 I2S clockless output engine.

#include "platforms/esp/8266/i2s_channel_driver.h"
#include "platforms/is_platform.h"

#if defined(FL_IS_ESP8266)

#include "fl/channels/config.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/has_include.h"
#include "fl/stl/noexcept.h"
#include "platforms/esp/8266/i2s_ws2812_codec.h"

// The Arduino core ships the verified I2S implementation (TX + RX). Gate on the
// header: never guess it exists.
#if FL_HAS_INCLUDE(<core_esp8266_i2s.h>)
#define FL_ESP8266_I2S_SUPPORTED

// IWYU pragma: begin_keep
#include <core_esp8266_i2s.h>
// IWYU pragma: end_keep

namespace fl {

namespace {
/// WS2812 wants the line held low for >= 50 us to latch. At the slot rate that
/// is 160 all-zero samples.
constexpr fl::u32 kResetSamples = 160;
}  // namespace

Esp8266I2sChannelDriver::Esp8266I2sChannelDriver() FL_NO_EXCEPT {}
Esp8266I2sChannelDriver::~Esp8266I2sChannelDriver() = default;

bool Esp8266I2sChannelDriver::canHandle(
    const ChannelDataPtr& data) const FL_NO_EXCEPT {
    // I2S emits an arbitrary bit stream, so it drives any clockless chipset.
    return static_cast<bool>(data) && data->isClockless();
}

void Esp8266I2sChannelDriver::enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT {
    if (channelData) {
        mEnqueuedChannels.push_back(fl::move(channelData));
    }
}

IChannelDriver::DriverState Esp8266I2sChannelDriver::poll() FL_NO_EXCEPT {
    return DriverState(DriverState::READY);
}

fl::string Esp8266I2sChannelDriver::getName() const FL_NO_EXCEPT {
    // Matches `busDriverName(Bus::FLEX_IO)` on ESP8266 (the "I2S" bus).
    return fl::string::from_literal("I2S");
}

IChannelDriver::Capabilities Esp8266I2sChannelDriver::getCapabilities()
    const FL_NO_EXCEPT {
    return Capabilities(/*supportsClockless=*/true, /*supportsSpi=*/false);
}

void Esp8266I2sChannelDriver::encodeFrame(
    const fl::u8* ledBytes, fl::size ledCount, fl::u16* outSamples,
    fl::size outCapacity) FL_NO_EXCEPT {
    using namespace fl::esp8266;
    const fl::size capacityBytes = outCapacity / kI2sSamplesPerByte;
    const fl::size bytes = ledCount < capacityBytes ? ledCount : capacityBytes;
    for (fl::size i = 0; i < bytes; ++i) {
        encodeByte(ledBytes[i], &outSamples[i * kI2sSamplesPerByte]);
    }
}

void Esp8266I2sChannelDriver::decodeFrame(
    const fl::u16* samples, fl::size sampleCount, fl::u8* outLedBytes,
    fl::size outCapacity) FL_NO_EXCEPT {
    using namespace fl::esp8266;
    const fl::size capacityBytes = sampleCount / kI2sSamplesPerByte;
    const fl::size bytes = outCapacity < capacityBytes ? outCapacity : capacityBytes;
    for (fl::size i = 0; i < bytes; ++i) {
        outLedBytes[i] = decodeByte(samples[i * kI2sSamplesPerByte],
                                    samples[i * kI2sSamplesPerByte + 1]);
    }
}

void Esp8266I2sChannelDriver::transmit(
    const ChannelDataPtr& channel) FL_NO_EXCEPT {
    if (!channel) {
        return;
    }
    using namespace fl::esp8266;

    // TX only, driving the TX clocks; RX is enabled separately for validation.
    (void)i2s_rxtxdrive_begin(/*enableRx=*/false, /*enableTx=*/true,
                              /*driveRxClocks=*/false, /*driveTxClocks=*/true);
    i2s_set_rate(kI2sSlotHz);
    i2s_set_bits(16);

    const fl::vector_psram<fl::u8>& data = channel->getData();
    fl::vector<fl::u16> samples;
    samples.resize(static_cast<fl::size>(i2sSamplesForBytes(
                       static_cast<fl::u32>(data.size()))) +
                   kResetSamples);
    encodeFrame(data.data(), data.size(), samples.data(), samples.size());

    // Stream the frame; `_nb` returns false when the DMA is momentarily full, so
    // retry until every sample (incl. the reset tail) is queued.
    fl::size sent = 0;
    while (sent < samples.size()) {
        if (i2s_write_sample_nb(samples[sent])) {
            ++sent;
        }
    }
    // Wait for the DMA to empty so the next frame does not clobber this one.
    while (!i2s_is_empty()) {
    }
}

void Esp8266I2sChannelDriver::show() FL_NO_EXCEPT {
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

#endif  // FL_HAS_INCLUDE(<core_esp8266_i2s.h>)
#endif  // FL_IS_ESP8266
