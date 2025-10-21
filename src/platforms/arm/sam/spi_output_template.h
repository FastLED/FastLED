#pragma once

/// @file spi_output_template.h
/// @brief SAM SPIOutput template definition

#include "fl/int.h"
#include "fastspi_arm_sam.h"

namespace fl {

#if defined(__SAM3X8E__)
/// SAM3X8E hardware SPI output for all pins
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public SAMHardwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};
#elif defined(__SAMD21G18A__) || defined(__SAMD21J18A__) || defined(__SAMD21E17A__) || \
      defined(__SAMD21E18A__) || defined(__SAMD51G19A__) || defined(__SAMD51J19A__) || \
      defined(__SAME51J19A__) || defined(__SAMD51P19A__) || defined(__SAMD51P20A__)
/// SAMD hardware SPI output for all pins
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public SAMDHardwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};
#endif

}  // namespace fl
