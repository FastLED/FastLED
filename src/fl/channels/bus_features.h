#pragma once

/// @file bus_features.h
/// @brief Zero-filled feature macros for portable bus selection.

#include "platforms/is_platform.h"

#if defined(FL_IS_ESP32)
#include "platforms/esp/32/feature_flags/enabled.h"  // IWYU pragma: keep  // ok platform headers
#endif

#ifndef FASTLED_HAS_OBJECTFLED
#if defined(FL_IS_TEENSY_4X)
#define FASTLED_HAS_OBJECTFLED 1
#else
#define FASTLED_HAS_OBJECTFLED 0
#endif
#endif

#ifndef FASTLED_HAS_FLEXIO
#if defined(FL_IS_TEENSY_4X)
#define FASTLED_HAS_FLEXIO 1
#else
#define FASTLED_HAS_FLEXIO 0
#endif
#endif

#ifndef FASTLED_HAS_PARLIO
#if defined(FASTLED_ESP32_HAS_PARLIO)
#define FASTLED_HAS_PARLIO FASTLED_ESP32_HAS_PARLIO
#else
#define FASTLED_HAS_PARLIO 0
#endif
#endif

#ifndef FASTLED_HAS_LCD_CAM_CLOCKLESS
#if defined(FASTLED_ESP32_HAS_I2S_LCD_CAM)
#define FASTLED_HAS_LCD_CAM_CLOCKLESS FASTLED_ESP32_HAS_I2S_LCD_CAM
#else
#define FASTLED_HAS_LCD_CAM_CLOCKLESS 0
#endif
#endif

#ifndef FASTLED_HAS_LCD_SPI
#if defined(FASTLED_ESP32_HAS_LCD_SPI)
#define FASTLED_HAS_LCD_SPI FASTLED_ESP32_HAS_LCD_SPI
#else
#define FASTLED_HAS_LCD_SPI 0
#endif
#endif

#ifndef FASTLED_HAS_I2S_LED
#if defined(FASTLED_ESP32_HAS_I2S_LCD_CAM)
#define FASTLED_HAS_I2S_LED FASTLED_ESP32_HAS_I2S_LCD_CAM
#else
#define FASTLED_HAS_I2S_LED 0
#endif
#endif

#ifndef FASTLED_HAS_I2S_SPI
#if defined(FASTLED_ESP32_HAS_I2S)
#define FASTLED_HAS_I2S_SPI FASTLED_ESP32_HAS_I2S
#else
#define FASTLED_HAS_I2S_SPI 0
#endif
#endif

#ifndef FASTLED_HAS_RMT4
#if defined(FASTLED_ESP32_HAS_RMT) && FASTLED_ESP32_HAS_RMT && \
    defined(FASTLED_RMT5) && !FASTLED_RMT5
#define FASTLED_HAS_RMT4 1
#else
#define FASTLED_HAS_RMT4 0
#endif
#endif

#ifndef FASTLED_HAS_RMT5
#ifdef FASTLED_RMT5
#define FASTLED_HAS_RMT5 FASTLED_RMT5
#else
#define FASTLED_HAS_RMT5 0
#endif
#endif

#ifndef FASTLED_HAS_LPSPI
#if defined(FL_IS_TEENSY_4X)
#define FASTLED_HAS_LPSPI 1
#else
#define FASTLED_HAS_LPSPI 0
#endif
#endif

#ifndef FASTLED_HAS_LPUART_PARALLEL
#if defined(FL_IS_TEENSY_4X)
#define FASTLED_HAS_LPUART_PARALLEL 1
#else
#define FASTLED_HAS_LPUART_PARALLEL 0
#endif
#endif

#ifndef FASTLED_HAS_PIO
#define FASTLED_HAS_PIO 0
#endif

#ifndef FASTLED_HAS_PIO1
#define FASTLED_HAS_PIO1 0
#endif

#ifndef FASTLED_HAS_SOFT_SPI
#define FASTLED_HAS_SOFT_SPI 0
#endif

#ifndef FASTLED_HAS_SOFT_UART
#define FASTLED_HAS_SOFT_UART 0
#endif
