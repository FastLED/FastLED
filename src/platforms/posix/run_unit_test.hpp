// ok no namespace fl
#pragma once

// IWYU pragma: private

/// @file run_unit_test.hpp
/// Linux/POSIX implementation of unit test runner
///
/// This file provides the main() function for loading and executing FastLED
/// test shared libraries on Linux using dlopen/dlsym.
///
/// Usage: runner <test_so_path> [doctest args...]
/// Or:    <test_name> (auto-loads <test_name>.so from same directory)
///
/// NOTE: Uses std:: types to avoid FastLED dependencies in the runner.

#include "platforms/posix/is_posix.h"
#include "platforms/win/is_win.h"

#if !defined(FL_IS_WIN) && !defined(FL_IS_APPLE)

#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include <cstddef>
#include <dlfcn.h>  // For dlopen, dlsym, dlclose
#include <unistd.h> // For readlink

// Crash handler setup (defined in crash_handler_main.cpp)
extern "C" void runner_setup_crash_handler();

// Function signature for the test entry point exported by test DLLs/SOs
typedef int (*RunTestsFunc)(int argc, const char** argv);

int main(int argc, char** argv) {
    // Setup crash handler BEFORE loading any shared libraries
    runner_setup_crash_handler();

    std::string so_path;
    const std::string shared_lib_ext = ".so";

    // Determine shared library path: explicit argument or inferred from exe name
    if (argc > 1 && argv[1][0] != '-') {
        // First argument is a path (doesn't start with -)
        so_path = argv[1];
    } else {
        // No explicit path: infer from exe name
        // On Linux, /proc/self/exe is a symlink to the executable
        std::string full_exe_path;
        char path_buf[1024];
        ssize_t len = readlink("/proc/self/exe", path_buf, sizeof(path_buf) - 1);
        if (len != -1) {
            path_buf[len] = '\0';
            full_exe_path = path_buf;
        } else {
            // Fallback to argv[0] if readlink fails
            full_exe_path = argv[0];
        }

        if (full_exe_path.empty()) {
            std::cout << "Error: Failed to get executable path" << std::endl;
            return 1;
        }

        // Extract directory and filename
        size_t last_slash = full_exe_path.find_last_of('/');
        std::string exe_dir = (last_slash != std::string::npos) ? full_exe_path.substr(0, last_slash) : ".";
        std::string exe_file = (last_slash != std::string::npos) ? full_exe_path.substr(last_slash + 1) : full_exe_path;

        // Remove executable extension (if any)
        size_t dot_pos = exe_file.find_last_of('.');
        std::string exe_name = (dot_pos != std::string::npos) ? exe_file.substr(0, dot_pos) : exe_file;

        // Construct shared library path
        std::string so_name = exe_name + shared_lib_ext;
        so_path = exe_dir + "/" + so_name;
    }

    // Load shared library with RTLD_NOW for immediate symbol resolution
    // This helps ASAN properly track symbols from the loaded library
    void* handle = dlopen(so_path.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        std::cout << "Error: Failed to load " << so_path << " (" << dlerror() << ")" << std::endl;
        return 1;
    }

    // Get run_tests function
    RunTestsFunc run_tests = (RunTestsFunc)dlsym(handle, "run_tests");
    if (!run_tests) {
        std::cout << "Error: Failed to find run_tests() in " << so_path << " (" << dlerror() << ")" << std::endl;
        dlclose(handle);
        return 1;
    }

    // Prepare arguments for run_tests (skip shared library path if it was provided)
    int test_argc;
    const char** test_argv;
    std::vector<const char*> adjusted_argv;

    if (argc > 1 && argv[1][0] != '-') {
        // Shared library path was provided: skip it in arguments passed to tests
        test_argc = argc - 1;
        adjusted_argv.push_back(argv[0]); // Keep program name
        for (int i = 2; i < argc; ++i) {
            adjusted_argv.push_back(argv[i]);
        }
        test_argv = adjusted_argv.data();
    } else {
        // No shared library path: pass all arguments
        test_argc = argc;
        test_argv = const_cast<const char**>(argv);
    }

    // Call test function with arguments
    int test_result = run_tests(test_argc, test_argv);

    // Cleanup: Skip dlclose when running with AddressSanitizer
    // ASAN runs leak detection at program exit. If we dlclose() the shared library
    // before that, ASAN cannot symbolize addresses from the unloaded library,
    // resulting in "<unknown module>" in stack traces.
    // See: https://github.com/google/sanitizers/issues/899
#if !defined(__SANITIZE_ADDRESS__) && !defined(__has_feature)
    dlclose(handle);
#elif defined(__has_feature)
#if !__has_feature(address_sanitizer)
    dlclose(handle);
#endif
#endif

    return test_result;
}

#endif // !FL_IS_WIN && !FL_IS_APPLE
