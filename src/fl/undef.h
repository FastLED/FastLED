#pragma once

// Undefine common macros that conflict with FastLED function/method names
// Arduino and many platform headers define min/max/abs/round as function-like macros,
// which prevents us from declaring functions or member functions with these names.
//
// Include this header immediately before any code that declares functions named
// min, max, abs, round, etc. to prevent macro expansion issues.
//
// Usage:
//   #include "fl/undef.h"  // Before declaring min/max/abs functions
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
