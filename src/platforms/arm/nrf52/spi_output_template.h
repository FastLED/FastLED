#pragma once

/// @file spi_output_template.h
/// @brief NRF52 SPIOutput template definition

#include "fl/int.h"
#include "spi_device_proxy.h"

namespace fl {

#if defined(FASTLED_NRF52) && defined(FASTLED_ALL_PINS_HARDWARE_SPI)

/// NRF52 hardware SPI output for all pins
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public NRF52HardwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};

#endif  // NRF52 with FASTLED_ALL_PINS_HARDWARE_SPI

}  // namespace fl
