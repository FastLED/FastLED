/// @file platforms/fastpin.h
/// Central dispatcher for platform-specific fastpin implementations
///
/// This file detects the target platform and includes the appropriate
/// platform-specific fastpin implementations. It serves as the single source of
/// platform detection for all pin access implementations across FastLED targets.
///
/// Dispatches to:
/// - Platform-specific Pin class (runtime pin access)
/// - Platform-specific FastPin<> specializations (compile-time pin access)

// ok no namespace fl
#pragma once

// Platform-specific implementations
// Each platform path provides FastPin<> specializations.
// Pin class implementation is platform-dependent (see below).

#if defined(FASTLED_STUB_IMPL) || defined(__EMSCRIPTEN__)
    // Stub implementation for testing and WebAssembly
    // Provides no-op Pin and FastPin<> implementations
    // For stub platforms, all pins are valid (no hardware constraints)
    // Note: FASTLED_ALL_PINS_VALID is automatically defined in fastled_config.h
    #include "platforms/stub/fastpin_stub.h"

#elif defined(ESP32)
    // ESP32 family (ESP32, S2, S3, C3, C6, P4)
    // Include generic PINMAP-based Pin, then ESP-specific FastPin<>
    #include "platforms/generic_pin.h"
    #include "platforms/esp/fastpin_esp.h"

#elif defined(ESP8266)
    // ESP8266 microcontroller
    #include "platforms/generic_pin.h"
    #include "platforms/esp/8266/fastpin_esp8266.h"

#elif defined(__AVR__)
    // AVR 8-bit microcontrollers (Arduino UNO, Nano, etc.)
    #include "platforms/generic_pin.h"
    #include "platforms/avr/fastpin_avr.h"

#elif defined(FASTLED_ARM)
    // ARM microcontroller family (Teensy, Arduino GIGA, Pico, etc.)
    #include "platforms/generic_pin.h"
    #include "platforms/arm/fastpin_arm.h"

#elif defined(APOLLO3)
    // Ambiq Apollo3 (SparkFun Artemis)
    #include "platforms/generic_pin.h"
    #include "platforms/apollo3/fastpin_apollo3.h"

#else
    // Fallback: no specific platform detected
    // Use generic PINMAP-based Pin class and default FastPin<> template
    #include "platforms/generic_pin.h"

#endif
