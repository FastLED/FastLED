#pragma once

/// @file spi_output_template.h
/// @brief Teensy LC SPIOutput template definition

#include "fl/int.h"
#include "fastspi_arm_kl26.h"

namespace fl {

/// Teensy LC hardware SPI output for all pins
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public ARMHardwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER, 0x40076000> {};

}  // namespace fl
