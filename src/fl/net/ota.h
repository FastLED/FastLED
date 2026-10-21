#pragma once

/// @file fl/net/ota.h
/// @brief Public include for fl::net::OTA. The declarations live in
/// fl/net/ota/ota.h, next to the implementation, which is compiled as its own
/// translation unit (fl/build/fl.net.ota+.cpp) so sketches that never use OTA
/// do not link the WiFi stack (FastLED #4726).

#include "fl/net/ota/ota.h"  // IWYU pragma: export
