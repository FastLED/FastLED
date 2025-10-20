#pragma once

/// @file spi_output_template.h
/// @brief NRF51 SPIOutput template definition

#ifndef __INC_PLATFORMS_NRF51_SPI_OUTPUT_TEMPLATE_H
#define __INC_PLATFORMS_NRF51_SPI_OUTPUT_TEMPLATE_H

#if defined(NRF51)

#include "fl/int.h"


/// NRF51 hardware SPI output
/// Routes to SPIDeviceProxy for transparent single/dual/quad SPI management
template<u8 _DATA_PIN, u8 _CLOCK_PIN, u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public SPIDeviceProxy<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};


#endif  // NRF51

#endif
