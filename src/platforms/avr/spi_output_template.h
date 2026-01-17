#pragma once

/// @file spi_output_template.h
/// @brief AVR SPIOutput template definition
///
/// Provides hardware SPI when AVR_HARDWARE_SPI is defined (chips with true SPI),
/// otherwise falls back to software bit-banging (e.g., ATtiny4313 with only USI).

#include "fl/int.h"

#if defined(AVR_HARDWARE_SPI)
#include "fastspi_avr.h"
#else
#include "platforms/shared/spi_bitbang/generic_software_spi.h"
#endif

namespace fl {

#if defined(AVR_HARDWARE_SPI)
/// AVR hardware SPI output for chips with true SPI peripheral
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public AVRHardwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};
#else
/// Software SPI output for AVR chips without hardware SPI (e.g., ATtiny4313)
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public fl::GenericSoftwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};
#endif

}  // namespace fl
