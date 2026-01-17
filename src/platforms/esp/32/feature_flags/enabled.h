// ok no namespace fl
#pragma once

#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
#include "sdkconfig.h"
#include "platforms/esp/esp_version.h"

// Include ESP-IDF SoC capability macros for hardware feature detection
// Note: soc/soc_caps.h only exists in ESP-IDF 4.0+
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
#include "fl/compiler_control.h"
FL_EXTERN_C_BEGIN
#include "soc/soc_caps.h"
FL_EXTERN_C_END
#endif

// Provide safe defaults for SoC capabilities if not defined
#ifndef SOC_RMT_SUPPORTED
#define SOC_RMT_SUPPORTED 0
#endif

#ifndef SOC_PARLIO_SUPPORTED
#define SOC_PARLIO_SUPPORTED 0
#endif

#if !defined(FASTLED_ESP32_HAS_CLOCKLESS_SPI)
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0) || defined(CONFIG_IDF_TARGET_ESP8266)
#define FASTLED_ESP32_HAS_CLOCKLESS_SPI 0
#else
#define FASTLED_ESP32_HAS_CLOCKLESS_SPI 1
#endif
#endif

// SPI wave8 encoding (Stage 6 refactoring)
// Replaces bit-by-bit encodeLedByte() with fast wave8 lookup tables
// Note: wave8 encoding is always enabled (FASTLED_SPI_USE_WAVE8 flag removed)

// RMT driver availability - use SoC capability macro
#define FASTLED_ESP32_HAS_RMT SOC_RMT_SUPPORTED

// Helper macro: Platforms that ONLY support RMT5 (no RMT4 fallback)
// These chips have newer RMT architecture incompatible with legacy RMT4 driver
#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32C5) || \
    defined(CONFIG_IDF_TARGET_ESP32P4) || defined(CONFIG_IDF_TARGET_ESP32H2)
#define FASTLED_ESP32_RMT5_ONLY_PLATFORM 1
#else
#define FASTLED_ESP32_RMT5_ONLY_PLATFORM 0
#endif

// Note that FASTLED_RMT5 is a legacy name,
// so we keep it because "RMT" is specific to ESP32
// Auto-detect RMT5 based on ESP-IDF version if not explicitly defined
#if !defined(FASTLED_RMT5)
#if FASTLED_ESP32_RMT5_ONLY_PLATFORM
#define FASTLED_RMT5 1  // RMT5-only chips must use new driver
#elif ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0) && FASTLED_ESP32_HAS_RMT
#define FASTLED_RMT5 1
#else
#define FASTLED_RMT5 0  // Legacy driver or no RMT
#endif
#endif

// PARLIO driver availability - use SoC capability macro
// PARLIO requires ESP-IDF 5.0+ and hardware support (ESP32-P4, C6, H2, C5)
// Note: ESP32-S3 does NOT have PARLIO hardware (it has LCD peripheral instead)
#if !defined(FASTLED_ESP32_HAS_PARLIO)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#define FASTLED_ESP32_HAS_PARLIO SOC_PARLIO_SUPPORTED
#else
#define FASTLED_ESP32_HAS_PARLIO 0  // Requires ESP-IDF 5.0+
#endif
#endif

// UART driver availability for LED output
// UART is available on all ESP32 variants (C3, S3, C6, H2, P4, etc.)
// Requires ESP-IDF 4.0+ for DMA support
// Note: Uses wave8 encoding adapted for UART framing (start/stop bits)
#if !defined(FASTLED_ESP32_HAS_UART)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
#define FASTLED_ESP32_HAS_UART 1  // All ESP32 variants have UART
#else
#define FASTLED_ESP32_HAS_UART 0  // Requires ESP-IDF 4.0+ for DMA
#endif
#endif

// LCD RGB driver availability for LED output
// LCD RGB peripheral is only available on ESP32-P4 (uses RGB LCD peripheral for parallel LED driving)
// Requires ESP-IDF 5.0+ and esp_lcd_panel_rgb.h header
#if !defined(FASTLED_ESP32_HAS_LCD_RGB)
#if defined(CONFIG_IDF_TARGET_ESP32P4) && __has_include("esp_lcd_panel_rgb.h")
#define FASTLED_ESP32_HAS_LCD_RGB 1
#else
#define FASTLED_ESP32_HAS_LCD_RGB 0
#endif
#endif

// I2S LCD_CAM driver availability for LED output (Yves driver channel API port)
// I2S LCD_CAM peripheral is available on ESP32-S3 (uses LCD_CAM via I80 bus for parallel LED driving)
// Requires ESP-IDF 5.0+ and esp_lcd_panel_io.h header
// Note: This is EXPERIMENTAL and lower priority than other drivers
#if !defined(FASTLED_ESP32_HAS_I2S_LCD_CAM)
#if defined(CONFIG_IDF_TARGET_ESP32S3) && __has_include("esp_lcd_panel_io.h")
#define FASTLED_ESP32_HAS_I2S_LCD_CAM 1
#else
#define FASTLED_ESP32_HAS_I2S_LCD_CAM 0
#endif
#endif

#endif  // ESP32 || ARDUINO_ARCH_ESP32
