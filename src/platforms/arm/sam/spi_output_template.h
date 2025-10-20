#pragma once

/// @file spi_output_template.h
/// @brief SAM/SAMD SPIOutput template definition

#ifndef __INC_PLATFORMS_SAM_SPI_OUTPUT_TEMPLATE_H
#define __INC_PLATFORMS_SAM_SPI_OUTPUT_TEMPLATE_H

#if defined(__SAM3X8E__) || defined(__SAMD21G18A__) || defined(__SAMD21J18A__) || defined(__SAMD21E17A__) || \
    defined(__SAMD21E18A__) || defined(__SAMD51G19A__) || defined(__SAMD51J19A__) || defined(__SAME51J19A__) || \
    defined(__SAMD51P19A__) || defined(__SAMD51P20A__)

#include "fl/int.h"

FASTLED_NAMESPACE_BEGIN

/// SAM3X8E / SAMD21 / SAMD51 hardware SPI output
/// Routes to SPIDeviceProxy for transparent single/dual/quad SPI management
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public fl::SPIDeviceProxy<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};

FASTLED_NAMESPACE_END

#endif  // SAM/SAMD

#endif
