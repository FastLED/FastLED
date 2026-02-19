// Note: intentionally no #pragma once — this file is designed to be re-includable.
// It is a macro-reset header, not a definitions header. Including it multiple times
// is idempotent and correct: it cleans up any macros active at the point of inclusion.
// Re-inclusion is needed because platform headers may re-define min/max/abs/etc. after
// an earlier cleanup (e.g. the fl/arduino.h cleanup that runs before platform headers,
// and the fl/stl/math.h cleanup that runs after them).

#include "fl/has_include.h"  // IWYU pragma: keep

#include "fl/compiler_control.h"

#if FL_HAS_INCLUDE(<stdlib.h>)
// IWYU pragma: begin_keep
#include <stdlib.h>
// IWYU pragma: end_keep  // This will sometimes define macros like abs, min, max, etc.
#endif

// Undefine common macros that conflict with FastLED function/method names
// Arduino and many platform headers define min/max/abs/round as function-like macros,
// which prevents us from declaring functions or member functions with these names.
//
// Include this header immediately before any code that declares functions named
// min, max, abs, round, etc. to prevent macro expansion issues.
//
// Usage:
//   #include "fl/stl/undef.h"  // Before declaring min/max/abs functions
//   static T min() { ... }
//   static T max() { ... }

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#ifdef abs
#undef abs
#endif

#ifdef round
#undef round
#endif

#ifdef radians
#undef radians
#endif

#ifdef degrees
#undef degrees
#endif

#ifdef map
#undef map
#endif
