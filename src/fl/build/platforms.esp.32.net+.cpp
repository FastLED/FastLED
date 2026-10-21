/// @file platforms.esp.32.net+.cpp
/// @brief Unity build entry-point for the ESP32 fl::net::wifi backend, split
/// out of `platforms+.cpp` so its strong `esp_wifi_init` /
/// `esp_netif_create_default_wifi_*` references reach the linker only when a
/// sketch calls `fl::net::wifi::*`. In `platforms+.cpp` they extracted the
/// WiFi archive members into every ESP32 sketch (FastLED #4726).

#include "platforms/new.h"

// IWYU pragma: begin_keep
#include "fl/system/arduino.h"
// IWYU pragma: end_keep

#include "platforms/esp/32/net/_build.cpp.hpp"
