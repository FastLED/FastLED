#pragma once

/// @file spi_output_template.h
/// @brief Apollo3 SPIOutput template definition

#ifndef __INC_PLATFORMS_APOLLO3_SPI_OUTPUT_TEMPLATE_H
#define __INC_PLATFORMS_APOLLO3_SPI_OUTPUT_TEMPLATE_H

#if defined(FASTLED_APOLLO3) && defined(FASTLED_ALL_PINS_HARDWARE_SPI)

#include "fl/int.h"


/// Apollo3 hardware SPI output for all pins
template<u8 _DATA_PIN, u8 _CLOCK_PIN, u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public APOLLO3HardwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};


#endif  // Apollo3 with FASTLED_ALL_PINS_HARDWARE_SPI

#endif
