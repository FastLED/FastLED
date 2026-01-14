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

#include <dlfcn.h> // For dlopen, dlsym, dlclose
#include <string.h> // For strrchr

#ifdef __APPLE__
#include <mach-o/dyld.h> // For _NSGetExecutablePath
#endif

int main(int argc, char** argv) {
    // Setup crash handler BEFORE loading any shared libraries
    runner_setup_crash_handler();
    std::string so_path;

    // Determine shared library extension
#ifdef __APPLE__
    const std::string shared_lib_ext = ".dylib";
#else
    const std::string shared_lib_ext = ".so";
#endif

    // Determine shared library path: explicit argument or inferred from exe name
    if (argc > 1 && argv[1][0] != '-') {
        // First argument is a path (doesn't start with -)
        so_path = argv[1];
    } else {
        // No explicit path: infer from exe name
        std::string full_exe_path;
#ifdef __APPLE__
        char path_buf[1024];
        uint32_t buf_size = sizeof(path_buf);
        if (_NSGetExecutablePath(path_buf, &buf_size) == 0) {
            full_exe_path = path_buf;
        } else {
            // Buffer too small, try with larger buffer (not strictly necessary for this example)
            // For simplicity, we'll just use argv[0] if _NSGetExecutablePath fails
            full_exe_path = argv[0];
        }
#else // Linux
        // On Linux, /proc/self/exe is a symlink to the executable
        char path_buf[1024];
        ssize_t len = readlink("/proc/self/exe", path_buf, sizeof(path_buf) - 1);
        if (len != -1) {
            path_buf[len] = '\0';
            full_exe_path = path_buf;
        } else {
            // Fallback to argv[0] if readlink fails
            full_exe_path = argv[0];
        }
#endif
        if (full_exe_path.empty()) {
            std::cerr << "Error: Failed to get executable path" << std::endl;
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

    // Load shared library
    void* handle = dlopen(so_path.c_str(), RTLD_LAZY);
    if (!handle) {
        std::cerr << "Error: Failed to load " << so_path << " (" << dlerror() << ")" << std::endl;
        return 1;
    }

    // Get run_example function
    RunExampleFunc run_example = (RunExampleFunc)dlsym(handle, "run_example");
    if (!run_example) {
        std::cerr << "Error: Failed to find run_example() in " << so_path << " (" << dlerror() << ")" << std::endl;
        dlclose(handle);
        return 1;
    }

    // Prepare arguments for run_example (skip shared library path if it was provided)
    int example_argc;
    const char** example_argv;
    std::vector<const char*> adjusted_argv;

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
