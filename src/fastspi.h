#pragma once

/// @file fastspi.h
/// Serial peripheral interface (SPI) definitions per platform
///
/// ARCHITECTURE:
/// This file provides:
/// 1. Common SPI macros and utilities (DATA_RATE_MHZ, DATA_RATE_KHZ)
/// 2. SoftwareSPIOutput template (generic bit-banging SPI)
/// 3. Includes all platform-specific implementations
///
/// IMPORTANT: The SPIOutput template is NOT defined in this file.
/// Instead, it is defined by platform-specific spi_output_template.h files:
/// - Each platform (esp32, teensy, nrf52, etc.) has its own platform directory
/// - Each platform directory contains spi_output_template.h
/// - When that file is included, it defines SPIOutput for that platform
/// - This approach centralizes platform logic in platform-specific directories
///
/// The include pattern is:
/// #include "fastspi.h"  // Includes this file
/// // At this point, SoftwareSPIOutput is available
/// // SPIOutput is defined (by whichever platform-specific template was included)

#ifndef __INC_FASTSPI_H
#define __INC_FASTSPI_H

// ============================================================================
// ALL INCLUDES FIRST (before any namespace declarations)
// This satisfies the namespace-include-order linting requirement
// ============================================================================

#include "controller.h"
#include "lib8tion.h"
#include "platforms/shared/spi_bitbang/generic_software_spi.h"
#include "fl/int.h"
#include "FastLED.h"

// Include platform-specific SPI device proxy implementations
// These provide the hardware SPI abstractions for each platform
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

// Include platform-specific SPIOutput template implementations
// Each of these files defines the SPIOutput template for its platform
// NOTE: These files must NOT have their own namespace wrappers - they define
// templates/classes at the global scope for use by FastLED
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

// ============================================================================
// DATA RATE MACROS (platform-specific clock calculations)
// ============================================================================

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

// ============================================================================
// GENERIC SOFTWARE SPI OUTPUT
// ============================================================================
// This template provides bit-banging SPI for any platform that supports GPIO
// It's a fallback for platforms without dedicated hardware SPI support

/// Software SPI output (generic cross-platform bit-banging)
/// NOTE: This is NOT in the fl namespace per fastspi.h design requirements
template<u8 _DATA_PIN, u8 _CLOCK_PIN, u32 _SPI_CLOCK_DIVIDER>
class SoftwareSPIOutput : public GenericSoftwareSPIOutput<_DATA_PIN, _CLOCK_PIN, _SPI_CLOCK_DIVIDER> {};

#endif
