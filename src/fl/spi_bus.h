#pragma once

#include "fl/stl/stdint.h"

namespace fl {

/// ESP-IDF SPI host selection for an individual clocked LED controller.
/// SPI1 is intentionally unavailable because ESP-IDF reserves it for flash/PSRAM.
enum class Esp32SpiBus : u8 {
    AUTO = 0,
    SPI2 = 2,
    SPI3 = 3,
};

} // namespace fl
