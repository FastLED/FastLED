/// @brief Keep optional coroutine task creation in its own compilation unit.

#include "platforms/new.h"
// IWYU pragma: begin_keep
#include "fl/system/arduino.h"
// IWYU pragma: end_keep

#include "fl/task/coroutine/_build.cpp.hpp"
