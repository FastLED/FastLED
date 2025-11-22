/// @file spi_hw_base.h
/// @brief Common ESP32 SPI hardware definitions and compatibility macros
///
/// This header provides shared definitions used across all ESP32 SPI implementations
/// (single, dual, quad, octal) to avoid code duplication.

#pragma once

#include "platforms/esp/is_esp.h"

#if defined(FL_IS_ESP32)

// Include soc_caps.h if available (ESP-IDF 4.0+)
// Older versions (like IDF 3.3) don't have this header
#include "fl/has_include.h"
#if FL_HAS_INCLUDE(<soc/soc_caps.h>)
  #include <soc/soc_caps.h>
#endif

// Determine SPI3_HOST availability using SOC capability macro
// SOC_SPI_PERIPH_NUM indicates the number of SPI peripherals available
// SPI3_HOST is available when SOC_SPI_PERIPH_NUM > 2 (SPI1, SPI2, SPI3)
#ifndef SOC_SPI_PERIPH_NUM
    #define SOC_SPI_PERIPH_NUM 2  // Default to 2 for older ESP-IDF versions
#endif

// ESP-IDF 3.3 compatibility: SPI_DMA_CH_AUTO was added in IDF 4.0
// On older versions, use DMA channel 1 as default
#ifndef SPI_DMA_CH_AUTO
    #define SPI_DMA_CH_AUTO 1
#endif

// ESP-IDF compatibility: Ensure SPI host constants are defined
// Modern IDF uses SPI2_HOST/SPI3_HOST, older versions may not have them
#ifndef SPI2_HOST
    #define SPI2_HOST ((spi_host_device_t)1)
#endif
#ifndef SPI3_HOST
    #define SPI3_HOST ((spi_host_device_t)2)
#endif

#endif  // FL_IS_ESP32
