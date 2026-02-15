#pragma once

// ok no namespace fl

/// @file spi_output_template.h
/// Platform dispatch header for SPI output template implementations.
///
/// This header includes the appropriate SPIOutput template implementation
/// based on the target platform. Each platform file defines the SPIOutput
/// template optimized for its hardware SPI peripheral.
///
/// This must be included before any SPI chipset controllers (e.g., APA102, P9813).
///
/// Uses standardized FL_IS_* macros from is_platform.h for platform detection.

#include "platforms/is_platform.h"

// Platform dispatch (coarse-to-fine detection)
#if defined(FL_IS_WASM) || defined(FL_IS_STUB)
#include "platforms/stub/spi_output_template.h"

#elif defined(FL_IS_ESP32)
#include "platforms/esp/32/drivers/spi/spi_output_template.h"

#elif defined(FL_IS_ESP8266)
#include "platforms/esp/8266/spi_output_template.h"

#elif defined(NRF51)
#include "platforms/arm/nrf51/spi_output_template.h"

#elif defined(FL_IS_NRF52)
#include "platforms/arm/nrf52/spi_output_template.h"

#elif defined(FL_IS_APOLLO3) && defined(FASTLED_ALL_PINS_HARDWARE_SPI)
#include "platforms/apollo3/spi_output_template.h"

#elif defined(FL_IS_TEENSY_3X)
#include "platforms/arm/teensy/teensy3_common/spi_output_template.h"

#elif defined(FL_IS_TEENSY_4X)
#include "platforms/arm/teensy/teensy4_common/spi_output_template.h"

#elif defined(FL_IS_TEENSY_LC)
#include "platforms/arm/teensy/teensy_lc/spi_output_template.h"

#elif defined(FL_IS_SAM) || defined(FL_IS_SAMD)
#include "platforms/arm/sam/spi_output_template.h"

#elif defined(FL_IS_STM32)
#include "platforms/arm/stm32/spi_output_template.h"

#elif defined(FL_IS_AVR)
#include "platforms/avr/spi_output_template.h"

#else
// Fallback: Generic software SPI for unsupported platforms
#include "platforms/shared/spi_bitbang/spi_output_template.h"
#endif
