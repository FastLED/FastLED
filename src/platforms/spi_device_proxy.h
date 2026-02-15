#pragma once

// ok no namespace fl

/// @file spi_device_proxy.h
/// Platform dispatch header for SPI device proxy implementations.
///
/// This header includes the appropriate SPIDeviceProxy implementation
/// based on the target platform. Not all platforms use device proxies -
/// only those with advanced GPIO matrix or runtime SPI routing capabilities.
///
/// Platforms with SPI device proxy support:
/// - ESP32: GPIO matrix allows any pin to be routed to SPI peripheral
/// - Teensy 4.x: LPSPI with flexible pin muxing
/// - nRF52: SPIM with configurable pins
/// - SAM/SAMD: SERCOM SPI with pin multiplexing
/// - STM32: SPI with alternate function pin mapping

#include "platforms/is_platform.h"

#if defined(FL_IS_ESP32)
#include "platforms/esp/32/drivers/spi/spi_device_proxy.h"

#elif defined(FL_IS_TEENSY_4X)
#include "platforms/arm/mxrt1062/spi_device_proxy.h"

#elif defined(FL_IS_NRF52)
#include "platforms/arm/nrf52/spi_device_proxy.h"

#elif defined(FL_IS_SAM) || defined(FL_IS_SAMD)
#include "platforms/arm/sam/spi_device_proxy.h"

#elif defined(FL_IS_STM32)
#include "platforms/arm/stm32/spi_device_proxy.h"

#endif

// No fallback needed - SPI device proxy is optional per platform
// Platforms without a device proxy use direct hardware SPI access
