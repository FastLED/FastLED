#pragma once

/// @file spi_output_template.h
/// @brief STM32 SPIOutput template definition

#ifndef __INC_PLATFORMS_STM32_SPI_OUTPUT_TEMPLATE_H
#define __INC_PLATFORMS_STM32_SPI_OUTPUT_TEMPLATE_H

#if defined(STM32F10X_MD) || defined(__STM32F1__) || defined(STM32F1) || defined(STM32F2XX) || defined(STM32F4)

#include "fl/int.h"

FASTLED_NAMESPACE_BEGIN

/// STM32 hardware SPI output
/// Routes to SPIDeviceProxy for hardware SPI management
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_SPEED>
class SPIOutput : public fl::SPIDeviceProxy<_DATA_PIN, _CLOCK_PIN, _SPI_SPEED> {};

FASTLED_NAMESPACE_END

#endif  // STM32 variants

#endif
