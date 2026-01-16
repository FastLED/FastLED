// ok no namespace fl
#pragma once

/// @file run_example.hpp
/// Apple/macOS implementation of example runner
///
/// This file provides the main() function for loading and executing FastLED
/// example shared libraries on macOS using dlopen/dlsym.
///
/// Usage: runner <example_dylib_path> [args...]
/// Or:    <example_name> (auto-loads <example_name>.dylib from same directory)

#ifdef __APPLE__

#include <dlfcn.h>       // For dlopen, dlsym, dlclose
#include <mach-o/dyld.h> // For _NSGetExecutablePath

int main(int argc, char** argv) {
    // Setup crash handler BEFORE loading any shared libraries
    runner_setup_crash_handler();

    fl::string so_path;
    const fl::string shared_lib_ext = ".dylib";

    // Determine shared library path: explicit argument or inferred from exe name
    if (argc > 1 && argv[1][0] != '-') {
        // First argument is a path (doesn't start with -)
        so_path = argv[1];
    } else {
        // No explicit path: infer from exe name
        fl::string full_exe_path;
        char path_buf[1024];
        uint32_t buf_size = sizeof(path_buf);
        if (_NSGetExecutablePath(path_buf, &buf_size) == 0) {
            full_exe_path = path_buf;
        } else {
            // Buffer too small, fallback to argv[0]
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

    // Get run_example function
    RunExampleFunc run_example = (RunExampleFunc)dlsym(handle, "run_example");
    if (!run_example) {
        fl::cout << "Error: Failed to find run_example() in " << so_path << " (" << dlerror() << ")" << fl::endl;
        dlclose(handle);
        return 1;
    }

    // Prepare arguments for run_example (skip shared library path if it was provided)
    int example_argc;
    const char** example_argv;
    fl::vector<const char*> adjusted_argv;

    if (argc > 1 && argv[1][0] != '-') {
        // Shared library path was provided: skip it in arguments passed to example
        example_argc = argc - 1;
        adjusted_argv.push_back(argv[0]); // Keep program name
        for (int i = 2; i < argc; ++i) {
            adjusted_argv.push_back(argv[i]);
        }
        example_argv = adjusted_argv.data();
    } else {
        // No shared library path: pass all arguments
        example_argc = argc;
        example_argv = const_cast<const char**>(argv);
    }

    // Call example function with arguments
    int example_result = run_example(example_argc, example_argv);

    // Cleanup
    dlclose(handle);

    return example_result;
}

#endif // __APPLE__
