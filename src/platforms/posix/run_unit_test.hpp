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

// IWYU pragma: begin_keep
#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <csignal>
#include <ctime>    // For clock_gettime(CLOCK_PROCESS_CPUTIME_ID)
#include <dlfcn.h>  // For dlopen, dlsym, dlclose
#include <unistd.h> // For readlink, alarm
#include "fl/stl/noexcept.h"
// IWYU pragma: end_keep

// Crash handler setup (defined in crash_handler_main.cpp)
extern "C" void runner_setup_crash_handler();
// Stack trace printer (defined in crash_handler_main.cpp)
extern "C" void runner_print_stacktrace();

namespace runner_watchdog {

static volatile bool g_active = false;
static double g_timeout_seconds = 20.0;
static unsigned int g_window_seconds = 20;
static unsigned int g_windows_elapsed = 0;
static double g_window_start_cpu = 0.0;

// Hard cap on extensions: a test never runs past 5x its timeout in wall time.
static const unsigned int kMaxWindows = 5;
// A window counts as progress when the test got at least this share of one
// CPU in it. A deadlocked test sits near zero; one starved by a loaded host
// (load average 200+ on 16 cores was measured) still runs a few percent.
static const double kMinProgressShare = 0.05;

// Process CPU time in seconds. clock_gettime is async-signal-safe.
static double process_cpu_seconds() FL_NO_EXCEPT {
    struct timespec ts;
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) != 0) {
        return 0.0;
    }
    return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) * 1e-9;
}

static void alarm_handler(int) FL_NO_EXCEPT {
    if (!g_active) {
        return;
    }

    // The deadline is wall-clock, so on a starved host a test that is still
    // working can reach it. Re-arm while the test made progress in the last
    // window and has not used its whole timeout in CPU time; a hang (no CPU),
    // a spin (CPU budget spent) or the 5x wall cap still fails it.
    const double cpu = process_cpu_seconds();
    const double window_cpu = cpu - g_window_start_cpu;
    ++g_windows_elapsed;
    if (g_windows_elapsed < kMaxWindows && cpu < g_timeout_seconds &&
        window_cpu >= kMinProgressShare * static_cast<double>(g_window_seconds)) {
        g_window_start_cpu = cpu;
        alarm(g_window_seconds);
        return;
    }

    fprintf(stderr, "\n");
    fprintf(stderr, "================================================================================\n");
    fprintf(stderr, "RUNNER WATCHDOG TIMEOUT\n");
    fprintf(stderr, "================================================================================\n");
    fprintf(stderr, "Test exceeded runner timeout of %.1f seconds\n", g_timeout_seconds);
    fprintf(stderr, "(%.1f s CPU in total, %.2f s in the last %u s window)\n", cpu,
            window_cpu, g_window_seconds);
    fprintf(stderr, "Dumping stack trace...\n");
    fprintf(stderr, "================================================================================\n");
    fprintf(stderr, "\n");

    runner_print_stacktrace();

    fprintf(stderr, "\n");
    fprintf(stderr, "================================================================================\n");
    fprintf(stderr, "END RUNNER WATCHDOG\n");
    fprintf(stderr, "Exiting with code 1\n");
    fprintf(stderr, "================================================================================\n");
    fprintf(stderr, "\n");

    _exit(1);
}

static void setup(double timeout_seconds = 20.0) FL_NO_EXCEPT {
    const char* disable_env = getenv("FASTLED_DISABLE_TIMEOUT_WATCHDOG");
    if (disable_env && (strcmp(disable_env, "1") == 0 || strcmp(disable_env, "true") == 0)) {
        return;
    }

    const char* timeout_env = getenv("FASTLED_TEST_TIMEOUT");
    if (timeout_env) {
        double parsed = atof(timeout_env);
        if (parsed > 0.0) {
            timeout_seconds = parsed;
        }
    }

    g_timeout_seconds = timeout_seconds;
    g_window_seconds = static_cast<unsigned int>(timeout_seconds);
    if (g_window_seconds == 0) {
        g_window_seconds = 1;
    }
    g_windows_elapsed = 0;
    g_window_start_cpu = process_cpu_seconds();
    g_active = true;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = alarm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGALRM, &sa, nullptr) == 0) {
        alarm(g_window_seconds);
        printf("Runner watchdog enabled (%.1f seconds)\n", timeout_seconds);
    }
}

static void cancel() FL_NO_EXCEPT {
    if (!g_active) {
        return;
    }
    g_active = false;
    alarm(0);
    signal(SIGALRM, SIG_DFL);
}

} // namespace runner_watchdog

// Function signature for the test entry point exported by test DLLs/SOs
typedef int (*RunTestsFunc)(int argc, const char** argv);

int main(int argc, char** argv) FL_NO_EXCEPT {
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

    // Start watchdog timer
    runner_watchdog::setup();

    // Call test function with arguments
    int test_result = run_tests(test_argc, test_argv);

    // Cancel watchdog - tests completed normally
    runner_watchdog::cancel();

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
