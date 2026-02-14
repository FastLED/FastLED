#pragma once

// IWYU pragma: private
#include "platforms/arm/samd/is_samd.h"

// ok no namespace fl

#ifdef FL_IS_SAMD21
#include "platforms/arduino/io_arduino.hpp"
#endif
