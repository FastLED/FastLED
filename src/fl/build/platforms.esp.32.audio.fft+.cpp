/// @brief Isolate ESP-DSP SDK headers from portable audio code.

#include "platforms/new.h"
// IWYU pragma: begin_keep
#include "fl/system/arduino.h"
// IWYU pragma: end_keep

#include "platforms/esp/32/audio/fft/_build.cpp.hpp"
