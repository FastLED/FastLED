/// @file src.cpp
/// @brief Unity build: flat includes from src/ root directory

// Marks FastLED's own translation units. Sketch-side startup symbols
// (esp32_lazy_serial0_main.h) must not be emitted here (FastLED#5365).
#define FL_BUILDING_FASTLED_LIBRARY 1

#include "platforms/new.h"

// IWYU pragma: begin_keep
#include "fl/system/arduino.h"
// IWYU pragma: end_keep

#include "FastLED.cpp.hpp"
#include "bitswap.cpp.hpp"
#include "cled_controller.cpp.hpp"
#include "colorpalettes.cpp.hpp"
#include "crgb.cpp.hpp"
#include "fastled_delay.cpp.hpp"
#include "hsv2rgb.cpp.hpp"
#include "lib8tion.cpp.hpp"
#include "noise.cpp.hpp"
#include "platforms.cpp.hpp"
#include "power_mgt.cpp.hpp"
#include "simplex.cpp.hpp"
#include "transpose8x1_noinline.cpp.hpp"
#include "wiring.cpp.hpp"
