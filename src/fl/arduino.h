#pragma once

#include "fl/has_include.h"

#if FL_HAS_INCLUDE(<Arduino.h>)
// IWYU pragma: begin_keep
#include <Arduino.h>
// IWYU pragma: end_keep
#endif

// Clean up Arduino macros (abs, min, max, round, etc.)
// This MUST be immediately after Arduino.h so macros never leak.
#include "fl/stl/undef.h"
