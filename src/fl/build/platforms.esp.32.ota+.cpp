/// @file platforms.esp.32.ota+.cpp
/// @brief Unity build entry-point for the ESP32 OTA backend, split out of
/// `platforms+.cpp` so its WiFi/HTTP/mDNS references reach the linker only
/// when a sketch uses `fl::net::OTA` (FastLED #4726). The strong link chain is
/// sketch -> `fl.net.ota+.cpp` (`fl::net::OTA`) -> `IOTA::create()` here.

#include "platforms/new.h"

// IWYU pragma: begin_keep
#include "fl/system/arduino.h"
// IWYU pragma: end_keep

#include "platforms/esp/32/ota/_build.cpp.hpp"
