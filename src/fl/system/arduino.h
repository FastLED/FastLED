#pragma once

#include "fl/stl/has_include.h"
#include "platforms/is_platform.h"

#if !defined(FL_IS_ESP32) && FL_HAS_INCLUDE(<Arduino.h>)
#ifdef FL_NO_ARDUINO
#error "Arduino.h is forbidden by FL_NO_ARDUINO"
#endif
// IWYU pragma: begin_keep
#include <Arduino.h>
// IWYU pragma: end_keep
#endif

// Clean up Arduino macros (abs, min, max, round, etc.)
// This MUST be immediately after Arduino.h so macros never leak.
