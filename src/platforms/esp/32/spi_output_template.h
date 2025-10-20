#pragma once

/// @file spi_output_template.h
/// @brief ESP32 SPIOutput template definition

#include "fl/int.h"
#include "spi_device_proxy.h"

namespace fl {

/// ESP32 hardware SPI output for all pins
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public SPIDeviceProxy<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};

}  // namespace fl
