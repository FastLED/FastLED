#pragma once

/// @file is_platform.h
/// Unified platform detection header
///
/// This header includes all platform-specific detection headers and provides
/// a single entry point for platform detection throughout the FastLED codebase.
///
/// This header defines standardized platform detection macros:
///
/// ARM Platforms:
/// - FASTLED_ARM: ARM-based platforms (Cortex-M, RP2040, nRF52, etc.)
///
/// Teensy Platforms (ARM-based):
/// - FL_IS_TEENSY: General Teensy platform (any Teensy board)
/// - FL_IS_TEENSY_LC: Teensy LC (Cortex-M0+, MKL26Z64)
/// - FL_IS_TEENSY_3X: Teensy 3.x family (all 3.x boards)
/// - FL_IS_TEENSY_30: Teensy 3.0 (Cortex-M4, MK20DX128)
/// - FL_IS_TEENSY_31: Teensy 3.1 (Cortex-M4, MK20DX256)
/// - FL_IS_TEENSY_32: Teensy 3.2 (Cortex-M4, MK20DX256)
/// - FL_IS_TEENSY_35: Teensy 3.5 (Cortex-M4F, MK64FX512)
/// - FL_IS_TEENSY_36: Teensy 3.6 (Cortex-M4F, MK66FX1M0)
/// - FL_IS_TEENSY_4X: Teensy 4.x family (all 4.x boards)
/// - FL_IS_TEENSY_40: Teensy 4.0 (Cortex-M7, IMXRT1062)
/// - FL_IS_TEENSY_41: Teensy 4.1 (Cortex-M7, IMXRT1062)
///
/// AVR Platforms:
/// - FL_IS_AVR: General AVR platform
/// - FL_IS_AVR_ATMEGA: ATmega family (328P, 2560, 32U4, etc.)
/// - FL_IS_AVR_ATMEGA_328P: ATmega328P family (UNO/Nano)
/// - FL_IS_AVR_ATTINY: ATtiny family (85, 84, 412, etc.)
/// - FL_IS_AVR_ATTINY_NO_UART: ATtiny without UART hardware
///
/// ESP Platforms:
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
/// - FL_IS_IDF_3_OR_LOWER: ESP-IDF 3.x or lower
///
/// WebAssembly Platforms:
/// - FL_IS_WASM: WebAssembly platform (Emscripten)
///
/// POSIX Platforms:
/// - FL_IS_POSIX: General POSIX-compliant platform (Linux, macOS, BSD, etc.)
/// - FL_IS_POSIX_LINUX: Linux systems
/// - FL_IS_POSIX_MACOS: macOS/Darwin systems
/// - FL_IS_POSIX_BSD: BSD variants (FreeBSD, OpenBSD, NetBSD)
///
/// Windows Platforms:
/// - FL_IS_WIN: General Windows platform (any toolchain)
/// - FL_IS_WIN_MSVC: Microsoft Visual C++ compiler
/// - FL_IS_WIN_MINGW: MinGW/MinGW-w64 GCC toolchain (native Win32, not POSIX)
///
/// Usage:
/// @code
/// #include "platforms/is_platform.h"
///
/// #ifdef FL_IS_ESP_32S3
///     // ESP32-S3 specific code
/// #endif
///
/// #ifdef FL_IS_AVR_ATTINY
///     // ATtiny specific code
/// #endif
///
/// #ifdef FASTLED_ARM
///     // ARM platform code
/// #endif
/// @endcode

// Include all platform detection headers
#include "arm/is_arm.h"
#include "arm/teensy/is_teensy.h"
#include "avr/is_avr.h"
#include "esp/is_esp.h"
#include "posix/is_posix.h"
#include "wasm/is_wasm.h"
#include "win/is_win.h"
