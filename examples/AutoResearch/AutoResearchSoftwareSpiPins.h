#pragma once

#include "fl/stl/int.h"

namespace software_spi_probe {
// CI13xx owns its usable defaults; sketch/build overrides still win.
#if defined(FL_IS_CI13XX)
constexpr fl::u8 kDataPin = FL_PIN_SPI_DATA_1;
constexpr fl::u8 kClockPin = FL_PIN_SPI_CLOCK_1;
#else
// Preserve this existing explicit compile-only pin-pair case on other targets.
constexpr fl::u8 kDataPin = 0;
constexpr fl::u8 kClockPin = 1;
#endif
} // namespace software_spi_probe
