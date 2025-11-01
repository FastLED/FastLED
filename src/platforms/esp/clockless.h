#pragma once

/// @file platforms/esp/clockless.h
/// @brief ESP32/ESP8266 platform-specific clockless controller dispatch
///
/// This header centralizes ESP platform-specific conditional logic for selecting
/// the appropriate clockless LED controller implementation. It handles:
/// - ESP32 ClocklessController alias selection (I2S, RMT, SPI, or blocking fallback)
/// - ESP32 FASTLED_CLOCKLESS_USES_NANOSECONDS configuration
/// - ESP8266 FASTLED_CLOCKLESS_USES_NANOSECONDS configuration
///
/// This keeps platform-specific #ifdef logic out of the main chipsets.h file.

#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)

// Include ESP32 driver availability checks
#include "third_party/espressif/led_strip/src/enabled.h"

namespace fl {

// Define platform-default ClocklessController alias for ESP32
// Multiple driver types are available (ClocklessRMT, ClocklessSPI, ClocklessI2S)
// This alias selects the preferred default for backward compatibility
#ifdef FASTLED_ESP32_I2S
  // I2S driver requested explicitly
  template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 5>
  using ClocklessController = ClocklessI2S<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME>;
  #define FL_CLOCKLESS_CONTROLLER_DEFINED 1

#elif FASTLED_ESP32_HAS_RMT && !defined(FASTLED_ESP32_USE_CLOCKLESS_SPI)
  // RMT is preferred default for ESP32 (best performance, most features)
  #pragma message "ESP32: Using ClocklessRMT as default ClocklessController"
  template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
  using ClocklessController = ClocklessRMT<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME>;
  #define FL_CLOCKLESS_CONTROLLER_DEFINED 1

#elif FASTLED_ESP32_HAS_CLOCKLESS_SPI
  // SPI fallback for platforms without RMT (e.g., ESP32-S2)
  template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 5>
  using ClocklessController = ClocklessSPI<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME>;
  #define FL_CLOCKLESS_CONTROLLER_DEFINED 1

#else
  // No hardware driver available - use generic blocking implementation
  template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
  using ClocklessController = ClocklessBlocking<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME>;
#endif

}  // namespace fl

// Define whether ESP32 clockless drivers use nanosecond-based timing
// All ESP32 drivers now support nanosecond-based timing:
// - RMT5: Native nanosecond support
// - RMT4: Converts nanoseconds → CPU cycles → RMT ticks
// - I2S: Uses nanosecond values directly
// - SPI: External driver handles timing
// - LCD (I80/RGB): Uses ClocklessTiming module with nanoseconds
// - ParLIO: Uses ClocklessTiming module with nanoseconds
// - Blocking: Converts nanoseconds → CPU cycles
#ifndef FASTLED_CLOCKLESS_USES_NANOSECONDS
  #define FASTLED_CLOCKLESS_USES_NANOSECONDS 1
#endif  // FASTLED_CLOCKLESS_USES_NANOSECONDS

#elif defined(ESP8266) || defined(ARDUINO_ARCH_ESP8266)

// ESP8266 now uses nanosecond-based timing
// The clockless driver (platforms/esp/8266/clockless_esp8266.h) converts
// nanoseconds to CPU cycles at compile-time using the NS_TO_CYCLES formula.
#ifndef FASTLED_CLOCKLESS_USES_NANOSECONDS
  #define FASTLED_CLOCKLESS_USES_NANOSECONDS 1
#endif

#endif  // ESP32 || ARDUINO_ARCH_ESP32 || ESP8266 || ARDUINO_ARCH_ESP8266
