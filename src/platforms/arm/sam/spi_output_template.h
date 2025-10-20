#pragma once

/// @file spi_output_template.h
/// @brief SAM SPIOutput template definition

#include "fl/int.h"
#include "spi_device_proxy.h"

namespace fl {

#if defined(FASTLED_SAM) && defined(FASTLED_ALL_PINS_HARDWARE_SPI)

/// SAM hardware SPI output for all pins
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public SAMHardwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};

#endif  // SAM with FASTLED_ALL_PINS_HARDWARE_SPI

}  // namespace fl
