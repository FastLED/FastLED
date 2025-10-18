#pragma once

// Platform-specific integer type definitions
// This file dispatches to the appropriate platform-specific int.h file

// ARM platform detection (inlined from is_arm.h to avoid extra header inclusion overhead)
#ifndef FASTLED_ARM
#if \
    /* ARM Cortex-M0/M0+ (SAM) */ \
    defined(__SAM3X8E__) || \
    /* ARM Cortex-M (STM32F/H7) */ \
    defined(STM32F10X_MD) || defined(__STM32F1__) || defined(STM32F1) || \
    defined(STM32F2XX) || defined(STM32F4) || \
    /* NXP Kinetis (MK20, MK26, IMXRT) */ \
    defined(__MK20DX128__) || defined(__MK20DX256__) || \
    defined(__MKL26Z64__) || defined(__IMXRT1062__) || \
    /* Arduino Renesas UNO R4 */ \
    defined(ARDUINO_ARCH_RENESAS_UNO) || \
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
#endif
#endif

#if defined(ESP8266)
    #include "platforms/esp/int_8266.h"
#elif defined(ESP32)
    #include "platforms/esp/int.h"
#elif defined(__AVR__)
    #include "platforms/avr/int.h"
#elif defined(__IMXRT1062__)
    // Teensy 4.0/4.1 (IMXRT1062 Cortex-M7) - needs special handling for system headers
    #include "platforms/arm/teensy/teensy4_common/int.h"
#elif defined(__MK20DX128__) || defined(__MK20DX256__) || defined(__MKL26Z64__)
    // Teensy 3.x family (MK20DX/MKL26Z Cortex-M4/M0+) - needs special handling for system headers
    // - Teensy 3.0 (MK20DX128), Teensy 3.1/3.2 (MK20DX256), Teensy LC (MKL26Z64)
    #include "platforms/arm/teensy/teensy3_common/int.h"
#elif defined(FASTLED_ARM)
    // All other ARM platforms (Due, STM32, nRF52, Apollo3, etc.)
    #include "platforms/arm/int.h"
#elif defined(__EMSCRIPTEN__)
    // WebAssembly / Emscripten
    #include "platforms/wasm/int.h"
#else
    // Default platform (desktop/generic)
    #include "platforms/shared/int.h"
#endif
