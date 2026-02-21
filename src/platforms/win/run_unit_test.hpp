// ok no namespace fl
#pragma once

// IWYU pragma: private

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

#include "platforms/win/is_win.h"

#ifdef FL_IS_WIN

// IWYU pragma: begin_keep
#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <windef.h>
#include <libloaderapi.h>
// IWYU pragma: end_keep
#include "errhandlingapi.h"
#include "minwindef.h"
#include "winbase.h"
#include <processthreadsapi.h>  // IWYU pragma: keep
#include <synchapi.h>  // IWYU pragma: keep

// Crash handler setup (defined in crash_handler_main.cpp)
extern "C" void runner_setup_crash_handler();
// Stack trace printer (defined in crash_handler_main.cpp)
extern "C" void runner_print_stacktrace();
// Thread-specific stack trace (walks a suspended thread's stack)
extern "C" void runner_print_stacktrace_for_thread(void* thread_handle);

namespace runner_watchdog {

static HANDLE g_timer_thread = nullptr;
static HANDLE g_main_thread = nullptr;
static volatile bool g_active = false;
static double g_timeout_seconds = 20.0;

static DWORD WINAPI timer_thread_func(LPVOID) {
    DWORD sleep_ms = static_cast<DWORD>(g_timeout_seconds * 1000.0);
    Sleep(sleep_ms);

    if (!g_active) {
        return 0;
    }

    fprintf(stderr, "\n");
    fprintf(stderr, "================================================================================\n");
    fprintf(stderr, "RUNNER WATCHDOG TIMEOUT\n");
    fprintf(stderr, "================================================================================\n");
    fprintf(stderr, "Test exceeded runner timeout of %.1f seconds\n", g_timeout_seconds);
    fprintf(stderr, "Dumping main thread stack trace...\n");
    fprintf(stderr, "================================================================================\n");
    fprintf(stderr, "\n");
    fflush(stderr);

    if (g_main_thread != nullptr) {
        SuspendThread(g_main_thread);
        // Use StackWalk64 to walk the suspended main thread's stack
        runner_print_stacktrace_for_thread((void*)g_main_thread);
        ResumeThread(g_main_thread);
    } else {
        // Fallback: print calling thread's stack
        runner_print_stacktrace();
    }

    fprintf(stderr, "\n");
    fprintf(stderr, "================================================================================\n");
    fprintf(stderr, "END RUNNER WATCHDOG\n");
    fprintf(stderr, "Exiting with code 1\n");
    fprintf(stderr, "================================================================================\n");
    fprintf(stderr, "\n");
    fflush(stderr);

    _exit(1);
    return 0; // unreachable
}

static void setup(double timeout_seconds = 20.0) {
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
    g_active = true;

    DuplicateHandle(
        GetCurrentProcess(), GetCurrentThread(),
        GetCurrentProcess(), &g_main_thread,
        0, FALSE, DUPLICATE_SAME_ACCESS
    );

    g_timer_thread = CreateThread(nullptr, 0, timer_thread_func, nullptr, 0, nullptr);
    if (g_timer_thread != nullptr) {
        printf("Runner watchdog enabled (%.1f seconds)\n", timeout_seconds);
    }
}

static void cancel() {
    if (!g_active) {
        return;
    }
    g_active = false;

    if (g_timer_thread != nullptr) {
        WaitForSingleObject(g_timer_thread, 2000);
        CloseHandle(g_timer_thread);
        g_timer_thread = nullptr;
    }
    if (g_main_thread != nullptr) {
        CloseHandle(g_main_thread);
        g_main_thread = nullptr;
    }
}

} // namespace runner_watchdog

// Function signature for the test entry point exported by test DLLs/SOs
typedef int (*RunTestsFunc)(int argc, const char** argv);

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

    // Pre-load fastled.dll so that transitive dependencies resolve correctly.
    // Windows LoadLibraryA doesn't use PATH for resolving DLL dependencies,
    // so we pre-load fastled.dll into the process first. Once loaded, the
    // Windows loader will find it when resolving test DLL imports.
    {
        std::string fastled_dll_path;
        const char* env_dir = getenv("FASTLED_LIB_DIR");
        if (env_dir && env_dir[0] != '\0') {
            fastled_dll_path = std::string(env_dir) + "\\fastled.dll";
        } else {
            // Derive from test DLL path: go up one level into ci/meson/native/
            size_t slash = dll_path.find_last_of("\\/");
            if (slash != std::string::npos) {
                std::string dll_dir = dll_path.substr(0, slash);
                size_t parent_slash = dll_dir.find_last_of("\\/");
                if (parent_slash != std::string::npos) {
                    fastled_dll_path = dll_dir.substr(0, parent_slash) + "\\ci\\meson\\native\\fastled.dll";
                }
            }
        }
        if (!fastled_dll_path.empty()) {
            HMODULE fastled = LoadLibraryA(fastled_dll_path.c_str());
            if (!fastled) {
                // Also try SetDllDirectoryA as fallback
                size_t last_slash = fastled_dll_path.find_last_of("\\/");
                if (last_slash != std::string::npos) {
                    SetDllDirectoryA(fastled_dll_path.substr(0, last_slash).c_str());
                }
            }
        }
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

    // Start watchdog timer
    runner_watchdog::setup();

    // Call test function with arguments
    int test_result = run_tests(test_argc, test_argv);

    // Cancel watchdog - tests completed normally
    runner_watchdog::cancel();

    // Skip FreeLibrary: With shared library (fastled.dll), unloading the test
    // DLL triggers static destructors that may reference objects in fastled.dll,
    // causing access violations from DLL destruction order issues.
    // The process is about to exit anyway - the OS will clean up all memory.
    // This also benefits ASAN which needs the DLL loaded for leak symbolization.
    // See: https://github.com/google/sanitizers/issues/899

    return test_result;
}

#endif // FL_IS_WIN
