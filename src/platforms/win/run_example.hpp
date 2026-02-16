// ok no namespace fl
#pragma once

// IWYU pragma: private

/// @file run_example.hpp
/// Windows implementation of example runner
///
/// This file provides the main() function for loading and executing FastLED
/// example DLLs on Windows using the Win32 API (LoadLibraryA, GetProcAddress).
///
/// Usage: runner.exe <example_dll_path> [args...]
/// Or:    <example_name>.exe (auto-loads <example_name>.dll from same directory)
///
/// NOTE: Uses std:: types to avoid FastLED dependencies in the runner.

#include "platforms/win/is_win.h"

#ifdef FL_IS_WIN

// IWYU pragma: begin_keep
#include <iostream>
#include <string>
#include <vector>
#include <cstddef>
#include <windef.h>
#include <libloaderapi.h>
#include "errhandlingapi.h"
#include "minwindef.h"
#include "winbase.h"
// IWYU pragma: end_keep

// Crash handler setup (defined in crash_handler_main.cpp)
extern "C" void runner_setup_crash_handler();

// Function signature for the example entry point exported by example DLLs/SOs
typedef int (*RunExampleFunc)(int argc, const char** argv);

int main(int argc, char** argv) {
    // Setup crash handler BEFORE loading any DLLs
    // This ensures crash handling is active for the entire process lifetime
    runner_setup_crash_handler();
    std::string dll_path;

    // Determine DLL path: explicit argument or inferred from exe name
    if (argc > 1 && argv[1][0] != '-') {
        // First argument is a path (doesn't start with -)
        dll_path = argv[1];
    } else {
        // No explicit path: infer from exe name
        char exe_path[MAX_PATH];
        DWORD result = GetModuleFileNameA(NULL, exe_path, MAX_PATH);

        if (result == 0 || result == MAX_PATH) {
            std::cout << "Error: Failed to get executable path" << std::endl;
            return 1;
        }

        // Extract directory and filename
        std::string full_path(exe_path);
        size_t last_slash = full_path.find_last_of("\\/");
        std::string exe_dir = (last_slash != std::string::npos) ? full_path.substr(0, last_slash) : ".";
        std::string exe_file = (last_slash != std::string::npos) ? full_path.substr(last_slash + 1) : full_path;

        // Remove .exe extension
        size_t dot_pos = exe_file.find_last_of('.');
        std::string exe_name = (dot_pos != std::string::npos) ? exe_file.substr(0, dot_pos) : exe_file;

        // Construct DLL path
        std::string dll_name = exe_name + ".dll";
        dll_path = exe_dir + "\\" + dll_name;
    }

    // Load DLL
    HMODULE dll = LoadLibraryA(dll_path.c_str());
    if (!dll) {
        DWORD error = GetLastError();
        std::cout << "Error: Failed to load " << dll_path << " (error code: " << error << ")" << std::endl;
        return 1;
    }

    // Get run_example function
    RunExampleFunc run_example = (RunExampleFunc)GetProcAddress(dll, "run_example");
    if (!run_example) {
        std::cout << "Error: Failed to find run_example() in " << dll_path << std::endl;
        FreeLibrary(dll);
        return 1;
    }

    // Prepare arguments for run_example (skip DLL path if it was provided)
    int example_argc;
    const char** example_argv;
    std::vector<const char*> adjusted_argv;

    if (argc > 1 && argv[1][0] != '-') {
        // DLL path was provided: skip it in arguments passed to example
        example_argc = argc - 1;
        adjusted_argv.push_back(argv[0]); // Keep program name
        for (int i = 2; i < argc; ++i) {
            adjusted_argv.push_back(argv[i]);
        }
        example_argv = adjusted_argv.data();
    } else {
        // No DLL path: pass all arguments
        example_argc = argc;
        example_argv = const_cast<const char**>(argv);
    }

    // Call example function with arguments
    int example_result = run_example(example_argc, example_argv);

    // Cleanup: Skip FreeLibrary when running with AddressSanitizer
    // ASAN runs leak detection at program exit. If we FreeLibrary() the DLL
    // before that, ASAN cannot symbolize addresses from the unloaded library,
    // resulting in "<unknown module>" in stack traces.
    // See: https://github.com/google/sanitizers/issues/899
#if !defined(__SANITIZE_ADDRESS__)
    FreeLibrary(dll);
#endif

    return example_result;
}

#endif // FL_IS_WIN
