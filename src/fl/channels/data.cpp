/// @file data.cpp
/// @brief Channel transmission data implementation

#include "fl/channels/data.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/cstring.h"
#include "fl/log.h"

namespace fl {

namespace {
// Helper to extract data pin from chipset variant
int getDataPinFromChipset(const ChipsetVariant& chipset) {
    if (const ClocklessChipset* clockless = chipset.ptr<ClocklessChipset>()) {
        return clockless->pin;
    } else if (const SpiChipsetConfig* spi = chipset.ptr<SpiChipsetConfig>()) {
        return spi->dataPin;
    }
    return -1;
}

// Helper to extract timing from chipset variant (clockless only)
ChipsetTimingConfig getTimingFromChipset(const ChipsetVariant& chipset) {
    if (const ClocklessChipset* clockless = chipset.ptr<ClocklessChipset>()) {
        return clockless->timing;
    }
    return ChipsetTimingConfig(0, 0, 0, 0);  // Invalid/empty timing for SPI
}
} // anonymous namespace

ChannelDataPtr ChannelData::create(
    const ChipsetVariant& chipset,
    fl::vector_psram<uint8_t>&& encodedData
) {
    return fl::make_shared<ChannelData>(chipset, fl::move(encodedData));
}

ChannelDataPtr ChannelData::create(
    int pin,
    const ChipsetTimingConfig& timing,
    fl::vector_psram<uint8_t>&& encodedData
) {
    return fl::make_shared<ChannelData>(pin, timing, fl::move(encodedData));
}

ChannelData::ChannelData(
    const ChipsetVariant& chipset,
    fl::vector_psram<uint8_t>&& encodedData
)
    : mChipset(chipset)
    , mPin(getDataPinFromChipset(chipset))
    , mTiming(getTimingFromChipset(chipset))
    , mEncodedData(fl::move(encodedData))
{}

ChannelData::ChannelData(
    int pin,
    const ChipsetTimingConfig& timing,
    fl::vector_psram<uint8_t>&& encodedData
)
    : mChipset(ClocklessChipset(pin, timing))
    , mPin(pin)
    , mTiming(timing)
    , mEncodedData(fl::move(encodedData))
{}

void ChannelData::writeWithPadding(fl::span<uint8_t> dst) {
    size_t targetSize = dst.size();
    size_t currentSize = mEncodedData.size();

    // Destination must be at least as large as current data
    if (targetSize < currentSize) {
        return; // or throw? For now, silently fail
    }

    // Create source span from encoded data
    fl::span<const uint8_t> src(mEncodedData.data(), currentSize);

    if (mPaddingGenerator) {
        // Use custom padding generator (writes directly to dst)
        mPaddingGenerator(src, dst);
    } else {
        // Default behavior: left-pad with zeros, then memcopy data
        // Padding bytes go out first to non-existent pixels
        size_t paddingSize = targetSize - currentSize;
        if (paddingSize > 0) {
            fl::fill(dst.begin(), dst.begin() + paddingSize, uint8_t(0));
        }
        fl::memcopy(dst.data() + paddingSize, src.data(), currentSize);
    }
}

ChannelData::~ChannelData() = default;

}  // namespace fl
