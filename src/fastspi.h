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

/// Software SPI output (generic cross-platform bit-banging)
template<fl::u8 _DATA_PIN, fl::u8 _CLOCK_PIN, fl::u32 _SPI_CLOCK_DIVIDER>
class SoftwareSPIOutput : public GenericSoftwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};

// Platform-specific SPIOutput template definitions
// Each platform includes its own spi_output_template.h which defines the SPIOutput template
// This eliminates platform-specific preprocessor logic from this file and makes it
// easier to add/maintain platforms by editing their own platform directory files

#if defined(FASTLED_STUB_IMPL)
#include "platforms/stub/spi_output_template.h"

#elif defined(ESP32) || defined(ESP32S2) || defined(ESP32S3) || defined(ESP32C3) || defined(ESP32P4)
#include "platforms/esp/32/spi_output_template.h"

#elif defined(ESP8266)
#include "platforms/esp/8266/spi_output_template.h"

#elif defined(NRF51)
#include "platforms/arm/nrf51/spi_output_template.h"

#elif defined(NRF52_SERIES)
#include "platforms/arm/nrf52/spi_output_template.h"

#elif defined(FASTLED_APOLLO3) && defined(FASTLED_ALL_PINS_HARDWARE_SPI)
#include "platforms/apollo3/spi_output_template.h"

#elif defined(FASTLED_TEENSY3) && defined(ARM_HARDWARE_SPI)
#include "platforms/arm/teensy/teensy3_common/spi_output_template.h"

#elif defined(FASTLED_TEENSY4) && defined(ARM_HARDWARE_SPI)
#include "platforms/arm/teensy/teensy4_common/spi_output_template.h"

#elif defined(FASTLED_TEENSYLC) && defined(ARM_HARDWARE_SPI)
#include "platforms/arm/teensy/teensy_lc/spi_output_template.h"

#elif defined(__SAM3X8E__) || defined(__SAMD21G18A__) || defined(__SAMD21J18A__) || defined(__SAMD21E17A__) || \
      defined(__SAMD21E18A__) || defined(__SAMD51G19A__) || defined(__SAMD51J19A__) || defined(__SAME51J19A__) || \
      defined(__SAMD51P19A__) || defined(__SAMD51P20A__)
#include "platforms/arm/sam/spi_output_template.h"

#elif defined(STM32F10X_MD) || defined(__STM32F1__) || defined(STM32F1) || defined(STM32F2XX) || defined(STM32F4)
#include "platforms/arm/stm32/spi_output_template.h"

#elif defined(AVR_HARDWARE_SPI) || defined(ARDUNIO_CORE_SPI)
#include "platforms/avr/spi_output_template.h"

#else
// Fallback: Generic software SPI for unsupported platforms
#include "platforms/shared/spi_bitbang/spi_output_template.h"

#  if !defined(FASTLED_INTERNAL) && !defined(FASTLED_ALL_PINS_HARDWARE_SPI)
#    ifdef FASTLED_HAS_PRAGMA_MESSAGE
#      pragma message "WARNING: No hardware SPI support for this platform. Using generic software SPI (bit-banging)."
#    else
#      warning "WARNING: No hardware SPI support for this platform. Using generic software SPI (bit-banging)."
#    endif
#  endif
#endif

FASTLED_NAMESPACE_END

#endif
