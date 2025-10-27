#pragma once

/// @file spi_output_template.h
/// @brief Teensy 4 SPIOutput template definition

#include "fl/int.h"
#include "platforms/arm/mxrt1062/spi_device_proxy.h"

namespace fl {

/// Teensy 4 hardware SPI output for all pins
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public SPIDeviceProxy<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER, SPI, 0> {};

}  // namespace fl
