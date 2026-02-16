// ok no namespace fl
#pragma once

/// @file run_example.h
/// Platform dispatch header for example runner
///
/// This header provides platform-specific shared library loading and execution
/// for running FastLED examples. It delegates to the appropriate platform
/// implementation based on compile-time detection.
///
/// Usage:
///   Include this header in your example runner main.cpp file.
///   It provides the main() function that loads and runs example shared libraries.
///
/// Platform Selection Logic:
/// - Windows (_WIN32) -> win/run_example.hpp (uses LoadLibraryA, GetProcAddress)
/// - Apple (__APPLE__) -> apple/run_example.hpp (uses dlopen, _NSGetExecutablePath)
/// - Linux/POSIX -> posix/run_example.hpp (uses dlopen, /proc/self/exe)
///
/// NOTE: This file intentionally uses std:: types instead of fl:: types to avoid
/// linking the runner against libfastled.a. The runner should be a lightweight
/// DLL loader with no FastLED dependencies - only the example DLLs link FastLED.

// IWYU pragma: begin_keep
#include <string>
#include <vector>
#include <iostream>
#include <cstddef>
// IWYU pragma: end_keep

// Crash handler setup (defined in crash_handler_main.cpp)
extern "C" void runner_setup_crash_handler();

// Function signature for the example entry point exported by example DLLs/SOs
typedef int (*RunExampleFunc)(int argc, const char** argv);

// Platform-specific implementations
#if defined(_WIN32)
#include "platforms/win/run_example.hpp"  // IWYU pragma: keep
#elif defined(__APPLE__)
#include "platforms/apple/run_example.hpp"  // IWYU pragma: keep
#else
#include "platforms/posix/run_example.hpp"  // IWYU pragma: keep
#endif
