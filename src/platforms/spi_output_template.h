#pragma once

// ok no namespace fl

/// @file spi_output_template.h
/// Platform-specific SPI output template dispatch header.
/// This header includes the appropriate SPIOutput template implementation
/// based on the target platform.
///
/// Each platform file defines the SPIOutput template for its platform.
/// This must be included before any SPI chipset controllers (e.g., APA102, P9813).

#include "platforms/arm/teensy/is_teensy.h"

#if defined(__EMSCRIPTEN__) || defined(FASTLED_STUB_IMPL)
#include "platforms/stub/spi_output_template.h"

#elif defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
#include "platforms/esp/32/drivers/spi/spi_output_template.h"

#elif defined(ESP8266)
#include "platforms/esp/8266/spi_output_template.h"

#elif defined(NRF51)
#include "platforms/arm/nrf51/spi_output_template.h"

#elif defined(NRF52_SERIES)
#include "platforms/arm/nrf52/spi_output_template.h"

#elif defined(FASTLED_APOLLO3) && defined(FASTLED_ALL_PINS_HARDWARE_SPI)
#include "platforms/apollo3/spi_output_template.h"

#elif defined(FL_IS_TEENSY_3X)
#include "platforms/arm/teensy/teensy3_common/spi_output_template.h"

#elif defined(FL_IS_TEENSY_4X)
#include "platforms/arm/teensy/teensy4_common/spi_output_template.h"

#elif defined(FL_IS_TEENSY_LC)
#include "platforms/arm/teensy/teensy_lc/spi_output_template.h"

#elif defined(__SAM3X8E__) || defined(__SAMD21G18A__) || defined(__SAMD21J18A__) || defined(__SAMD21E17A__) || \
      defined(__SAMD21E18A__) || defined(__SAMD51G19A__) || defined(__SAMD51J19A__) || defined(__SAME51J19A__) || \
      defined(__SAMD51P19A__) || defined(__SAMD51P20A__)
#include "platforms/arm/sam/spi_output_template.h"

#elif defined(STM32F10X_MD) || defined(__STM32F1__) || defined(STM32F1) || defined(STM32F1xx) || defined(STM32F2XX) || defined(STM32F4)
#include "platforms/arm/stm32/spi_output_template.h"

#elif defined(AVR_HARDWARE_SPI) || defined(ARDUNIO_CORE_SPI)
#include "platforms/avr/spi_output_template.h"

#else
// Fallback: Generic software SPI for unsupported platforms
#include "platforms/shared/spi_bitbang/spi_output_template.h"
#endif
