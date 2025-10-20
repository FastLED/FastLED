#pragma once

/// @file fastspi.h
/// Serial peripheral interface (SPI) definitions per platform

#ifndef __INC_FASTSPI_H
#define __INC_FASTSPI_H

#include "controller.h"
#include "lib8tion.h"

#include "platforms/shared/spi_bitbang/generic_software_spi.h"
#include "fl/int.h"

// Include platform-specific SPI device proxy headers
#if defined(ESP32) || defined(ESP32S2) || defined(ESP32S3) || defined(ESP32C3) || defined(ESP32P4)
#include "platforms/esp/32/spi_device_proxy.h"
#elif defined(__IMXRT1062__) && defined(ARM_HARDWARE_SPI)
#include "platforms/arm/mxrt1062/spi_device_proxy.h"
#elif defined(NRF51)
#include "platforms/arm/nrf52/spi_device_proxy.h"
#elif defined(NRF52_SERIES)
#include "platforms/arm/nrf52/spi_device_proxy.h"
#elif defined(__SAM3X8E__) || defined(__SAMD21G18A__) || defined(__SAMD21J18A__) || defined(__SAMD21E17A__) || defined(__SAMD21E18A__) || defined(__SAMD51G19A__) || defined(__SAMD51J19A__) || defined(__SAME51J19A__) || defined(__SAMD51P19A__) || defined(__SAMD51P20A__)
#include "platforms/arm/sam/spi_device_proxy.h"
#elif defined(STM32F10X_MD) || defined(__STM32F1__) || defined(STM32F1) || defined(STM32F2XX) || defined(STM32F4)
#include "platforms/arm/stm32/spi_device_proxy.h"
#endif

// Forward declaration for SPIDeviceProxy to avoid circular dependency issues
// Include FastLED.h here to avoid circular dependency issues
#include "FastLED.h"

// Platform-specific forward declarations for SPIDeviceProxy
#if defined(ESP32) || defined(ESP32S2) || defined(ESP32S3) || defined(ESP32C3) || defined(ESP32P4)
namespace fl {
template<uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint32_t SPI_SPEED>
class SPIDeviceProxy;
}
#endif

#if defined(__IMXRT1062__) && defined(ARM_HARDWARE_SPI)
namespace fl {
template<uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint32_t SPI_CLOCK_RATE, SPIClass & SPIObject, int SPI_INDEX>
class SPIDeviceProxy;
}
#endif

#if defined(NRF51) || defined(NRF52_SERIES)
namespace fl {
template<uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint32_t SPI_CLOCK_DIVIDER>
class SPIDeviceProxy;
}
#endif

#if defined(__SAM3X8E__) || defined(__SAMD21G18A__) || defined(__SAMD21J18A__) || defined(__SAMD21E17A__) || \
    defined(__SAMD21E18A__) || defined(__SAMD51G19A__) || defined(__SAMD51J19A__) || defined(__SAME51J19A__) || \
    defined(__SAMD51P19A__) || defined(__SAMD51P20A__)
namespace fl {
template<uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint32_t SPI_CLOCK_DIVIDER>
class SPIDeviceProxy;
}
#endif

#if defined(STM32F10X_MD) || defined(__STM32F1__) || defined(STM32F1) || defined(STM32F2XX) || defined(STM32F4)
namespace fl {
template<uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint32_t SPI_SPEED>
class SPIDeviceProxy;
}
#endif

#if defined(FASTLED_TEENSY3)
namespace fl {
template<uint8_t DATA_PIN, uint8_t CLOCK_PIN, uint32_t SPI_SPEED>
class SPIDeviceProxy;
}
#endif

