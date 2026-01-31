// ok no namespace fl
#pragma once

/// @file run_unit_test.hpp
/// Windows implementation of unit test runner
///
/// This file provides the main() function for loading and executing FastLED
/// test DLLs on Windows using the Win32 API (LoadLibraryA, GetProcAddress).
///
/// Usage: runner.exe <test_dll_path> [doctest args...]
/// Or:    <test_name>.exe (auto-loads <test_name>.dll from same directory)
///
/// NOTE: Uses std:: types to avoid FastLED dependencies in the runner.

#ifdef _WIN32

#include <windef.h>
#include <libloaderapi.h>
#include "errhandlingapi.h"
#include "minwindef.h"
#include "winbase.h"

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

    // Get run_tests function
    RunTestsFunc run_tests = (RunTestsFunc)GetProcAddress(dll, "run_tests");
    if (!run_tests) {
        std::cout << "Error: Failed to find run_tests() in " << dll_path << std::endl;
        FreeLibrary(dll);
        return 1;
    }

    // Prepare arguments for run_tests (skip DLL path if it was provided)
    int test_argc;
    const char** test_argv;
    std::vector<const char*> adjusted_argv;

    if (argc > 1 && argv[1][0] != '-') {
        // DLL path was provided: skip it in arguments passed to tests
        test_argc = argc - 1;
        adjusted_argv.push_back(argv[0]); // Keep program name
        for (int i = 2; i < argc; ++i) {
            adjusted_argv.push_back(argv[i]);
        }
        test_argv = adjusted_argv.data();
    } else {
        // No DLL path: pass all arguments
        test_argc = argc;
        test_argv = const_cast<const char**>(argv);
    }

    // Call test function with arguments
    int test_result = run_tests(test_argc, test_argv);

    // Cleanup: Skip FreeLibrary when running with AddressSanitizer
    // ASAN runs leak detection at program exit. If we FreeLibrary() the DLL
    // before that, ASAN cannot symbolize addresses from the unloaded library,
    // resulting in "<unknown module>" in stack traces.
    // See: https://github.com/google/sanitizers/issues/899
#if !defined(__SANITIZE_ADDRESS__)
    FreeLibrary(dll);
#endif

    return test_result;
}

#endif // _WIN32
