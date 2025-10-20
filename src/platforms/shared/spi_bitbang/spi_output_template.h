#pragma once

/// @file spi_output_template.h
/// @brief Generic software SPI output template for platforms without hardware SPI

#ifndef __INC_PLATFORMS_SHARED_SPI_OUTPUT_TEMPLATE_H
#define __INC_PLATFORMS_SHARED_SPI_OUTPUT_TEMPLATE_H

#include "platforms/shared/spi_bitbang/generic_software_spi.h"

FASTLED_NAMESPACE_BEGIN

/// Generic software SPI output - bit-banging implementation
/// Used as fallback for platforms without hardware SPI support
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public GenericSoftwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};

FASTLED_NAMESPACE_END

#endif
