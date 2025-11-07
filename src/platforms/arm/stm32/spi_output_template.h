#pragma once

/// @file spi_output_template.h
/// @brief STM32 SPIOutput template definition

#include "fl/int.h"
#include "platforms/arm/stm32/fastspi_arm_stm32.h"

namespace fl {

/// STM32 hardware SPI output for all pins
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public STM32SPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};

}  // namespace fl
