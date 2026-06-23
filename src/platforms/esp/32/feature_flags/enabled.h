// ok no namespace fl
#pragma once

// IWYU pragma: private

#include "platforms/is_platform.h"
#if defined(FL_IS_ESP32)
#include "fl/stl/compiler_control.h"
#include "fl/stl/has_include.h"
#include "sdkconfig.h"
#include "platforms/esp/esp_version.h"

// Include ESP-IDF SoC capability macros for hardware feature detection
// Note: soc/soc_caps.h only exists in ESP-IDF 4.0+
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
FL_EXTERN_C_BEGIN
// IWYU pragma: begin_keep
#include "soc/soc_caps.h"
// IWYU pragma: end_keep
FL_EXTERN_C_END
#endif

// Provide safe defaults for SoC capabilities if not defined
#ifndef SOC_RMT_SUPPORTED
#define SOC_RMT_SUPPORTED 0
#endif

// SOC_CPU_CORES_NUM is defined in soc/soc_caps.h on IDF 4.0+. For IDF 3.x
// (where soc_caps.h does not exist) fall back to explicit variant checks.
// Original ESP32 / ESP32-S3 / ESP32-P4 are dual-core; everything else is
// single-core from an SMP-FreeRTOS perspective.
#ifndef SOC_CPU_CORES_NUM
#if defined(FL_IS_ESP_32DEV) || defined(FL_IS_ESP_32S3) || defined(FL_IS_ESP_32P4)
#define SOC_CPU_CORES_NUM 2
#else
#define SOC_CPU_CORES_NUM 1
#endif
#endif

// FL_CPU_CORES: Number of CPU cores available for RTOS task pinning.
// FL_HAS_MULTICORE_AFFINITY: 1 when the platform supports xTaskCreatePinnedToCore
// with a core argument other than tskNO_AFFINITY (i.e. has >=2 cores).
//
// Used by platforms/esp/32/coroutine_esp32.impl.hpp to validate
// CoroutineConfig::core_id. Out-of-range requests fall back to tskNO_AFFINITY.
#ifndef FL_CPU_CORES
#define FL_CPU_CORES SOC_CPU_CORES_NUM
#endif
#ifndef FL_HAS_MULTICORE_AFFINITY
#if FL_CPU_CORES > 1
#define FL_HAS_MULTICORE_AFFINITY 1
#else
#define FL_HAS_MULTICORE_AFFINITY 0
#endif
#endif

#ifndef SOC_PARLIO_SUPPORTED
#define SOC_PARLIO_SUPPORTED 0
#endif

#ifndef SOC_WIFI_SUPPORTED
#define SOC_WIFI_SUPPORTED 0
#endif

// FL_HAS_NETWORK: True when the chip has WiFi hardware and IDF 4.0+ is available.
//
// Used to gate:
//   - lwip_hook_ip6_input() stub (required when CONFIG_LWIP_HOOK_IP6_INPUT=CUSTOM)
//   - Other networking-dependent code that would cause linker failures on
//     non-WiFi chips (ESP32-H2, ESP32-P4) or older IDF toolchains.
//
// SOC_WIFI_SUPPORTED is 0 on ESP32-H2 and ESP32-P4 (no WiFi silicon).
// All other ESP32 variants have WiFi and SOC_WIFI_SUPPORTED=1.
#if !defined(FL_HAS_NETWORK)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0) && SOC_WIFI_SUPPORTED
#define FL_HAS_NETWORK 1
#else
#define FL_HAS_NETWORK 0
#endif
#endif

#if !defined(FASTLED_ESP32_HAS_CLOCKLESS_SPI)
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0) || defined(FL_IS_ESP8266)
#define FASTLED_ESP32_HAS_CLOCKLESS_SPI 0
#else
#define FASTLED_ESP32_HAS_CLOCKLESS_SPI 1
#endif
#endif

// SPI wave8 encoding (Stage 6 refactoring)
// Replaces bit-by-bit encodeLedByte() with fast wave8 lookup tables
// Note: wave8 encoding is always enabled (FASTLED_SPI_USE_WAVE8 flag removed)

// RMT driver availability - use SoC capability macro
#if !defined(FASTLED_ESP32_HAS_RMT)
#define FASTLED_ESP32_HAS_RMT SOC_RMT_SUPPORTED
#endif

