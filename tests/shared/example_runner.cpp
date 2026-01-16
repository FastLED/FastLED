// Generic example runner that loads and executes example shared libraries
// Usage: example_runner.exe <example_dll_path> [args...]
// Or:    <example_name>.exe (auto-loads <example_name>.dll from same directory)
//
// This file simply includes the platform dispatch header which provides
// the main() function with platform-specific shared library loading.

#include "platforms/run_example.h"
