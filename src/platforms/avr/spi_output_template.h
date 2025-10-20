#pragma once

/// @file spi_output_template.h
/// @brief AVR SPIOutput template definition

#include "fl/int.h"
#include "fastspi_avr.h"

namespace fl {

/// AVR hardware SPI output for all pins
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public AVRHardwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};

}  // namespace fl
