#pragma once

// IWYU pragma: private

/// @file spi_output_template.h
/// @brief Teensy LC SPIOutput template definition

#include "fl/stl/int.h"
#include "platforms/arm/teensy/teensy_lc/spi_output_selector.h"

namespace fl {

/// Teensy LC SPI output: native SPI0 on its pads, software SPI on any other pins.
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public TeensyLcSPIOutputBase<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};

}  // namespace fl
