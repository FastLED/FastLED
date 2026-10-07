/// @brief Keep SPI registry initialization dependencies local to SPI users.

#include "platforms/new.h"
// IWYU pragma: begin_keep
#include "fl/system/arduino.h"
// IWYU pragma: end_keep

#include "platforms/shared/spi_registry/_build.cpp.hpp"
