#pragma once

// IWYU pragma: private

/// @file spi_output_template.h
/// @brief AVR SPIOutput template definition
///
/// Uses hardware SPI for the board's native data and clock pair when available.
/// Other pin pairs and FASTLED_FORCE_SOFTWARE_SPI use software bit-banging.

#include "fl/stl/int.h"
#include "platforms/shared/spi_bitbang/generic_software_spi.h"

#if defined(AVR_HARDWARE_SPI) && !defined(FASTLED_FORCE_SOFTWARE_SPI)
#include "platforms/avr/fastspi_avr.h"
#endif

namespace fl {

template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public fl::GenericSoftwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};

#if defined(AVR_HARDWARE_SPI) && !defined(FASTLED_FORCE_SOFTWARE_SPI)
template<fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput<SPI_DATA, SPI_CLOCK, _SPI_CLOCK_DIVIDER>
    : public AVRHardwareSPIOutput<SPI_DATA, SPI_CLOCK, _SPI_CLOCK_DIVIDER> {};
#endif

}  // namespace fl
