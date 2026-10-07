/// @brief Isolate ESP32 condition variables so unused std::unique_lock
/// operations cannot extract system-error and locale code into LED sketches.

#include "platforms/new.h"
// IWYU pragma: begin_keep
#include "fl/system/arduino.h"
// IWYU pragma: end_keep

#include "platforms/esp/32/condition_variable/_build.cpp.hpp"
