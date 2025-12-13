#pragma once

// ok no namespace fl

/// @file is_renesas.h
/// @brief Renesas platform detection macros for FastLED
///
/// This header provides detection for Renesas (ARM Cortex-M4) platforms,
/// including the Arduino UNO R4 Minima and WiFi boards.
/// Detection uses Arduino board-specific macros (ARDUINO_ARCH_RENESAS).

// Arduino UNO R4 Minima (Renesas RA4M1, ARM Cortex-M4, 48 MHz)
#if defined(ARDUINO_MINIMA)
#define FL_IS_RENESAS_RA4M1
#endif

// Arduino UNO R4 WiFi (Renesas RA4M1, ARM Cortex-M4, 48 MHz, ESP32-S3 co-processor)
#if defined(ARDUINO_UNOWIFIR4)
#define FL_IS_RENESAS_RA4M1
#endif

// General Renesas platform (any Renesas variant)
#if defined(FL_IS_RENESAS_RA4M1) || \
    defined(ARDUINO_ARCH_RENESAS) || \
    defined(ARDUINO_ARCH_RENESAS_UNO) || \
    defined(ARDUINO_ARCH_RENESAS_PORTENTA)
#define FL_IS_RENESAS
#endif
