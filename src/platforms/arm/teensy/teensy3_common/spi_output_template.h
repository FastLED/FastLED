#pragma once

/// @file spi_output_template.h
/// @brief Teensy 3 SPIOutput template definition

#include "fl/int.h"
#include "spi_device_proxy.h"

namespace fl {

#if defined(FASTLED_TEENSY3) && defined(FASTLED_ALL_PINS_HARDWARE_SPI)

/// Teensy 3 hardware SPI output for all pins
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public Teensy3HardwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};

#endif  // Teensy 3 with FASTLED_ALL_PINS_HARDWARE_SPI

}  // namespace fl
