#pragma once

/// @file spi_output_template.h
/// @brief STM32 SPIOutput template definition

#include "fl/int.h"
#include "spi_device_proxy.h"

namespace fl {

#if defined(FASTLED_STM32) && defined(FASTLED_ALL_PINS_HARDWARE_SPI)

/// STM32 hardware SPI output for all pins
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public STM32HardwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};

#endif  // STM32 with FASTLED_ALL_PINS_HARDWARE_SPI

}  // namespace fl
