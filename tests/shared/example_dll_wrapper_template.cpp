// ok standalone
// Static wrapper template for Arduino example DLLs
// This file replaces dynamically generated wrappers with a compile-time approach
//
// Usage: Compile with -DEXAMPLE_INO_PATH="path/to/Example.ino"
//
// This eliminates the need for generate_wrapper.py and custom_target overhead

#ifndef EXAMPLE_INO_PATH
#error "EXAMPLE_INO_PATH must be defined (e.g., -DEXAMPLE_INO_PATH=\"examples/Blink/Blink.ino\")"
#endif

// Platform guard: If this example requires a specific platform (e.g., esp8266, teensy)
// and we're compiling for the stub/native host platform, compile a skip-stub instead
// of the actual example. This prevents mysterious compile errors when the meson build
// cache is stale and a platform-specific example ends up in the host build.
#if defined(STUB_PLATFORM) && defined(EXAMPLE_STUB_INCOMPATIBLE)

#include <stdio.h> // ok include

// Stringify helper macros
#define _EXAMPLE_STR(x) #x
#define EXAMPLE_STR(x) _EXAMPLE_STR(x)

// Provide minimal setup/loop stubs that report the incompatibility
void setup() {
#ifdef EXAMPLE_PLATFORM_FILTER
    printf("[SKIP] Example requires filter: %s (compiled on stub/native platform)\n",
           EXAMPLE_STR(EXAMPLE_PLATFORM_FILTER));
#else
    printf("[SKIP] Example is platform-specific (compiled on stub/native platform)\n");
#endif
}
void loop() {}

#else

// Include the .ino file (which provides setup() and loop() functions)
#include EXAMPLE_INO_PATH

#endif

// DLL mode: Use export function from example_dll_main.hpp
#ifdef EXAMPLE_DLL_MODE
#include "shared/example_dll_main.hpp"
#else
// Standalone mode: Use standard stub_main.hpp with main()
#include "platforms/stub_main.hpp"
#endif
