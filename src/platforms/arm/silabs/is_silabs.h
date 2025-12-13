#pragma once

// ok no namespace fl

/// @file is_silabs.h
/// @brief Silicon Labs (SiLabs) platform detection macros for FastLED
///
/// This header provides detection for Silicon Labs EFM32/EFR32 platforms.
/// Detection uses Arduino board-specific macros (e.g., ARDUINO_ARCH_SILABS).
///
/// Platforms:
/// - MGM240 (EFR32MG24) - Arduino Nano Matter, SparkFun Thing Plus Matter
/// - Other Silicon Labs EFM32/EFR32 boards with Arduino support

// Silicon Labs EFM32/EFR32 platform detection
#if defined(ARDUINO_ARCH_SILABS)
#define FL_IS_SILABS
#endif

// MGM240 (EFR32MG24) specific detection
#if defined(ARDUINO_ARCH_SILABS) && \
    (defined(EFR32MG24) || defined(MGM240) || \
     defined(ARDUINO_SPARKFUN_THING_PLUS_MATTER) || \
     defined(ARDUINO_NANO_MATTER))
#define FL_IS_SILABS_MGM240
#endif