// Helper macro: Platforms that ONLY support RMT5 (no RMT4 fallback)
// These chips have newer RMT architecture incompatible with legacy RMT4 driver
#if defined(FL_IS_ESP_32C6) || defined(FL_IS_ESP_32C5) || \
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
#if defined(FL_IS_ESP_32P4) && FL_HAS_INCLUDE("esp_lcd_panel_rgb.h")
#define FASTLED_ESP32_HAS_LCD_RGB 1
#else
#define FASTLED_ESP32_HAS_LCD_RGB 0
#endif
#endif

// I2S LCD_CAM driver availability for LED output (Yves driver channel API port)
// I2S LCD_CAM peripheral is available on ESP32-S3 (uses LCD_CAM via I80 bus for parallel LED driving)
// Note: This is EXPERIMENTAL and lower priority than other drivers
// The LCD_CAM peripheral is ESP32-S3 specific - it does NOT exist on other ESP32 variants
#if !defined(FASTLED_ESP32_HAS_I2S_LCD_CAM)
#if defined(FL_IS_ESP_32S3)
#define FASTLED_ESP32_HAS_I2S_LCD_CAM 1
#else
#define FASTLED_ESP32_HAS_I2S_LCD_CAM 0
#endif
#endif

// LCD_SPI driver availability for clocked SPI LED output via LCD_CAM (ESP32-S3)
// Uses the LCD_CAM I80 bus with PCLK as SPI clock for parallel SPI chipsets (APA102, SK9822)
#if !defined(FASTLED_ESP32_HAS_LCD_SPI)
#if defined(FL_IS_ESP_32S3)
#define FASTLED_ESP32_HAS_LCD_SPI 1
#else
#define FASTLED_ESP32_HAS_LCD_SPI 0
#endif
#endif

// I2S parallel mode driver availability
// I2S parallel mode is only available on original ESP32 (ESP32dev).
// ESP32-S2 has an I2S peripheral but with different register layout (no clka_en).
// Other variants (S3, C2, C3, C5, C6, H2, P4) have completely different I2S architecture.
// Also disabled on ESP-IDF 6.0+ (PERIPH_I2S1_MODULE removed, needs LL API port).
// Users can override with -D FASTLED_ESP32_HAS_I2S=0 to disable.
#if !defined(FASTLED_ESP32_HAS_I2S)
#if defined(FL_IS_ESP32) && !defined(FL_IS_ESP_32S2) && !defined(FL_IS_ESP_32S3) && !defined(FL_IS_ESP_32C2) && !defined(FL_IS_ESP_32C3) && !defined(FL_IS_ESP_32C5) && !defined(FL_IS_ESP_32C6) && !defined(FL_IS_ESP_32H2) && !defined(FL_IS_ESP_32P4) && !ESP_IDF_VERSION_6_OR_HIGHER
#define FASTLED_ESP32_HAS_I2S 1
#else
#define FASTLED_ESP32_HAS_I2S 0
#endif
#endif

// ============================================================================
// Channel driver force macros
// ============================================================================
// These macros force a specific channel driver to be the highest priority,
// overriding the default priority-based selection. Define before including
// FastLED.h to force a specific driver for all LED output.
//
// FASTLED_ESP32_FORCE_I2S_LCD_CAM - Force I2S LCD_CAM driver (ESP32-S3 clockless)
// FASTLED_ESP32_FORCE_I2S_SPI    - Force I2S parallel SPI driver (ESP32dev, true SPI chipsets)
// FASTLED_ESP32_FORCE_LCD_SPI    - Force LCD_CAM SPI driver (ESP32-S3, true SPI chipsets)

// Legacy macro migration: map old macros to new channel driver force macros
// FASTLED_USES_ESP32S3_I2S (removed) → now forces I2S LCD_CAM channel driver
// FASTLED_ESP32_LCD_DRIVER (broken)  → now forces I2S LCD_CAM channel driver
#if defined(FASTLED_USES_ESP32S3_I2S) && !defined(FASTLED_ESP32_FORCE_I2S_LCD_CAM)
#define FASTLED_ESP32_FORCE_I2S_LCD_CAM 1
#endif
#if defined(FASTLED_ESP32_LCD_DRIVER) && !defined(FASTLED_ESP32_FORCE_I2S_LCD_CAM)
#define FASTLED_ESP32_FORCE_I2S_LCD_CAM 1
#endif

#endif  // FL_IS_ESP32
