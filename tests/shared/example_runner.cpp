// Generic example runner that loads and executes example DLLs
// Usage: example_runner.exe <example_dll_path> [args...]
// Or:    <example_name>.exe (auto-loads <example_name>.dll from same directory)

// Generic example runner that loads and executes example DLLs
// Usage: example_runner.exe <example_dll_path> [args...]
// Or:    <example_name>.exe (auto-loads <example_name>.dll from same directory)

#include <string>    // ok include
#include <iostream>  // ok include
#include <vector>    // ok include
#include <stddef.h>  // ok include

// Crash handler setup (defined in crash_handler_main.cpp)
extern "C" void runner_setup_crash_handler();

typedef int (*RunExampleFunc)(int argc, const char** argv);

#ifdef _WIN32
#include "example_runner_win.hpp"
#else // _WIN32
#incldue "example_runner_posix.hpp"
#endif // _WIN32
