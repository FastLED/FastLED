#pragma once

#include "fl/channels/channel.h"

namespace fl {

// Helper to extract data pin from chipset variant
inline int getDataPinFromChipset(const ChipsetVariant& chipset) {
    if (const ClocklessChipset* clockless = chipset.ptr<ClocklessChipset>()) {
        return clockless->pin;
    } else if (const SpiChipsetConfig* spi = chipset.ptr<SpiChipsetConfig>()) {
        return spi->dataPin;
    }
    return -1;
}

// Helper to extract timing from chipset variant (clockless only)
inline ChipsetTimingConfig getTimingFromChipset(const ChipsetVariant& chipset) {
    if (const ClocklessChipset* clockless = chipset.ptr<ClocklessChipset>()) {
        return clockless->timing;
    }
    return ChipsetTimingConfig(0, 0, 0, 0);  // Invalid/empty timing for SPI
}

} // namespace fl
