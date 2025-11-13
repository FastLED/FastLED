#pragma once

// ok no namespace fl

/// @file is_esp.h
/// ESP platform detection header
///
/// This header detects ESP8266 and ESP32 platforms (all variants) by checking
/// compiler-defined macros and defines standardized FL_IS_ESP* macros.
///
/// Also provides ESP-IDF toolchain version detection macros.
///
/// Defines:
/// - FL_IS_ESP: General ESP platform (ESP8266 or ESP32)
/// - FL_IS_ESP8266: ESP8266 platform
/// - FL_IS_ESP32: ESP32 platform (any variant)
/// - FL_IS_ESP_32DEV: Original ESP32 (dual-core Xtensa LX6)
/// - FL_IS_ESP_32S2: ESP32-S2 (single-core Xtensa LX7)
/// - FL_IS_ESP_32S3: ESP32-S3 (dual-core Xtensa LX7)
/// - FL_IS_ESP_32C2: ESP32-C2 (RISC-V)
/// - FL_IS_ESP_32C3: ESP32-C3 (RISC-V)
/// - FL_IS_ESP_32C5: ESP32-C5 (RISC-V)
/// - FL_IS_ESP_32C6: ESP32-C6 (RISC-V)
/// - FL_IS_ESP_32H2: ESP32-H2 (RISC-V)
/// - FL_IS_ESP_32P4: ESP32-P4 (dual-core RISC-V)
/// - FL_IS_IDF_4_OR_HIGHER: ESP-IDF 4.x or higher
/// - FL_IS_IDF_5_OR_HIGHER: ESP-IDF 5.x or higher

// Include ESP-IDF version detection (normalizes version macros)
#include "esp_version.h"

// ============================================================================
// FL_IS_ESP8266 - ESP8266 platform detection
// ============================================================================
#if defined(ESP8266) || defined(ARDUINO_ARCH_ESP8266)
#define FL_IS_ESP8266
#endif

// ============================================================================
// FL_IS_ESP32 - ESP32 platform detection (any variant)
// ============================================================================
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32) || \
    defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2) || \
    defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C2) || \
    defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C5) || \
    defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || \
    defined(CONFIG_IDF_TARGET_ESP32P4)
#define FL_IS_ESP32
#endif

// ============================================================================
// FL_IS_ESP - General ESP platform detection (ESP8266 or ESP32)
// ============================================================================
#if defined(FL_IS_ESP8266) || defined(FL_IS_ESP32)
#define FL_IS_ESP
#endif

// ============================================================================
// ESP32 Variant Detection - Specific chip identification
// ============================================================================

// FL_IS_ESP_32DEV - Original ESP32 (dual-core Xtensa LX6)
#if defined(CONFIG_IDF_TARGET_ESP32)
#define FL_IS_ESP_32DEV
#endif

// FL_IS_ESP_32S2 - ESP32-S2 (single-core Xtensa LX7)
#if defined(CONFIG_IDF_TARGET_ESP32S2)
#define FL_IS_ESP_32S2
#endif

// FL_IS_ESP_32S3 - ESP32-S3 (dual-core Xtensa LX7)
#if defined(CONFIG_IDF_TARGET_ESP32S3)
#define FL_IS_ESP_32S3
#endif

// FL_IS_ESP_32C2 - ESP32-C2 (RISC-V)
#if defined(CONFIG_IDF_TARGET_ESP32C2)
#define FL_IS_ESP_32C2
#endif

// FL_IS_ESP_32C3 - ESP32-C3 (RISC-V)
#if defined(CONFIG_IDF_TARGET_ESP32C3)
#define FL_IS_ESP_32C3
#endif

// FL_IS_ESP_32C5 - ESP32-C5 (RISC-V)
#if defined(CONFIG_IDF_TARGET_ESP32C5)
#define FL_IS_ESP_32C5
#endif

// FL_IS_ESP_32C6 - ESP32-C6 (RISC-V)
#if defined(CONFIG_IDF_TARGET_ESP32C6)
#define FL_IS_ESP_32C6
#endif

// FL_IS_ESP_32H2 - ESP32-H2 (RISC-V)
#if defined(CONFIG_IDF_TARGET_ESP32H2)
#define FL_IS_ESP_32H2
#endif

// FL_IS_ESP_32P4 - ESP32-P4 (dual-core RISC-V)
#if defined(CONFIG_IDF_TARGET_ESP32P4)
#define FL_IS_ESP_32P4
#endif

// ============================================================================
// ESP-IDF Version Detection - Toolchain version detection
// ============================================================================
// Note: esp_version.h normalizes ESP_IDF_VERSION macros across all toolchains
// and provides ESP_IDF_VERSION_4_OR_HIGHER, ESP_IDF_VERSION_5_OR_HIGHER

// FL_IS_IDF_4_OR_HIGHER - ESP-IDF 4.x or higher
#if defined(ESP_IDF_VERSION_4_OR_HIGHER) && ESP_IDF_VERSION_4_OR_HIGHER
#define FL_IS_IDF_4_OR_HIGHER
#endif

// FL_IS_IDF_5_OR_HIGHER - ESP-IDF 5.x or higher
#if defined(ESP_IDF_VERSION_5_OR_HIGHER) && ESP_IDF_VERSION_5_OR_HIGHER
#define FL_IS_IDF_5_OR_HIGHER
#endif

// FL_IS_IDF_3_OR_LOWER - ESP-IDF 3.x or lower (for legacy code paths)
#if !defined(FL_IS_IDF_4_OR_HIGHER)
#define FL_IS_IDF_3
#endif
