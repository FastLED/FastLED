#pragma once

// IWYU pragma: private
// ok no namespace fl

#include "platforms/arm/teensy/is_teensy.h"

#ifdef FL_IS_TEENSY

// Teensy platforms use Arduino Serial interface
// Just include Arduino's implementation directly
#include "platforms/arduino/io_arduino.hpp"

#endif // FL_IS_TEENSY
