#pragma once

/// @file spi_output_template.h
/// @brief AVR SPIOutput template definition

#ifndef __INC_PLATFORMS_AVR_SPI_OUTPUT_TEMPLATE_H
#define __INC_PLATFORMS_AVR_SPI_OUTPUT_TEMPLATE_H

#include "fl/int.h"

FASTLED_NAMESPACE_BEGIN

#if defined(AVR_HARDWARE_SPI)

/// AVR hardware SPI output for standard SPI pins
template<fl::u32 SPI_SPEED>
class SPIOutput<SPI_DATA, SPI_CLOCK, SPI_SPEED> : public AVRHardwareSPIOutput<SPI_DATA, SPI_CLOCK, SPI_SPEED> {};

#if defined(SPI_UART0_DATA)
/// AVR UART0 SPI output for UART0-based SPI pins
template<fl::u32 SPI_SPEED>
class SPIOutput<SPI_UART0_DATA, SPI_UART0_CLOCK, SPI_SPEED> : public AVRUSART0SPIOutput<SPI_UART0_DATA, SPI_UART0_CLOCK, SPI_SPEED> {};
#endif

#if defined(SPI_UART1_DATA)
/// AVR UART1 SPI output for UART1-based SPI pins
template<fl::u32 SPI_SPEED>
class SPIOutput<SPI_UART1_DATA, SPI_UART1_CLOCK, SPI_SPEED> : public AVRUSART1SPIOutput<SPI_UART1_DATA, SPI_UART1_CLOCK, SPI_SPEED> {};
#endif

#elif defined(ARDUNIO_CORE_SPI)

/// Arduino core SPI output
template<fl::u32 SPI_SPEED>
class SPIOutput<SPI_DATA, SPI_CLOCK, SPI_SPEED> : public ArdunioCoreSPIOutput<SPI_DATA, SPI_CLOCK, SPI_SPEED, SPI> {};

#endif

FASTLED_NAMESPACE_END

#endif
