#pragma once

/// @file spi_output_template.h
/// @brief Teensy LC SPIOutput template definition

#ifndef __INC_PLATFORMS_TEENSYLC_SPI_OUTPUT_TEMPLATE_H
#define __INC_PLATFORMS_TEENSYLC_SPI_OUTPUT_TEMPLATE_H

#if defined(FASTLED_TEENSYLC) && defined(ARM_HARDWARE_SPI)

#include "fl/int.h"


/// Declare SPI0 specializations (base address 0x40076000)
#define DECLARE_SPI0(__DATA,__CLOCK) template<u32 SPI_SPEED>\
 class SPIOutput<__DATA, __CLOCK, SPI_SPEED> : public ARMHardwareSPIOutput<__DATA, __CLOCK, SPI_SPEED, 0x40076000> {};

/// Declare SPI1 specializations (base address 0x40077000)
#define DECLARE_SPI1(__DATA,__CLOCK) template<u32 SPI_SPEED>\
  class SPIOutput<__DATA, __CLOCK, SPI_SPEED> : public ARMHardwareSPIOutput<__DATA, __CLOCK, SPI_SPEED, 0x40077000> {};

// SPI0 pins: Data pin 7, 8, 11, 12 with clock pin 13 or 14
DECLARE_SPI0(7,13);
DECLARE_SPI0(8,13);
DECLARE_SPI0(11,13);
DECLARE_SPI0(12,13);
DECLARE_SPI0(7,14);
DECLARE_SPI0(8,14);
DECLARE_SPI0(11,14);
DECLARE_SPI0(12,14);

// SPI1 pins: Data pin 0, 1, 21 with clock pin 20
DECLARE_SPI1(0,20);
DECLARE_SPI1(1,20);
DECLARE_SPI1(21,20);


#endif  // Teensy LC with hardware SPI

#endif
