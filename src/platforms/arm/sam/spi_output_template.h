#pragma once

// IWYU pragma: private

/// @file spi_output_template.h
/// @brief SAM SPIOutput template definition

#include "fl/stl/int.h"
#include "platforms/arm/sam/fastspi_arm_sam.h"
#include "platforms/arm/sam/is_sam.h"
#include "platforms/arm/samd/is_samd.h"

#if defined(FL_IS_SAMD21) || defined(FL_IS_SAMD51)
#include "platforms/arm/sam/fastspi_arm_samd.hpp"
#include "platforms/shared/spi_bitbang/generic_software_spi.h"
#endif

namespace fl {

#if defined(FL_IS_SAM)
/// SAM3X8E hardware SPI output for all pins
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public SAMHardwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};
#elif defined(FL_IS_SAMD21) || defined(FL_IS_SAMD51)
template<fl::u8 DATA_PIN, fl::u8 CLOCK_PIN, fl::u32 SPI_CLOCK_DIVIDER,
         bool USE_HARDWARE = DATA_PIN == PIN_SPI_MOSI && CLOCK_PIN == PIN_SPI_SCK>
class SAMDSPIOutputSelector
    : public fl::GenericSoftwareSPIOutput<DATA_PIN, CLOCK_PIN, SPI_CLOCK_DIVIDER> {};

template<fl::u8 DATA_PIN, fl::u8 CLOCK_PIN, fl::u32 SPI_CLOCK_DIVIDER>
class SAMDSPIOutputSelector<DATA_PIN, CLOCK_PIN, SPI_CLOCK_DIVIDER, true>
    : public SAMDHardwareSPIOutput<DATA_PIN, CLOCK_PIN, SPI_CLOCK_DIVIDER> {};

/// SAMD uses its native SERCOM on the variant SPI pins and bit-bangs other pins.
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput
    : public SAMDSPIOutputSelector<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};
#endif

}  // namespace fl
