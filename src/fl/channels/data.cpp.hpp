/// @file data.cpp
/// @brief Channel transmission data implementation

#include "fl/channels/data.h"
#include "fl/channels/chipset_helpers.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/cstring.h"
#include "fl/log.h"

namespace fl {

ChannelDataPtr ChannelData::create(
    const ChipsetVariant& chipset,
    fl::vector_psram<u8>&& encodedData
) {
    return fl::make_shared<ChannelData>(chipset, fl::move(encodedData));
}

ChannelDataPtr ChannelData::create(
    int pin,
    const ChipsetTimingConfig& timing,
    fl::vector_psram<u8>&& encodedData
) {
    return fl::make_shared<ChannelData>(pin, timing, fl::move(encodedData));
}

ChannelData::ChannelData(
    const ChipsetVariant& chipset,
    fl::vector_psram<u8>&& encodedData
)
    : mChipset(chipset)
    , mPin(getDataPinFromChipset(chipset))
    , mTiming(getTimingFromChipset(chipset))
    , mEncodedData(fl::move(encodedData))
{}

ChannelData::ChannelData(
    int pin,
    const ChipsetTimingConfig& timing,
    fl::vector_psram<u8>&& encodedData
)
    : mChipset(ClocklessChipset(pin, timing))
    , mPin(pin)
    , mTiming(timing)
    , mEncodedData(fl::move(encodedData))
{}

void ChannelData::writeWithPadding(fl::span<u8> dst) {
    size_t targetSize = dst.size();
    size_t currentSize = mEncodedData.size();

    // Destination must be at least as large as current data
    if (targetSize < currentSize) {
        return; // or throw? For now, silently fail
    }

    // Create source span from encoded data
    fl::span<const u8> src(mEncodedData.data(), currentSize);

    if (mPaddingGenerator) {
        // Use custom padding generator (writes directly to dst)
        mPaddingGenerator(src, dst);
    } else {
        // Default behavior: left-pad with zeros, then memcopy data
        // Padding bytes go out first to non-existent pixels
        size_t paddingSize = targetSize - currentSize;
        if (paddingSize > 0) {
            fl::fill(dst.begin(), dst.begin() + paddingSize, u8(0));
        }
        fl::memcopy(dst.data() + paddingSize, src.data(), currentSize);
    }
}

ChannelData::~ChannelData() = default;

}  // namespace fl
