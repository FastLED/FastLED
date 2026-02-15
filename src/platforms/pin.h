#pragma once

// ok no namespace fl

/// @file platforms/pin.h
/// Trampoline dispatcher for platform-specific pin implementations
///
/// This file selects the appropriate platform-specific pin.hpp file based on
/// compile-time platform detection. It is included ONLY by fl/pin.cpp to
/// maintain the compilation boundary.
///
/// Uses FL_IS_* macros from is_platform.h for standardized platform detection.

#include "platforms/is_platform.h"

// Platform-specific pin implementations (header-only .hpp files)
// Uses standardized FL_IS_* macros for platform detection
#if defined(FL_IS_ESP32)
    #include "platforms/esp/32/pin_esp32.hpp"
#elif defined(FL_IS_ESP8266)
    #include "platforms/esp/8266/pin_esp8266.hpp"
#elif defined(FL_IS_AVR)
    #include "platforms/avr/pin_avr.hpp"
#elif defined(FL_IS_TEENSY)
    #include "platforms/arm/teensy/pin_teensy.hpp"
#elif defined(FL_IS_STM32)
    #include "platforms/arm/stm32/pin_stm32.hpp"
#elif defined(FL_IS_SAMD)
    #include "platforms/arm/samd/pin_samd.hpp"
#elif defined(FL_IS_SAM)
    #include "platforms/arm/sam/pin_sam.hpp"
#elif defined(FL_IS_RP)
    #include "platforms/arm/rp/pin_rp.hpp"
#elif defined(FL_IS_NRF52)
    #include "platforms/arm/nrf52/pin_nrf52.hpp"
#elif defined(FL_IS_RENESAS)
    #include "platforms/arm/renesas/pin_renesas.hpp"
#elif defined(FL_IS_SILABS)
    #include "platforms/arm/silabs/pin_silabs.hpp"
#elif defined(FL_IS_APOLLO3)
    #include "platforms/apollo3/pin_apollo3.hpp"
#elif defined(FL_IS_STUB)
    // Stub platform for testing (no-op pins)
    #include "platforms/shared/pin_noop.hpp"
#elif defined(FL_IS_WASM)
    // WASM platform uses no-op pin implementation
    #include "platforms/shared/pin_noop.hpp"
#elif defined(ARDUINO)
    // Arduino-compatible platform without specific pin implementation
    #error "Platform-specific pin implementation not defined for this Arduino variant. Please add support in src/platforms/pin.h or update the platform detection logic."
#else
    // Non-Arduino builds (host tests, native builds)
    #include "platforms/shared/pin_noop.hpp"
#endif
