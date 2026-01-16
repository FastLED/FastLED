#pragma once

// ok no namespace fl

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
/// STM32 Platforms (ARM-based):
/// - FL_IS_STM32: General STM32 platform (any STM32 family)
/// - FL_IS_STM32_F1: STM32F1 family (Blue Pill, etc.)
/// - FL_IS_STM32_F2: STM32F2 family (Particle Photon)
/// - FL_IS_STM32_F4: STM32F4 family (Black Pill, etc.)
/// - FL_IS_STM32_F7: STM32F7 family
/// - FL_IS_STM32_L4: STM32L4 family (low-power)
/// - FL_IS_STM32_H7: STM32H7 family (Giga R1, high-performance)
/// - FL_IS_STM32_G4: STM32G4 family (motor control)
/// - FL_IS_STM32_U5: STM32U5 family (ultra-low-power)
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
/// SAMD Platforms (ARM-based):
/// - FL_IS_SAMD: General SAMD platform (any SAMD board)
/// - FL_IS_SAMD21: SAMD21 family (Cortex-M0+, Arduino Zero, etc.)
/// - FL_IS_SAMD51: SAMD51/SAME51 family (Cortex-M4F, Adafruit Metro M4, etc.)
///
/// SAM Platforms (ARM-based):
/// - FL_IS_SAM: General SAM platform (any SAM board)
/// - FL_IS_SAM3X8E: SAM3X8E family (Cortex-M3, 84 MHz) - Arduino Due
///
/// RP Platforms (ARM-based):
/// - FL_IS_RP: General RP platform (any RP2xxx variant)
/// - FL_IS_RP2040: Raspberry Pi RP2040 (dual Cortex-M0+, Pico, Pico W, etc.)
/// - FL_IS_RP2350: Raspberry Pi RP2350 (dual Cortex-M33 or RISC-V, Pico 2, etc.)
///
/// Silicon Labs Platforms (ARM-based):
/// - FL_IS_SILABS: General Silicon Labs EFM32/EFR32 platform
/// - FL_IS_SILABS_MGM240: MGM240 (EFR32MG24) - Arduino Nano Matter, SparkFun Thing Plus Matter
///
/// Nordic nRF52 Platforms (ARM-based):
/// - FL_IS_NRF52: General nRF52 platform (any nRF52 variant)
/// - FL_IS_NRF52832: nRF52832 (Cortex-M4F, 64 MHz, 512KB Flash)
/// - FL_IS_NRF52833: nRF52833 (Cortex-M4F, 64 MHz, 512KB Flash, BLE 5.1)
/// - FL_IS_NRF52840: nRF52840 (Cortex-M4F, 64 MHz, 1MB Flash, USB) - Adafruit Feather nRF52840, etc.
///
/// Renesas Platforms (ARM-based):
/// - FL_IS_RENESAS: General Renesas platform (any Renesas variant)
/// - FL_IS_RENESAS_RA4M1: Renesas RA4M1 (Cortex-M4, 48 MHz) - Arduino UNO R4 Minima/WiFi
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
/// Apollo3 Platforms:
/// - FL_IS_APOLLO3: Ambiq Apollo3 platform (SparkFun Artemis family)
///
/// WebAssembly Platforms:
/// - FL_IS_WASM: WebAssembly platform (Emscripten)
///
/// Stub/Testing Platforms:
/// - FL_IS_STUB: Stub platform (native builds, unit tests, host environments)
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

/// #ifdef FL_IS_STM32
///     // STM32 platform code (any family)
/// #endif
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
#include "platforms/apollo3/is_apollo3.h"
#include "platforms/arm/is_arm.h"
#include "platforms/arm/nrf52/is_nrf52.h"
#include "platforms/arm/renesas/is_renesas.h"
#include "platforms/arm/rp/is_rp.h"
#include "platforms/arm/sam/is_sam.h"
#include "platforms/arm/samd/is_samd.h"
#include "platforms/arm/silabs/is_silabs.h"
#include "platforms/arm/stm32/is_stm32.h"
#include "platforms/arm/teensy/is_teensy.h"
#include "platforms/avr/is_avr.h"
#include "platforms/esp/is_esp.h"
#include "platforms/posix/is_posix.h"
#include "platforms/stub/is_stub.h"
#include "platforms/wasm/is_wasm.h"
#include "platforms/win/is_win.h"
