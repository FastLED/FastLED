#pragma once

// ok no namespace fl

/// @file platforms/pin.h
/// Trampoline dispatcher for platform-specific pin implementations
///
/// This file selects the appropriate platform-specific pin.hpp file based on
/// compile-time platform detection. It is included ONLY by fl/pin.cpp to
/// maintain the compilation boundary.

// Platform-specific pin implementations (header-only .hpp files)
#if defined(ESP32)
    #include "platforms/esp/32/pin_esp32.hpp"
#elif defined(ESP8266)
    #include "platforms/esp/8266/pin_esp8266.hpp"
#elif defined(ARDUINO_ARCH_AVR) || defined(__AVR__)
    #include "platforms/avr/pin_avr.hpp"
#elif defined(ARDUINO_ARCH_TEENSY) || defined(__MK20DX256__) || defined(__MK66FX1M0__) || defined(__IMXRT1062__)
    #include "platforms/arm/teensy/pin_teensy.hpp"
#elif defined(ARDUINO_ARCH_STM32) || defined(STM32)
    #include "platforms/arm/stm32/pin_stm32.hpp"
#elif defined(ARDUINO_ARCH_SAMD) || defined(__SAMD21__) || defined(__SAMD51__)
    #include "platforms/arm/samd/pin_samd.hpp"
#elif defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_RASPBERRY_PI_PICO)
    #include "platforms/arm/rp/pin_rp.hpp"
#elif defined(ARDUINO_ARCH_APOLLO3) || defined(ARDUINO_APOLLO3)
    #include "platforms/apollo3/pin_apollo3.hpp"
#elif defined(ARDUINO)
    // Generic Arduino fallback for any Arduino-compatible platform
    #include "platforms/shared/pin_arduino.hpp"
#else
    // Non-Arduino builds (host tests, native builds)
    #include "platforms/shared/pin_noop.hpp"
#endif
