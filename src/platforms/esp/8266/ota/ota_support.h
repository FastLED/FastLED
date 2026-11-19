#pragma once

// IWYU pragma: private

// ok no namespace fl

/// @file platforms/esp/8266/ota/ota_support.h
/// @brief Single support gate for the ESP8266 OTA backend.
///
/// Both `platforms/ota.cpp.hpp` (which drops its null `IOTA::create()` when a
/// real backend exists) and `platforms/esp/8266/ota/ota_impl.cpp.hpp` (which
/// then must define it) read this one macro. When the two kept separate copies
/// of the check they drifted: the backend also required `<WiFi.h>`, which the
/// ESP8266 Arduino core does not ship, so neither side defined
/// `IOTA::create()` and the OTA example failed to link.
///
/// The ESP8266 core ships `ArduinoOTA` in `libraries/ArduinoOTA` and its WiFi
/// API as `<ESP8266WiFi.h>` (`libraries/ESP8266WiFi`); `ArduinoOTA.h` itself
/// includes `<ESP8266WiFi.h>`.

#include "fl/stl/has_include.h"
#include "platforms/is_platform.h"

#if defined(FL_IS_ESP8266) && FL_HAS_INCLUDE(<ArduinoOTA.h>) && \
    FL_HAS_INCLUDE(<ESP8266WiFi.h>)
#define FL_ESP8266_OTA_SUPPORTED 1
#else
#define FL_ESP8266_OTA_SUPPORTED 0
#endif
