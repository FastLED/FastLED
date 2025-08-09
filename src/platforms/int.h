#pragma once

// Platform-specific integer type definitions
// This file dispatches to the appropriate platform-specific int.h file

#if defined(ESP32)
    #include "platforms/esp/int.h"
#elif defined(__AVR__)
    #include "platforms/avr/int.h"
#elif defined(__SAM3X8E__) || defined(__MK20DX128__) || defined(__MK20DX256__) || defined(__MKL26Z64__) || defined(__IMXRT1062__) || defined(ARDUINO_ARCH_RENESAS_UNO) || defined(STM32F1) || defined(ARDUINO_GIGA) || defined(ARDUINO_GIGA_M7) || defined(NRF52_SERIES) || defined(ARDUINO_ARCH_NRF52) || defined(NRF52840_XXAA) || defined(ARDUINO_ARCH_APOLLO3) || defined(FASTLED_APOLLO3)
    // All ARM platforms (Due, Teensy, STM32, nRF52, Apollo3, etc.)
    #include "platforms/arm/int.h"
#elif defined(ARDUINO_ARCH_RP2040) || defined(TARGET_RP2040) || defined(PICO_32BIT) || defined(ARDUINO_RASPBERRY_PI_PICO)
    // Raspberry Pi RP2040 (Pico) uses ARM-like type widths where uint32_t is 'unsigned long'
    #include "platforms/arm/int.h"
#elif defined(__EMSCRIPTEN__)
    // WebAssembly / Emscripten
    #include "platforms/wasm/int.h"
#else
    // Default platform (desktop/generic)
    #include "platforms/shared/int.h"
#endif
