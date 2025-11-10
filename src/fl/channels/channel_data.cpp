/// @file channel_data.cpp
/// @brief Channel transmission data implementation

#include "channel_data.h"

namespace fl {

ChannelDataPtr ChannelData::create(
    int pin,
    const ChipsetTimingConfig& timing,
    fl::vector_psram<uint8_t>&& encodedData
) {
    return fl::make_shared<ChannelData>(pin, timing, fl::move(encodedData));
}

ChannelData::ChannelData(
    int pin,
    const ChipsetTimingConfig& timing,
    fl::vector_psram<uint8_t>&& encodedData
)
    : mPin(pin)
    , mTiming(timing)
    , mEncodedData(fl::move(encodedData))
{}

}  // namespace fl
