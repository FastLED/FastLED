/// @file data.cpp
/// @brief Channel transmission data implementation

#include "fl/channels/data.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/cstring.h"
#include "fl/log/log.h"
#include "fl/stl/noexcept.h"

namespace fl {

ChannelDataPtr ChannelData::create(
    const ChipsetVariant& chipset,
    fl::vector_psram<u8>&& encodedData,
    ChannelPixelFormat pixelFormat
) FL_NO_EXCEPT {
    return fl::make_shared<ChannelData>(
            chipset, fl::move(encodedData), pixelFormat);
}

ChannelDataPtr ChannelData::create(
    int pin,
    const ChipsetTimingConfig& timing,
    fl::vector_psram<u8>&& encodedData,
    ChannelPixelFormat pixelFormat
) FL_NO_EXCEPT {
    return fl::make_shared<ChannelData>(
            pin, timing, fl::move(encodedData), pixelFormat);
}

int ChannelData::getPin() const FL_NO_EXCEPT {
    if (const ClocklessChipset* cs = mChipset.ptr<ClocklessChipset>()) {
        return cs->pin;
    }
    if (const SpiChipsetConfig* spi = mChipset.ptr<SpiChipsetConfig>()) {
        return spi->dataPin;
    }
    return -1;
}

const ChipsetTimingConfig& ChannelData::getTiming() const FL_NO_EXCEPT {
    if (const ClocklessChipset* cs = mChipset.ptr<ClocklessChipset>()) {
        return cs->timing;
    }
    static const ChipsetTimingConfig sEmpty(0, 0, 0, 0);
    return sEmpty;
}

ChannelData::ChannelData(
    const ChipsetVariant& chipset,
    fl::vector_psram<u8>&& encodedData,
    ChannelPixelFormat pixelFormat
) FL_NO_EXCEPT
    : mChipset(chipset)
    , mPixelFormat(pixelFormat)
    , mEncodedData(fl::move(encodedData))
{}

ChannelData::ChannelData(
    int pin,
    const ChipsetTimingConfig& timing,
    fl::vector_psram<u8>&& encodedData,
    ChannelPixelFormat pixelFormat
) FL_NO_EXCEPT
    : mChipset(ClocklessChipset(pin, timing))
    , mPixelFormat(pixelFormat)
    , mEncodedData(fl::move(encodedData))
{}

void ChannelData::writeWithPadding(fl::span<u8> dst) FL_NO_EXCEPT {
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

ChannelData::~ChannelData() FL_NO_EXCEPT = default;

}  // namespace fl
