// Minimal translation unit for clang-query analysis of FastLED-owned fl/ code.
// Uses the stub platform so host-visible implementations parse without board
// framework headers.
#include "platforms/new.h"
#include "fl/system/arduino.h"
#include "fl/_build.cpp.hpp"