#if defined(FASTLED_TEENSY3) && (F_CPU > 48000000)
#define DATA_RATE_MHZ(X) (((48000000L / 1000000L) / X))
#define DATA_RATE_KHZ(X) (((48000000L / 1000L) / X))
#elif defined(FASTLED_TEENSY4) || (defined(ESP32) && defined(FASTLED_ALL_PINS_HARDWARE_SPI)) || (defined(ESP8266) && defined(FASTLED_ALL_PINS_HARDWARE_SPI) || defined(FASTLED_STUB_IMPL))
// just use clocks
#define DATA_RATE_MHZ(X) (1000000 * (X))
#define DATA_RATE_KHZ(X) (1000 * (X))
#else
/// Convert data rate from megahertz (MHz) to clock cycles per bit
#define DATA_RATE_MHZ(X) ((F_CPU / 1000000L) / X)
/// Convert data rate from kilohertz (KHz) to clock cycles per bit
#define DATA_RATE_KHZ(X) ((F_CPU / 1000L) / X)
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// External SPI template definition with partial instantiation(s) to map to hardware SPI ports on platforms/builds where the pin
// mappings are known at compile time.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FASTLED_NAMESPACE_BEGIN

#if defined(FASTLED_STUB_IMPL)

template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public fl::StubSPIOutput {};


#else

#if !defined(FASTLED_ALL_PINS_HARDWARE_SPI) && !defined(ESP32) && !defined(FASTLED_TEENSY3) && !defined(FASTLED_TEENSY4) && !defined(FASTLED_TEENSYLC)
/// Hardware SPI output
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public GenericSoftwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};
#endif

/// Software SPI output
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SoftwareSPIOutput : public GenericSoftwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};

#ifndef FASTLED_FORCE_SOFTWARE_SPI

#if defined(NRF51)
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public fl::SPIDeviceProxy<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};
#endif

#if defined(NRF52_SERIES)
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public fl::SPIDeviceProxy<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};
#endif

#if defined(FASTLED_APOLLO3) && defined(FASTLED_ALL_PINS_HARDWARE_SPI)
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public APOLLO3HardwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};
#endif

#if defined(ESP32)
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public fl::SPIDeviceProxy<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};
#endif

#if defined(ESP8266) && defined(FASTLED_ALL_PINS_HARDWARE_SPI)
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SPIOutput : public ESP8266SPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};
#endif




#if defined(SPI_DATA) && defined(SPI_CLOCK)

#if defined(FASTLED_TEENSY3) && defined(ARM_HARDWARE_SPI)
// Generic fallback for generic pins on Teensy 3
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_SPEED>
class SPIOutput : public GenericSoftwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_SPEED> {};

// Specialization for hardware SPI pins
template<fl::u32 _SPI_SPEED>
class SPIOutput<SPI_DATA, SPI_CLOCK, _SPI_SPEED> : public fl::SPIDeviceProxy<SPI_DATA, SPI_CLOCK, _SPI_SPEED> {};

#elif defined(FASTLED_TEENSY4) && defined(ARM_HARDWARE_SPI)
// Specialized templates for each SPI peripheral on Teensy 4.x
// These provide the SPIClass reference and index required by SPIDeviceProxy
template<fl::u32 _SPI_SPEED>
class SPIOutput<SPI_DATA, SPI_CLOCK, _SPI_SPEED> : public fl::SPIDeviceProxy<SPI_DATA, SPI_CLOCK, _SPI_SPEED, SPI, 0> {};

template<fl::u32 _SPI_SPEED>
class SPIOutput<SPI1_DATA, SPI1_CLOCK, _SPI_SPEED> : public fl::SPIDeviceProxy<SPI1_DATA, SPI1_CLOCK, _SPI_SPEED, SPI1, 1> {};

template<fl::u32 _SPI_SPEED>
class SPIOutput<SPI2_DATA, SPI2_CLOCK, _SPI_SPEED> : public fl::SPIDeviceProxy<SPI2_DATA, SPI2_CLOCK, _SPI_SPEED, SPI2, 2> {};

#elif defined(FASTLED_TEENSYLC) && defined(ARM_HARDWARE_SPI)

#define DECLARE_SPI0(__DATA,__CLOCK) template<fl::u32 SPI_SPEED>\
 class SPIOutput<__DATA, __CLOCK, SPI_SPEED> : public ARMHardwareSPIOutput<__DATA, __CLOCK, SPI_SPEED, 0x40076000> {};
 #define DECLARE_SPI1(__DATA,__CLOCK) template<fl::u32 SPI_SPEED>\
  class SPIOutput<__DATA, __CLOCK, SPI_SPEED> : public ARMHardwareSPIOutput<__DATA, __CLOCK, SPI_SPEED, 0x40077000> {};

