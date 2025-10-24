#pragma once

/// @file spi_output_template.h
/// @brief Teensy 3 SPIOutput template definition

#include "fl/int.h"
#include "platforms/arm/teensy/teensy31_32/fastspi_arm_k20.h"

namespace fl {

/// Teensy 3 hardware SPI output for all pins
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public ARMHardwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER, 0x4002C000> {};

}  // namespace fl
