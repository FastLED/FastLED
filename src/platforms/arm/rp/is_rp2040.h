#pragma once

// ok no namespace fl

/// @file is_rp2040.h
/// @brief RP2040 platform detection macros for FastLED
///
/// This header provides detection for Raspberry Pi RP2040 (dual ARM Cortex-M0+) platforms.
/// Detection uses both Arduino board-specific macros (e.g., ARDUINO_ARCH_RP2040) and 
/// CPU-specific macros (e.g., TARGET_RP2040).

// RP2040 platform detection (Raspberry Pi Pico, Pico W, etc.)
#if defined(ARDUINO_ARCH_RP2040) || defined(TARGET_RP2040) || \
    defined(PICO_32BIT) || defined(ARDUINO_RASPBERRY_PI_PICO)
#define FL_IS_RP2040
#endif
