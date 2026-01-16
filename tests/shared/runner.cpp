// Generic test runner that loads and executes test shared libraries
// Usage: runner.exe <test_dll_path> [doctest args...]
// Or:    <test_name>.exe (auto-loads <test_name>.dll from same directory)
//
// This file simply includes the platform dispatch header which provides
// the main() function with platform-specific shared library loading.

#include "platforms/run_unit_test.h"
