#pragma once

/// @file spi_output_template.h
/// @brief Teensy 3.x SPIOutput template definition

#ifndef __INC_PLATFORMS_TEENSY3_SPI_OUTPUT_TEMPLATE_H
#define __INC_PLATFORMS_TEENSY3_SPI_OUTPUT_TEMPLATE_H

#if defined(FASTLED_TEENSY3) && defined(ARM_HARDWARE_SPI) && defined(SPI_DATA) && defined(SPI_CLOCK)

#include "fl/int.h"


/// Generic fallback for generic pins on Teensy 3
template<u8 _DATA_PIN, u8 _CLOCK_PIN, u32 _SPI_SPEED>
class SPIOutput : public GenericSoftwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_SPEED> {};

/// Specialization for hardware SPI pins
template<u32 _SPI_SPEED>
class SPIOutput<SPI_DATA, SPI_CLOCK, _SPI_SPEED> : public SPIDeviceProxy<SPI_DATA, SPI_CLOCK, _SPI_SPEED> {};


#endif  // Teensy3 with hardware SPI

#endif
