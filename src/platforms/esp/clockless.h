#pragma once

/// @file platforms/esp/clockless.h
/// @brief ESP32/ESP8266 platform-specific clockless controller dispatch
///
/// This header centralizes ESP platform-specific conditional logic for selecting
/// the appropriate clockless LED controller implementation.
///
/// ESP32: Selects ClocklessController alias (I2S, RMT, SPI, or blocking fallback)
/// ESP8266: Includes clockless driver implementation
///
/// This keeps platform-specific #ifdef logic out of the main chipsets.h file.

#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)

// Include ESP32 driver availability checks
#include "platforms/esp/32/feature_flags/enabled.h"

// Include portable blocking clockless controller for fallback
#include "platforms/shared/clockless_blocking.h"

namespace fl {

// Define platform-default ClocklessController alias for ESP32
// Multiple driver types are available (ClocklessIdf4/ClocklessIdf5, ClocklessSPI, ClocklessI2S)
// This alias selects the preferred default for backward compatibility
#ifdef FASTLED_ESP32_I2S
  // I2S driver requested explicitly
  template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 5>
  using ClocklessController = ClocklessI2S<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME>;
  #define FL_CLOCKLESS_CONTROLLER_DEFINED 1

#elif FASTLED_ESP32_HAS_RMT && !defined(FASTLED_ESP32_USE_CLOCKLESS_SPI)
  // RMT is preferred default for ESP32 (best performance, most features)
  // Use ClocklessIdf4 (ESP-IDF 4.x) or ClocklessIdf5 (ESP-IDF 5.x) based on FASTLED_RMT5
  #if FASTLED_RMT5
    template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
    using ClocklessController = ClocklessIdf5<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME>;
  #else
    template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 5>
    using ClocklessController = ClocklessIdf4<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME>;
  #endif
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
  #define FL_CLOCKLESS_CONTROLLER_DEFINED 1
#endif

}  // namespace fl

#elif defined(ESP8266) || defined(ARDUINO_ARCH_ESP8266)

#endif  // ESP32 || ARDUINO_ARCH_ESP32 || ESP8266 || ARDUINO_ARCH_ESP8266
