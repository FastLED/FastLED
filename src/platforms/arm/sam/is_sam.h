#pragma once

// ok no namespace fl

/// @file is_sam.h
/// @brief SAM platform detection macros for FastLED
///
/// This header provides detection for Atmel/Microchip SAM (ARM Cortex-M3) platforms.
/// Detection uses both Arduino board-specific macros (e.g., ARDUINO_SAM_DUE) and
/// CPU-specific macros (e.g., __SAM3X8E__).

// SAM3X8E (ARM Cortex-M3, 84 MHz) - Arduino Due
#if defined(__SAM3X8E__) || defined(ARDUINO_SAM_DUE)
#define FL_IS_SAM3X8E
#endif

// General SAM platform (any SAM board)
#if defined(FL_IS_SAM3X8E)
#define FL_IS_SAM
#endif
