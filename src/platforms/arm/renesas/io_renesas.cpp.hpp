#pragma once

// IWYU pragma: private
#include "platforms/arm/renesas/is_renesas.h"

// ok no namespace fl

// Renesas RA boards (Arduino UNO R4 Minima / WiFi) are Arduino-framework
// targets; the standard `Serial.print*` API is available. Pulling in
// platforms/arduino/io_arduino.hpp provides the `fl::platforms::print*`
// implementations the rest of FastLED links against. Without this file
// every `uno_r4_wifi` example fails at link with:
//   undefined reference to `fl::platforms::println(char const*)'
#ifdef FL_IS_RENESAS
#include "platforms/arduino/io_arduino.hpp"
#endif
