#pragma once

// IWYU pragma: private
#include "platforms/arm/nrf52/is_nrf52.h"

// ok no namespace fl

#ifdef FL_IS_NRF52
#include "platforms/arduino/io_arduino.hpp"
#endif
