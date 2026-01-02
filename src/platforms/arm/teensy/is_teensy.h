// ok no namespace fl

/// @file is_teensy.h
/// @brief Teensy platform detection macros for FastLED
///
/// This header provides fine-grained detection of Teensy boards across all
/// generations (LC, 3.x, 4.x). Detection uses both Arduino board-specific
/// macros (e.g., ARDUINO_TEENSY41) and CPU-specific macros (e.g., __IMXRT1062__).

#pragma once

// Teensy LC (ARM Cortex-M0+, 48 MHz, MKL26Z64)
#if defined(ARDUINO_TEENSYLC) || defined(__MKL26Z64__)
    #define FL_IS_TEENSY_LC
#endif

// Teensy 3.0 (ARM Cortex-M4, 48 MHz, MK20DX128)
#if defined(ARDUINO_TEENSY30) || defined(__MK20DX128__)
    #define FL_IS_TEENSY_30
#endif

// Teensy 3.1 (ARM Cortex-M4, 72 MHz, MK20DX256)
#if defined(ARDUINO_TEENSY31)
    #define FL_IS_TEENSY_31
#endif

// Teensy 3.2 (ARM Cortex-M4, 72 MHz, MK20DX256)
#if defined(ARDUINO_TEENSY32)
    #define FL_IS_TEENSY_32
#endif

// Fallback for 3.1/3.2 when Arduino macro is not available
// Both use MK20DX256, but we can't distinguish them without board macro
#if defined(__MK20DX256__) && !defined(FL_IS_TEENSY_31) && !defined(FL_IS_TEENSY_32)
    // Default to 3.2 as it's the more common/recent board
    #define FL_IS_TEENSY_32
#endif

// Teensy 3.5 (ARM Cortex-M4F, 120 MHz, MK64FX512)
#if defined(ARDUINO_TEENSY35) || defined(__MK64FX512__)
    #define FL_IS_TEENSY_35
#endif

// Teensy 3.6 (ARM Cortex-M4F, 180 MHz, MK66FX1M0)
#if defined(ARDUINO_TEENSY36) || defined(__MK66FX1M0__)
    #define FL_IS_TEENSY_36
#endif

// Teensy 4.0 (ARM Cortex-M7, 600 MHz, IMXRT1062)
#if defined(ARDUINO_TEENSY40)
    #define FL_IS_TEENSY_40
#endif

// Teensy 4.1 (ARM Cortex-M7, 600 MHz, IMXRT1062)
#if defined(ARDUINO_TEENSY41)
    #define FL_IS_TEENSY_41
#endif

// Fallback for 4.0/4.1 when Arduino macro is not available
// Both use IMXRT1062, but we can't distinguish them without board macro
#if defined(__IMXRT1062__) && !defined(FL_IS_TEENSY_40) && !defined(FL_IS_TEENSY_41)
    // Default to 4.1 as it's the more common/feature-complete board
    #define FL_IS_TEENSY_41
#endif

// Teensy 3.x family umbrella (all 3.x boards)
#if defined(FL_IS_TEENSY_30) || defined(FL_IS_TEENSY_31) || defined(FL_IS_TEENSY_32) || \
    defined(FL_IS_TEENSY_35) || defined(FL_IS_TEENSY_36)
    #define FL_IS_TEENSY_3X
#endif

// Teensy 4.x family umbrella (all 4.x boards)
#if defined(FL_IS_TEENSY_40) || defined(FL_IS_TEENSY_41)
    #define FL_IS_TEENSY_4X
#endif

// General Teensy platform (any Teensy board)
#if defined(FL_IS_TEENSY_LC) || defined(FL_IS_TEENSY_3X) || defined(FL_IS_TEENSY_4X)
    #define FL_IS_TEENSY
#endif
