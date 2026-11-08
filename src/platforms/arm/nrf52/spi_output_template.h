#pragma once

// IWYU pragma: private

/// @file spi_output_template.h
/// @brief NRF52 SPIOutput template definition

#include "fl/stl/int.h"
#include "platforms/arm/nrf52/fastspi_arm_nrf52.h"

#if defined(FASTLED_FORCE_SOFTWARE_SPI)
// Hardware SPI disabled: fall back to the generic bit-bang SPIOutput (#5120).
#include "platforms/shared/spi_bitbang/spi_output_template.h"
#else

namespace fl {

/// NRF52 hardware SPI output for all pins
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public fl::NRF52HardwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};

}  // namespace fl

#endif
