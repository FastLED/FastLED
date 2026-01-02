#pragma once

// ok no namespace fl

/// @file platforms/pin.h
/// Trampoline dispatcher for platform-specific pin implementations
///
/// This file selects the appropriate platform-specific pin.hpp file based on
/// compile-time platform detection. It is included ONLY by fl/pin.cpp to
/// maintain the compilation boundary.
///
/// Uses compiler-defined builtin macros for platform detection to avoid
/// circular dependency issues with FL_IS_* macros from is_platform.h.

// Platform-specific pin implementations (header-only .hpp files)
// Uses compiler builtins for detection to avoid circular dependencies
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
    #include "platforms/esp/32/pin_esp32.hpp"
#elif defined(ESP8266) || defined(ARDUINO_ARCH_ESP8266)
    #include "platforms/esp/8266/pin_esp8266.hpp"
#elif defined(__AVR__)
    #include "platforms/avr/pin_avr.hpp"
#elif defined(TEENSYDUINO) || defined(__MKL26Z64__) || defined(__MK20DX128__) || defined(__MK20DX256__) || defined(__MK64FX512__) || defined(__MK66FX1M0__) || defined(__IMXRT1062__)
    #include "platforms/arm/teensy/pin_teensy.hpp"
#elif defined(STM32F10X_MD) || defined(__STM32F1__) || defined(STM32F1) || defined(STM32F1xx) || \
      defined(STM32F2XX) || defined(STM32F2xx) || \
      defined(STM32F4) || defined(STM32F4xx) || \
      defined(STM32F7) || defined(STM32F7xx) || \
      defined(STM32L4) || defined(STM32L4xx) || \
      defined(STM32H7) || defined(STM32H7xx) || \
      defined(STM32G4) || defined(STM32G4xx) || \
      defined(STM32U5) || defined(STM32U5xx)
    #include "platforms/arm/stm32/pin_stm32.hpp"
#elif defined(ARDUINO_ARCH_SAMD) || defined(__SAMD21__) || defined(__SAMD51__)
    #include "platforms/arm/samd/pin_samd.hpp"
#elif defined(__SAM3X8E__)
    #include "platforms/arm/sam/pin_sam.hpp"
#elif defined(ARDUINO_ARCH_RP2040) || defined(PICO_BOARD)
    #include "platforms/arm/rp/pin_rp.hpp"
#elif defined(NRF52_SERIES) || defined(ARDUINO_ARCH_NRF52) || defined(NRF52832_XXAA) || defined(NRF52833_XXAA) || defined(NRF52840_XXAA)
    #include "platforms/arm/nrf52/pin_nrf52.hpp"
#elif defined(ARDUINO_ARCH_RENESAS) || defined(ARDUINO_UNOWIFIR4) || defined(ARDUINO_MINIMA)
    #include "platforms/arm/renesas/pin_renesas.hpp"
#elif defined(ARDUINO_NANO_MATTER) || defined(ARDUINO_SPARKFUN_THINGPLUS_MATTER)
    #include "platforms/arm/silabs/pin_silabs.hpp"
#elif defined(ARDUINO_ARCH_APOLLO3)
    #include "platforms/apollo3/pin_apollo3.hpp"
#elif defined(FASTLED_STUB_IMPL) && !defined(__EMSCRIPTEN__)
    // Stub platform for testing (no-op pins)
    #include "platforms/shared/pin_noop.hpp"
#elif defined(ARDUINO)
    // Arduino-compatible platform without specific pin implementation
    #error "Platform-specific pin implementation not defined for this Arduino variant. Please add support in src/platforms/pin.h or update the platform detection logic."
#else
    // Non-Arduino builds (host tests, native builds)
    #include "platforms/shared/pin_noop.hpp"
#endif
