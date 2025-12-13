#pragma once

// ok no namespace fl

/// @file is_nrf52.h
/// @brief Nordic nRF52 platform detection macros for FastLED
///
/// This header provides detection for Nordic nRF52 (ARM Cortex-M4F) platforms.
/// Detection uses both Arduino board-specific macros (e.g., ARDUINO_ARCH_NRF52) and
/// CPU-specific macros (e.g., NRF52832_XXAA, NRF52840_XXAA).

// nRF52832 (ARM Cortex-M4F with FPU, 64 MHz, 512KB Flash)
#if defined(NRF52832_XXAA) || \
    defined(NRF52_SERIES) && !defined(NRF52840_XXAA) && !defined(NRF52833_XXAA) || \
    defined(ARDUINO_NRF52_ADAFRUIT) && !defined(NRF52840_XXAA)
#define FL_IS_NRF52832
#endif

// nRF52833 (ARM Cortex-M4F with FPU, 64 MHz, 512KB Flash, BLE 5.1)
#if defined(NRF52833_XXAA)
#define FL_IS_NRF52833
#endif

// nRF52840 (ARM Cortex-M4F with FPU, 64 MHz, 1MB Flash, USB)
#if defined(NRF52840_XXAA) || \
    defined(ARDUINO_NRF52840_FEATHER) || \
    defined(ARDUINO_NRF52840_FEATHER_SENSE) || \
    defined(ARDUINO_NRF52840_ITSYBITSY) || \
    defined(ARDUINO_NRF52840_CLUE)
#define FL_IS_NRF52840
#endif

// General nRF52 platform (any nRF52 variant)
#if defined(FL_IS_NRF52832) || defined(FL_IS_NRF52833) || defined(FL_IS_NRF52840) || \
    defined(NRF52_SERIES) || defined(ARDUINO_ARCH_NRF52) || \
    defined(ARDUINO_NRF52_ADAFRUIT)
#define FL_IS_NRF52
#endif
