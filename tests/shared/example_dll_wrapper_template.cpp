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

// Include the .ino file (which provides setup() and loop() functions)
#include EXAMPLE_INO_PATH

// DLL mode: Use export function from example_dll_main.hpp
#ifdef EXAMPLE_DLL_MODE
#include "shared/example_dll_main.hpp"
#else
// Standalone mode: Use standard stub_main.hpp with main()
#include "platforms/stub_main.hpp"
#endif
