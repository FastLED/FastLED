#pragma once

// IWYU pragma: private, include "fl/system/pin.h"

/// @file fl/system/analog/analog_read.h
/// @brief `fl::analogRead()` is declared in `fl/system/pin.h`; its definition
/// lives in `analog_read.cpp.hpp`, a separate unity object
/// (`fl/build/fl.system.analog+.cpp`) so the platform ADC driver links only
/// when a sketch reads an analog pin (FastLED #4796).

#include "fl/system/pin.h"  // IWYU pragma: export
