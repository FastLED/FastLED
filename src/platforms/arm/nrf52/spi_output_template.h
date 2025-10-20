#pragma once

/// @file spi_output_template.h
/// @brief NRF52 SPIOutput template definition

#ifndef __INC_PLATFORMS_NRF52_SPI_OUTPUT_TEMPLATE_H
#define __INC_PLATFORMS_NRF52_SPI_OUTPUT_TEMPLATE_H

#if defined(NRF52_SERIES)

#include "fl/int.h"

FASTLED_NAMESPACE_BEGIN

/// NRF52 hardware SPI output
/// Routes to SPIDeviceProxy for transparent single/dual/quad SPI management
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public fl::SPIDeviceProxy<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};

FASTLED_NAMESPACE_END

#endif  // NRF52_SERIES

#endif
