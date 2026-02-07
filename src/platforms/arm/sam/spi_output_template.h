#pragma once

/// @file spi_output_template.h
/// @brief SAM SPIOutput template definition

#include "fl/int.h"
#include "fastspi_arm_sam.h"
#include "platforms/arm/sam/is_sam.h"
#include "platforms/arm/samd/is_samd.h"

namespace fl {

#if defined(FL_IS_SAM)
/// SAM3X8E hardware SPI output for all pins
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public SAMHardwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};
#elif defined(FL_IS_SAMD21) || defined(FL_IS_SAMD51)
/// SAMD hardware SPI output for all pins
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public SAMDHardwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};
#endif

}  // namespace fl
