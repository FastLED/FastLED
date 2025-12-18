/*
 * Stub implementation of main() for Arduino sketches (.ino files).
 *
 * This file supports the old macro-based approach with FASTLED_STUB_MAIN_INCLUDE_INO.
 * New code should use stub_main.hpp with generated wrapper files instead.
 */

// This can't be in the namespace fl. It needs to be in the global namespace.

// Only compile this file for stub platforms (not real Arduino platforms)
#if (defined(FASTLED_STUB_MAIN) || defined(FASTLED_STUB_MAIN_INCLUDE_INO)) && (!defined(ARDUINO) || defined(FASTLED_USE_STUB_ARDUINO))

#ifndef _FASTLED_STRINGIFY
#define _FASTLED_STRINGIFY_HELPER(x) #x
#define _FASTLED_STRINGIFY(x) _FASTLED_STRINGIFY_HELPER(x)
#endif

#ifdef FASTLED_STUB_MAIN_INCLUDE_INO
// Old approach: Include .ino via preprocessor macro (deprecated)
#include _FASTLED_STRINGIFY(FASTLED_STUB_MAIN_INCLUDE_INO)
#else
// No .ino file specified - provide empty setup/loop
void setup() {}
void loop() {}
#endif // FASTLED_STUB_MAIN_INCLUDE_INO

// Include the header which provides main() and other infrastructure
#include "platforms/stub_main.hpp"

#endif // (FASTLED_STUB_MAIN || FASTLED_STUB_MAIN_INCLUDE_INO) && (!ARDUINO || FASTLED_USE_STUB_ARDUINO)
