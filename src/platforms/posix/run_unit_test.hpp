// ok no namespace fl
#pragma once

/// @file run_unit_test.hpp
/// Linux/POSIX implementation of unit test runner
///
/// This file provides the main() function for loading and executing FastLED
/// test shared libraries on Linux using dlopen/dlsym.
///
/// Usage: runner <test_so_path> [doctest args...]
/// Or:    <test_name> (auto-loads <test_name>.so from same directory)

#if !defined(_WIN32) && !defined(__APPLE__)

#include <dlfcn.h>  // For dlopen, dlsym, dlclose
#include <unistd.h> // For readlink

int main(int argc, char** argv) {
    // Setup crash handler BEFORE loading any shared libraries
    runner_setup_crash_handler();

    fl::string so_path;
    const fl::string shared_lib_ext = ".so";

    // Determine shared library path: explicit argument or inferred from exe name
    if (argc > 1 && argv[1][0] != '-') {
        // First argument is a path (doesn't start with -)
        so_path = argv[1];
    } else {
        // No explicit path: infer from exe name
        // On Linux, /proc/self/exe is a symlink to the executable
        fl::string full_exe_path;
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
            fl::cout << "Error: Failed to get executable path" << fl::endl;
            return 1;
        }

        // Extract directory and filename
        size_t last_slash = full_exe_path.find_last_of('/');
        fl::string exe_dir = (last_slash != fl::string::npos) ? full_exe_path.substr(0, last_slash) : ".";
        fl::string exe_file = (last_slash != fl::string::npos) ? full_exe_path.substr(last_slash + 1) : full_exe_path;

        // Remove executable extension (if any)
        size_t dot_pos = exe_file.find_last_of('.');
        fl::string exe_name = (dot_pos != fl::string::npos) ? exe_file.substr(0, dot_pos) : exe_file;

        // Construct shared library path
        fl::string so_name = exe_name + shared_lib_ext;
        so_path = exe_dir + "/" + so_name;
    }

    // Load shared library
    void* handle = dlopen(so_path.c_str(), RTLD_LAZY);
    if (!handle) {
        fl::cout << "Error: Failed to load " << so_path << " (" << dlerror() << ")" << fl::endl;
        return 1;
    }

    // Get run_tests function
    RunTestsFunc run_tests = (RunTestsFunc)dlsym(handle, "run_tests");
    if (!run_tests) {
        fl::cout << "Error: Failed to find run_tests() in " << so_path << " (" << dlerror() << ")" << fl::endl;
        dlclose(handle);
        return 1;
    }

    // Prepare arguments for run_tests (skip shared library path if it was provided)
    int test_argc;
    const char** test_argv;
    fl::vector<const char*> adjusted_argv;

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

    // Cleanup
    dlclose(handle);

    return test_result;
}

#endif // !_WIN32 && !__APPLE__
