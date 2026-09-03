#pragma once

#include "fl/stl/stdint.h"

namespace fl {

/// ESP-IDF SPI host selection for an individual clocked LED controller.
/// SPI1 is intentionally unavailable because ESP-IDF reserves it for flash/PSRAM.
///
/// HOST2 and HOST3 are ESP-IDF's SPI2_HOST and SPI3_HOST. They are deliberately
/// *not* spelled SPI2 and SPI3: this header reaches every platform through
/// fl/channels/cled_controller.h, and on STM32 the CMSIS device headers define
/// those names as object-like macros --
/// `#define SPI2 ((SPI_TypeDef *) SPI2_BASE)` -- so the preprocessor rewrote
/// the enumerator to `((SPI_TypeDef *) SPI2_BASE) = 2` and every STM32 board
/// failed to compile (FastLED#4113). Arduino-ESP32 3.1.0+ defines SPI2 as well.
/// tests/fl/spi_bus_macro_collision.cpp pins this.
enum class Esp32SpiBus : u8 {
    AUTO = 0,
    HOST2 = 2,
    HOST3 = 3,
};

} // namespace fl
