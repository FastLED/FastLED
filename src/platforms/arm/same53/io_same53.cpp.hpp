#pragma once

// IWYU pragma: private

// ok no namespace fl

#include "platforms/is_platform.h"

#ifdef FL_IS_SAME53
#include "platforms/arduino/io_arduino.hpp"
#endif
