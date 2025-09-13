// FastLED ESP-IDF version normalization
// -------------------------------------
// Goal: Provide a single, consistent set of ESP-IDF version macros across
// Arduino-ESP32 and native ESP-IDF toolchains.
//
// This file normalizes the following symbols so all code can rely on them:
//   - ESP_IDF_VERSION_MAJOR / _MINOR / _PATCH
//   - ESP_IDF_VERSION_VAL(major, minor, patch)
//   - ESP_IDF_VERSION (packed integer using ESP_IDF_VERSION_VAL)
//   - ESP_IDF_VERSION_4_OR_HIGHER (convenience predicate)
//
// Why this matters:
// Some Arduino-ESP32 versions and older toolchains do not ship
// <esp_idf_version.h> or define all helpers. We defensively include where
// available, then define any missing pieces. This prevents incorrect
// feature detection such as compiling legacy polyfills on newer IDF.

#include "fl/has_include.h"

// Prefer official version header when available (Arduino-ESP32 / ESP-IDF 4+)
#if FL_HAS_INCLUDE(<esp_idf_version.h>)
#include "esp_idf_version.h"
#endif

// Pull in sdkconfig if present (sometimes defines version components)
#if FL_HAS_INCLUDE("sdkconfig.h")
#include "sdkconfig.h"
#endif

// Provide safe defaults for very old environments that define nothing
#ifndef ESP_IDF_VERSION_MAJOR
#define ESP_IDF_VERSION_MAJOR 3
#endif
#ifndef ESP_IDF_VERSION_MINOR
#define ESP_IDF_VERSION_MINOR 0
#endif
#ifndef ESP_IDF_VERSION_PATCH
#define ESP_IDF_VERSION_PATCH 0
#endif

// Helper to pack version components when missing
#ifndef ESP_IDF_VERSION_VAL
#define ESP_IDF_VERSION_VAL(major, minor, patch) ((major << 16) | (minor << 8) | (patch))
#endif

// Construct a comparable integer version if not provided by the toolchain
#ifndef ESP_IDF_VERSION
#define ESP_IDF_VERSION  ESP_IDF_VERSION_VAL(ESP_IDF_VERSION_MAJOR, \
                                             ESP_IDF_VERSION_MINOR, \
                                             ESP_IDF_VERSION_PATCH)
#endif

// Convenience predicate used by feature gates across the codebase
// Example: AnalogOutput analogWrite polyfill compiles only when this is false
#ifndef ESP_IDF_VERSION_4_OR_HIGHER
#define ESP_IDF_VERSION_4_OR_HIGHER (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0))
#endif

#ifndef ESP_IDF_VERSION_5_OR_HIGHER
#define ESP_IDF_VERSION_5_OR_HIGHER (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
#endif
