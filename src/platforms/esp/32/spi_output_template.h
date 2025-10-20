#pragma once

/// @file spi_output_template.h
/// @brief ESP32 SPIOutput template definition

#ifndef __INC_PLATFORMS_ESP32_SPI_OUTPUT_TEMPLATE_H
#define __INC_PLATFORMS_ESP32_SPI_OUTPUT_TEMPLATE_H

#if defined(ESP32) || defined(ESP32S2) || defined(ESP32S3) || defined(ESP32C3) || defined(ESP32P4)

#include "fl/int.h"


/// ESP32 hardware SPI output
/// Routes to appropriate SPI backend via SPIDeviceProxy
template<u8 _DATA_PIN, u8 _CLOCK_PIN, u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public SPIDeviceProxy<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};


#endif  // ESP32 variants

#endif
