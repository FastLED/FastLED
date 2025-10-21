#pragma once

/// @file spi_output_template.h
/// @brief Apollo3 SPIOutput template definition

#include "fl/int.h"
#include "fastspi_apollo3.h"

namespace fl {

/// Apollo3 hardware SPI output for all pins
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public APOLLO3HardwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};

}  // namespace fl
