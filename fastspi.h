#ifndef __INC_FASTSPI_H
#define __INC_FASTSPI_H

#include "controller.h"
#include "lib8tion.h"

#include "fastspi_bitbang.h"

FASTLED_NAMESPACE_BEGIN

#if (CLK_DBL == 1)
#define DATA_RATE_MHZ(X) (((F_CPU / 1000000L) / X)/2)
#define DATA_RATE_KHZ(X) (((F_CPU / 1000L) / X)/2)
#else
#define DATA_RATE_MHZ(X) ((F_CPU / 1000000L) / X)
#define DATA_RATE_KHZ(X) ((F_CPU / 1000L) / X)
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// External SPI template definition with partial instantiation(s) to map to hardware SPI ports on platforms/builds where the pin
// mappings are known at compile time.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_CLOCK_DIVIDER>
class SPIOutput : public AVRSoftwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};

template<uint8_t _DATA_PIN, uint8_t _CLOCK_PIN, uint8_t _SPI_CLOCK_DIVIDER>
class SoftwareSPIOutput : public AVRSoftwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};

#ifndef FASTLED_FORCE_SOFTWARE_SPI
#if defined(SPI_DATA) && defined(SPI_CLOCK)

#if defined(FASTLED_TEENSY3) && defined(ARM_HARDWARE_SPI)

template<uint8_t SPI_SPEED>
class SPIOutput<SPI_DATA, SPI_CLOCK, SPI_SPEED> : public ARMHardwareSPIOutput<SPI_DATA, SPI_CLOCK, SPI_SPEED, 0x4002C000> {};

#if defined(SPI2_DATA)

template<uint8_t SPI_SPEED>
class SPIOutput<SPI2_DATA, SPI2_CLOCK, SPI_SPEED> : public ARMHardwareSPIOutput<SPI2_DATA, SPI2_CLOCK, SPI_SPEED, 0x4002C000> {};
#endif

#elif defined(FASTLED_TEENSYLC) && defined(ARM_HARDWARE_SPI)

template<uint8_t SPI_SPEED>
class SPIOutput<SPI_DATA, SPI_CLOCK, SPI_SPEED> : public ARMHardwareSPIOutput<SPI_DATA, SPI_CLOCK, SPI_SPEED, 0x40076000> {};

#if defined(SPI2_DATA)

template<uint8_t SPI_SPEED>
class SPIOutput<SPI2_DATA, SPI2_CLOCK, SPI_SPEED> : public ARMHardwareSPIOutput<SPI2_DATA, SPI2_CLOCK, SPI_SPEED, 0x40077000> {};
#endif

#elif defined(__SAM3X8E__)

template<uint8_t SPI_SPEED>
class SPIOutput<SPI_DATA, SPI_CLOCK, SPI_SPEED> : public SAMHardwareSPIOutput<SPI_DATA, SPI_CLOCK, SPI_SPEED> {};

#elif defined(AVR_HARDWARE_SPI)

template<uint8_t SPI_SPEED>
class SPIOutput<SPI_DATA, SPI_CLOCK, SPI_SPEED> : public AVRHardwareSPIOutput<SPI_DATA, SPI_CLOCK, SPI_SPEED> {};

#if defined(SPI_UART0_DATA)

template<uint8_t SPI_SPEED>
class SPIOutput<SPI_UART0_DATA, SPI_UART0_CLOCK, SPI_SPEED> : public AVRUSART0SPIOutput<SPI_UART0_DATA, SPI_UART0_CLOCK, SPI_SPEED> {};

#endif

#if defined(SPI_UART1_DATA)

template<uint8_t SPI_SPEED>
class SPIOutput<SPI_UART1_DATA, SPI_UART1_CLOCK, SPI_SPEED> : public AVRUSART1SPIOutput<SPI_UART1_DATA, SPI_UART1_CLOCK, SPI_SPEED> {};

#endif

#endif

#else
#warning "No hardware SPI pins defined.  All SPI access will default to bitbanged output"

#endif

// #if defined(USART_DATA) && defined(USART_CLOCK)
// template<uint8_t SPI_SPEED>
// class AVRSPIOutput<USART_DATA, USART_CLOCK, SPI_SPEED> : public AVRUSARTSPIOutput<USART_DATA, USART_CLOCK, SPI_SPEED> {};
// #endif

#else
#warning "Forcing software SPI - no hardware SPI for you!"
#endif

FASTLED_NAMESPACE_END

#endif
