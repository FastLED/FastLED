/// @file spi_hw_base.h
/// @brief Common ESP32 SPI hardware definitions and compatibility macros
///
/// This header provides shared definitions used across all ESP32 SPI implementations
/// (single, dual, quad, octal) to avoid code duplication.

#pragma once

// ok no namespace fl

#include "platforms/esp/is_esp.h"

#if defined(FL_IS_ESP32)

// Include ESP-IDF headers first to detect what's already defined
#include "fl/has_include.h"
#if FL_HAS_INCLUDE(<soc/soc_caps.h>)
  #include <soc/soc_caps.h>
#endif
#if FL_HAS_INCLUDE(<driver/spi_common.h>)
  #include <driver/spi_common.h>  // For spi_host_device_t, spi_common_dma_t
#endif
#if FL_HAS_INCLUDE(<hal/spi_types.h>)
  #include <hal/spi_types.h>  // For SPI2_HOST, SPI3_HOST (modern ESP-IDF)
#endif

// Include driver/spi_master.h for spi_host_device_t type definition
// Required for SPI2_HOST/SPI3_HOST macro definitions below
#include <driver/spi_master.h>

// Determine SPI3_HOST availability using SOC capability macro
// SOC_SPI_PERIPH_NUM indicates the number of SPI peripherals available
// SPI3_HOST is available when SOC_SPI_PERIPH_NUM > 2 (SPI1, SPI2, SPI3)
#ifndef SOC_SPI_PERIPH_NUM
    #define SOC_SPI_PERIPH_NUM 2  // Default to 2 for older ESP-IDF versions
#endif

// ESP-IDF 3.3 compatibility: SPI_DMA_CH_AUTO was added in IDF 4.0
// The correct value is 3 (from spi_common_dma_t enum: SPI_DMA_CH_AUTO = 3)
// This matches the ESP-IDF definition: SPI_DMA_DISABLED=0, SPI_DMA_CH1=1, SPI_DMA_CH2=2, SPI_DMA_CH_AUTO=3
// Note: Only define if not already defined by ESP-IDF headers
#ifndef SPI_DMA_CH_AUTO
    #define SPI_DMA_CH_AUTO 3
#endif

// ESP-IDF compatibility: Ensure SPI host constants are defined
// Modern IDF defines these as enum values in hal/spi_types.h (included above)
// Only define as macros for very old ESP-IDF versions that don't have them
#ifndef SPI2_HOST
    #define SPI2_HOST ((spi_host_device_t)1)
#endif

// SPI3_HOST is only available on chips with more than 2 SPI peripherals
// ESP32-C3/C6/H2 only have 2 SPI peripherals (SPI0/1 flash, SPI2 general)
// ESP32/S2/S3/P4 have 3+ SPI peripherals (SPI2 and SPI3 for general use)
#if SOC_SPI_PERIPH_NUM > 2
  #ifndef SPI3_HOST
      #define SPI3_HOST ((spi_host_device_t)2)
  #endif
  #define FASTLED_ESP32_HAS_SPI3 1
#else
  #define FASTLED_ESP32_HAS_SPI3 0
#endif

#endif  // FL_IS_ESP32
