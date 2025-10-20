#pragma once

/// @file spi_output_template.h
/// @brief STM32 SPIOutput template definition

#ifndef __INC_PLATFORMS_STM32_SPI_OUTPUT_TEMPLATE_H
#define __INC_PLATFORMS_STM32_SPI_OUTPUT_TEMPLATE_H

#if defined(STM32F10X_MD) || defined(__STM32F1__) || defined(STM32F1) || defined(STM32F2XX) || defined(STM32F4)

#include "fl/int.h"
#include "spi_device_proxy.h"


/// STM32 hardware SPI output
/// Routes to SPIDeviceProxy for hardware SPI management
template<u8 _DATA_PIN, u8 _CLOCK_PIN, u32 _SPI_SPEED>
class SPIOutput : public fl::SPIDeviceProxy<_DATA_PIN, _CLOCK_PIN, _SPI_SPEED> {};


#endif  // STM32 variants

#endif
