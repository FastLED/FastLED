#pragma once

/// @file spi_output_template.h
/// @brief Teensy 4.x SPIOutput template definition

#ifndef __INC_PLATFORMS_TEENSY4_SPI_OUTPUT_TEMPLATE_H
#define __INC_PLATFORMS_TEENSY4_SPI_OUTPUT_TEMPLATE_H

#if defined(FASTLED_TEENSY4) && defined(ARM_HARDWARE_SPI) && defined(SPI_DATA) && defined(SPI_CLOCK)

#include "fl/int.h"
#include "spi_device_proxy.h"


/// Generic fallback for generic pins on Teensy 4
template<u8 _DATA_PIN, u8 _CLOCK_PIN, u32 _SPI_SPEED>
class SPIOutput : public GenericSoftwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_SPEED> {};

/// Specialization for primary SPI peripheral (SPI object)
template<u32 _SPI_SPEED>
class SPIOutput<SPI_DATA, SPI_CLOCK, _SPI_SPEED> : public fl::SPIDeviceProxy<SPI_DATA, SPI_CLOCK, _SPI_SPEED, SPI, 0> {};

/// Specialization for secondary SPI peripheral (SPI1 object)
template<u32 _SPI_SPEED>
class SPIOutput<SPI1_DATA, SPI1_CLOCK, _SPI_SPEED> : public fl::SPIDeviceProxy<SPI1_DATA, SPI1_CLOCK, _SPI_SPEED, SPI1, 1> {};

/// Specialization for tertiary SPI peripheral (SPI2 object)
template<u32 _SPI_SPEED>
class SPIOutput<SPI2_DATA, SPI2_CLOCK, _SPI_SPEED> : public fl::SPIDeviceProxy<SPI2_DATA, SPI2_CLOCK, _SPI_SPEED, SPI2, 2> {};


#endif  // Teensy4 with hardware SPI

#endif
