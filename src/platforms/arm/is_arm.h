// ok no namespace fl
#pragma once

/// @file is_arm.h
/// ARM platform detection header
///
/// This header detects ARM-based platforms by checking compiler-defined macros
/// and defines FASTLED_ARM when an ARM platform is detected.
///
/// Used by platforms/int.h for platform dispatching and by ARM platform headers
/// for validation that ARM detection has occurred.

// Include platform-specific detection headers
#include "nrf52/is_nrf52.h"
#include "renesas/is_renesas.h"
#include "rp/is_rp2040.h"
#include "sam/is_sam.h"
#include "samd/is_samd.h"
#include "silabs/is_silabs.h"
#include "stm32/is_stm32.h"
#include "teensy/is_teensy.h"

/// ARM platform detection with optimized macro grouping
/// This checks for various ARM-based microcontroller families
#if \
    /* Atmel SAM (Arduino Due, SAM3X8E Cortex-M3) - defined by is_sam.h */ \
    defined(FL_IS_SAM) || defined(__SAM3X8E__) || \
    /* STM32 Family (all variants) - defined by is_stm32.h */ \
    defined(FL_IS_STM32) || \
    /* NXP Kinetis (MK20, MK26, MK64, MK66, IMXRT) */ \
    defined(__MK20DX128__) || defined(__MK20DX256__) || \
    defined(__MKL26Z64__) || defined(__MK64FX512__) || defined(__MK66FX1M0__) || \
    defined(__IMXRT1062__) || \
    /* Renesas (Arduino UNO R4, RA4M1) - defined by is_renesas.h */ \
    defined(FL_IS_RENESAS) || \
    /* Arduino STM32H747 (GIGA) */ \
    defined(ARDUINO_GIGA) || defined(ARDUINO_GIGA_M7) || \
    /* Nordic nRF52 */ \
    defined(NRF52_SERIES) || defined(ARDUINO_ARCH_NRF52) || \
    defined(NRF52840_XXAA) || defined(ARDUINO_NRF52840_FEATHER_SENSE) || \
    /* Ambiq Apollo3 */ \
    defined(ARDUINO_ARCH_APOLLO3) || defined(FASTLED_APOLLO3) || \
    /* Raspberry Pi RP2040 */ \
    defined(ARDUINO_ARCH_RP2040) || defined(TARGET_RP2040) || \
    defined(PICO_32BIT) || defined(ARDUINO_RASPBERRY_PI_PICO) || \
    /* Silicon Labs EFM32 */ \
    defined(ARDUINO_ARCH_SILABS) || \
    /* Microchip SAMD21 */ \
    defined(__SAMD21G18A__) || defined(__SAMD21J18A__) || \
    defined(__SAMD21E17A__) || defined(__SAMD21E18A__) || \
    /* Microchip SAMD51/SAME51 */ \
    defined(__SAMD51G19A__) || defined(__SAMD51J19A__) || \
    defined(__SAME51J19A__) || defined(__SAMD51P19A__) || defined(__SAMD51P20A__)
#define FASTLED_ARM
#endif  // ARM platform detection