DECLARE_SPI0(7,13);
DECLARE_SPI0(8,13);
DECLARE_SPI0(11,13);
DECLARE_SPI0(12,13);
DECLARE_SPI0(7,14);
DECLARE_SPI0(8,14);
DECLARE_SPI0(11,14);
DECLARE_SPI0(12,14);
DECLARE_SPI1(0,20);
DECLARE_SPI1(1,20);
DECLARE_SPI1(21,20);

#elif defined(__SAM3X8E__) || defined(STM32F10X_MD) || defined(__STM32F1__) || defined(STM32F1) || defined(STM32F2XX) || defined(STM32F4)

template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_SPEED>
class SPIOutput : public fl::SPIDeviceProxy<_DATA_PIN, _CLOCK_PIN, _SPI_SPEED> {};

#elif defined(AVR_HARDWARE_SPI)

template<fl::u32 SPI_SPEED>
class SPIOutput<SPI_DATA, SPI_CLOCK, SPI_SPEED> : public AVRHardwareSPIOutput<SPI_DATA, SPI_CLOCK, SPI_SPEED> {};

#if defined(SPI_UART0_DATA)

template<fl::u32 SPI_SPEED>
class SPIOutput<SPI_UART0_DATA, SPI_UART0_CLOCK, SPI_SPEED> : public AVRUSART0SPIOutput<SPI_UART0_DATA, SPI_UART0_CLOCK, SPI_SPEED> {};

#endif

#if defined(SPI_UART1_DATA)

template<fl::u32 SPI_SPEED>
class SPIOutput<SPI_UART1_DATA, SPI_UART1_CLOCK, SPI_SPEED> : public AVRUSART1SPIOutput<SPI_UART1_DATA, SPI_UART1_CLOCK, SPI_SPEED> {};

#endif

#elif defined(ARDUNIO_CORE_SPI)

template<fl::u32 SPI_SPEED>
class SPIOutput<SPI_DATA, SPI_CLOCK, SPI_SPEED> : public ArdunioCoreSPIOutput<SPI_DATA, SPI_CLOCK, SPI_SPEED, SPI> {};

#endif

#else
#  if !defined(FASTLED_INTERNAL) && !defined(FASTLED_ALL_PINS_HARDWARE_SPI) && !defined(ESP32)
#    ifdef FASTLED_HAS_PRAGMA_MESSAGE
#      pragma message "WARNING: The SPI pins you chose have not been marked as hardware accelerated within the code base. All SPI access will default to bitbanged output. Consult the data sheet for hardware spi pins designed for efficient SPI transfer, typically via DMA / MOSI / SCK / SS pin"
#    else
#      warning "The SPI pins you chose have not been marked as hardware accelerated within the code base. All SPI access will default to bitbanged output. Consult the data sheet for hardware spi pins designed for efficient SPI transfer, typically via DMA / MOSI / SCK / SS pins"
#    endif
#  endif
#endif

// #if defined(USART_DATA) && defined(USART_CLOCK)
// template<uint32_t SPI_SPEED>
// class AVRSPIOutput<USART_DATA, USART_CLOCK, SPI_SPEED> : public AVRUSARTSPIOutput<USART_DATA, USART_CLOCK, SPI_SPEED> {};
// #endif

#else
#  if !defined(FASTLED_INTERNAL) && !defined(FASTLED_ALL_PINS_HARDWARE_SPI)
#    ifdef FASTLED_HAS_PRAGMA_MESSAGE
#      pragma message "Forcing software SPI - no hardware accelerated SPI for you!"
#    else
#      warning "Forcing software SPI - no hardware accelerated SPI for you!"
#    endif
#  endif
#endif

#endif  // !defined(FASTLED_STUB_IMPL)

FASTLED_NAMESPACE_END

#endif
