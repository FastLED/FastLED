#pragma once

// ok no namespace fl

/// @file clockless.h
/// Platform-specific clockless controller dispatch header.
///
/// This header includes the appropriate clockless controller implementation
/// based on the target platform. The includes below MUST be in this file
/// (rather than at the top of chipsets.h) because:
///
/// 1. **Template Specialization Architecture**: Base templates must be defined
///    before platform-specific specializations can reference them
/// 2. **Platform Detection**: Each platform's clockless implementation needs
///    its specific platform macros and base types to be visible
/// 3. **Conditional Compilation**: Different platforms get different controller
///    implementations without requiring user code changes
///
/// This is intentional C++ template architecture for cross-platform support.

// ============================================================================
// PLATFORM-SPECIFIC CLOCKLESS DRIVER INCLUDES (Bottom Includes)
// ============================================================================
// These includes are at the "bottom" of the dependency chain because they
// define concrete implementations that specialize the abstract base templates
// defined earlier in the include hierarchy.
// ============================================================================
#if defined(__EMSCRIPTEN__)
  #include "platforms/wasm/clockless.h"
#elif defined(FASTLED_STUB_IMPL) && !defined(__EMSCRIPTEN__)
  #include "platforms/stub/clockless_stub_generic.h"
#elif defined(ESP32) || defined(ARDUINO_ARCH_ESP32) || defined(ESP8266) || defined(ARDUINO_ARCH_ESP8266)
  #include "platforms/esp/clockless.h"
#elif defined(FASTLED_TEENSY4)
  #include "platforms/arm/teensy/teensy4_common/clockless.h"
#elif defined(__AVR__)
  #include "platforms/avr/is_avr.h"
  #ifdef FL_IS_AVR_ATTINY
    // ATtiny platforms use optimized assembly implementation from FastLED 3.10.3
    #include "platforms/avr/attiny/clockless_blocking.h"
  #else
    // Other AVR platforms (Uno, Mega, etc.) use the standard clockless controller
    #include "platforms/avr/clockless_avr.h"
  #endif
#endif

// Generic ClocklessBlocking controller is available at platforms/shared/clockless_blocking.h
// Platforms should explicitly include and use it as a fallback if they don't have
// a hardware-accelerated implementation

// Template alias to ClocklessController (platform-specific or generic blocking)
// This must come AFTER all clockless drivers are included
#include "fl/clockless_controller_impl.h"
