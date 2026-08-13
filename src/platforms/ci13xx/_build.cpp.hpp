#pragma once

// IWYU pragma: private

#include "platforms/ci13xx/is_ci13xx.h"

#if defined(FL_IS_CI13XX)
#include "platforms/arduino/io_arduino.hpp"
#endif
