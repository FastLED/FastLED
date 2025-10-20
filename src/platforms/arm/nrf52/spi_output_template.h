#pragma once

/// @file spi_output_template.h
/// @brief NRF52 SPIOutput template definition

#ifndef __INC_PLATFORMS_NRF52_SPI_OUTPUT_TEMPLATE_H
#define __INC_PLATFORMS_NRF52_SPI_OUTPUT_TEMPLATE_H

#if defined(NRF52_SERIES)

#include "fl/int.h"


/// NRF52 hardware SPI output
/// Routes to SPIDeviceProxy for transparent single/dual/quad SPI management
template<u8 _DATA_PIN, u8 _CLOCK_PIN, u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public SPIDeviceProxy<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};


#endif  // NRF52_SERIES

#endif
