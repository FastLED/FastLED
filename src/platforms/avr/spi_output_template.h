#pragma once

/// @file spi_output_template.h
/// @brief AVR SPIOutput template definition

#include "fl/int.h"

namespace fl {

#if defined(FASTLED_AVR) && defined(FASTLED_ALL_PINS_HARDWARE_SPI)

/// AVR hardware SPI output for all pins
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public AVRHardwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};

#endif  // AVR with FASTLED_ALL_PINS_HARDWARE_SPI

}  // namespace fl
