#pragma once

// IWYU pragma: private

/// @file spi_output_template.h
/// @brief NRF51 SPIOutput template definition

#include "fl/stl/int.h"
#include "platforms/arm/nrf51/fastspi_arm_nrf51.h"

#if defined(FASTLED_FORCE_SOFTWARE_SPI)
// Hardware SPI disabled: fall back to the generic bit-bang SPIOutput.
#include "platforms/shared/spi_bitbang/spi_output_template.h"
#else

namespace fl {

/// NRF51 hardware SPI (SPI0) output for all pins
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public fl::NRF51SPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};

}  // namespace fl

#endif
