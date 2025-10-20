#pragma once

/// @file spi_output_template.h
/// @brief ESP8266 SPIOutput template definition

#ifndef __INC_PLATFORMS_ESP8266_SPI_OUTPUT_TEMPLATE_H
#define __INC_PLATFORMS_ESP8266_SPI_OUTPUT_TEMPLATE_H

#if defined(ESP8266)

#include "fl/int.h"
#include "platforms/esp/8266/fastspi_esp8266.h"

FASTLED_NAMESPACE_BEGIN

#ifdef FASTLED_ALL_PINS_HARDWARE_SPI
/// ESP8266 hardware SPI output for any pins
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public ESP8266SPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};
#else
// Specialization for standard SPI pins only
template<fl::u32 SPI_SPEED>
class SPIOutput<SPI_DATA, SPI_CLOCK, SPI_SPEED> : public ESP8266SPIOutput<SPI_DATA, SPI_CLOCK, SPI_SPEED> {};
#endif

FASTLED_NAMESPACE_END

#endif  // ESP8266

#endif
