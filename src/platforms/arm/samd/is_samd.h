#pragma once

// ok no namespace fl

/// @file is_samd.h
/// @brief SAMD/SAME platform detection macros for FastLED
///
/// This header provides detection for Microchip SAMD (ARM Cortex-M0+/M4) platforms.
/// Detection uses both Arduino board-specific macros (e.g., ARDUINO_ARCH_SAMD) and 
/// CPU-specific macros (e.g., __SAMD21G18A__).

// SAMD21 (ARM Cortex-M0+, 48 MHz)
#if defined(ARDUINO_ARCH_SAMD) && (defined(__SAMD21__) || \
    defined(__SAMD21G18A__) || defined(__SAMD21J18A__) || \
    defined(__SAMD21E17A__) || defined(__SAMD21E18A__))
#define FL_IS_SAMD21
#endif

// SAMD51/SAME51 (ARM Cortex-M4F, 120 MHz)
#if defined(ARDUINO_ARCH_SAMD) && (defined(__SAMD51__) || \
    defined(__SAMD51G19A__) || defined(__SAMD51J19A__) || \
    defined(__SAME51J19A__) || defined(__SAMD51P19A__) || defined(__SAMD51P20A__))
#define FL_IS_SAMD51
#endif

// General SAMD platform (any SAMD board)
#if defined(FL_IS_SAMD21) || defined(FL_IS_SAMD51) || defined(ARDUINO_ARCH_SAMD)
#define FL_IS_SAMD
#endif
