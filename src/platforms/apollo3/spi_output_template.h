#pragma once

/// @file spi_output_template.h
/// @brief Apollo3 SPIOutput template definition

#if defined(FASTLED_APOLLO3) && defined(FASTLED_ALL_PINS_HARDWARE_SPI)

#include "fl/int.h"

namespace fl {

/// Apollo3 hardware SPI output for all pins
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public APOLLO3HardwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};

}  // namespace fl

#endif  // Apollo3 with FASTLED_ALL_PINS_HARDWARE_SPI
