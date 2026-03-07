#pragma once

// IWYU pragma: private
#include "platforms/arm/sam/is_sam.h"

// ok no namespace fl

#ifdef FL_IS_SAM
#include "platforms/arduino/io_arduino.hpp"
#endif
