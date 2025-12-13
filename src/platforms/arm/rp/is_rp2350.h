#pragma once

// ok no namespace fl

/// @file is_rp2350.h
/// @brief RP2350 platform detection macros for FastLED
///
/// This header provides detection for Raspberry Pi RP2350 (dual ARM Cortex-M33 or RISC-V) platforms.
/// Detection uses both Arduino board-specific macros (e.g., ARDUINO_ARCH_RP2350) and
/// CPU-specific macros (e.g., PICO_RP2350).

// RP2350 platform detection (Raspberry Pi Pico 2, etc.)
#if defined(ARDUINO_ARCH_RP2350) || defined(PICO_RP2350) || \
    defined(ARDUINO_RASPBERRY_PI_PICO_2)
#define FL_IS_RP2350
#endif
