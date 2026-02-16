/// @file timeout_watchdog.h
/// @brief Internal timeout watchdog for test execution
///
/// Monitors test execution from within the test process and dumps stack trace
/// when a timeout occurs. This is separate from external timeout monitoring
/// by test_wrapper.py and provides earlier detection with better diagnostics.
///
/// Usage:
///   timeout_watchdog::setup(30.0);  // 30 second timeout
///   // ... run tests ...
///   timeout_watchdog::cancel();     // Cancel if tests complete
///
/// Environment variables:
///   FASTLED_TEST_TIMEOUT - Timeout in seconds (default: 20.0)
///   FASTLED_DISABLE_TIMEOUT_WATCHDOG - Set to "1" to disable

// IWYU pragma: no_include <minwindef.h>
// IWYU pragma: no_include "handleapi.h"
// IWYU pragma: no_include "stdio.h"
// IWYU pragma: no_include "stdlib.h"
// IWYU pragma: no_include "string.h"
// IWYU pragma: no_include "synchapi.h"
// IWYU pragma: no_include "winerror.h"

#ifndef TIMEOUT_WATCHDOG_H
#define TIMEOUT_WATCHDOG_H

#include "crash_handler.h"  // IWYU pragma: keep
#include "fl/stl/cstdio.h"  // IWYU pragma: keep
#include "fl/stl/cstdlib.h"  // IWYU pragma: keep
#include "fl/stl/cstring.h"  // IWYU pragma: keep
#include "fl/stl/atomic.h"

#ifdef _WIN32
    #include <windows.h>  // IWYU pragma: keep
    #include <processthreadsapi.h>
#else
    #include <csignal>
    #include <unistd.h>
    #include "fl/stl/thread.h"
#endif

namespace timeout_watchdog {

// Global state
static fl::atomic<bool> g_watchdog_active{false};
static fl::atomic<bool> g_watchdog_triggered{false};
static double g_timeout_seconds = 20.0;

#ifdef _WIN32
// Windows implementation using timer thread
static HANDLE g_timer_thread = nullptr;
static HANDLE g_main_thread = nullptr;
static fl::atomic<bool> g_should_exit{false};

inline DWORD WINAPI watchdog_timer_thread(LPVOID param) {
    double timeout_ms = *static_cast<double*>(param);
    DWORD sleep_time = static_cast<DWORD>(timeout_ms * 1000.0);

    // Wait for timeout or cancellation
    if (WaitForSingleObject(GetCurrentThread(), sleep_time) == WAIT_TIMEOUT) {
        if (g_watchdog_active.load() && !g_should_exit.load()) {
            // Timeout occurred!
            g_watchdog_triggered.store(true);

            fprintf(stderr, "\n");
            fprintf(stderr, "================================================================================\n");
            fprintf(stderr, "INTERNAL TIMEOUT WATCHDOG TRIGGERED\n");
            fprintf(stderr, "================================================================================\n");
            fprintf(stderr, "Test exceeded internal timeout of %.1f seconds\n", g_timeout_seconds);
            fprintf(stderr, "Dumping main thread stack trace...\n");
            fprintf(stderr, "================================================================================\n");
            fprintf(stderr, "\n");
            fflush(stderr);

            // Suspend main thread to get accurate stack trace
            if (g_main_thread != nullptr) {
                SuspendThread(g_main_thread);
            }

            // Dump stack trace from main thread context
            print_stacktrace();

            // Resume and exit
            if (g_main_thread != nullptr) {
                ResumeThread(g_main_thread);
            }

            fprintf(stderr, "\n");
            fprintf(stderr, "================================================================================\n");
            fprintf(stderr, "END TIMEOUT WATCHDOG\n");
            fprintf(stderr, "Exiting with code 1\n");
            fprintf(stderr, "================================================================================\n");
            fprintf(stderr, "\n");
            fflush(stderr);

            // Exit with code 1
            _exit(1);
        }
    }

    return 0;
}

inline void setup(double timeout_seconds = 20.0) {
    // Check if watchdog should be disabled
    const char* disable_watchdog = getenv("FASTLED_DISABLE_TIMEOUT_WATCHDOG");
    if (disable_watchdog && (strcmp(disable_watchdog, "1") == 0 || strcmp(disable_watchdog, "true") == 0)) {
        printf("Timeout watchdog disabled (FASTLED_DISABLE_TIMEOUT_WATCHDOG set)\n");
        return;
    }

    // Check for timeout override from environment
    const char* timeout_env = getenv("FASTLED_TEST_TIMEOUT");
    if (timeout_env) {
        double parsed_timeout = atof(timeout_env);
        if (parsed_timeout > 0.0) {
            timeout_seconds = parsed_timeout;
        }
    }

    g_timeout_seconds = timeout_seconds;
    g_watchdog_active.store(true);
    g_should_exit.store(false);
    g_watchdog_triggered.store(false);

    // Duplicate handle to main thread for stack walking
    DuplicateHandle(
        GetCurrentProcess(),
        GetCurrentThread(),
        GetCurrentProcess(),
        &g_main_thread,
        0,
        FALSE,
        DUPLICATE_SAME_ACCESS
    );

    // Start watchdog thread
    g_timer_thread = CreateThread(
        nullptr,
        0,
        watchdog_timer_thread,
        &g_timeout_seconds,
        0,
        nullptr
    );

    if (g_timer_thread != nullptr) {
        printf("⏱️  Internal timeout watchdog enabled (%.1f seconds)\n", timeout_seconds);
    } else {
        fprintf(stderr, "Warning: Failed to create watchdog thread\n");
    }
}

inline void cancel() {
    if (!g_watchdog_active.load()) {
        return;
    }

    g_watchdog_active.store(false);
    g_should_exit.store(true);

    // Wait for watchdog thread to exit
    if (g_timer_thread != nullptr) {
        WaitForSingleObject(g_timer_thread, 1000);  // 1 second timeout
        CloseHandle(g_timer_thread);
        g_timer_thread = nullptr;
    }

    // Clean up main thread handle
    if (g_main_thread != nullptr) {
        CloseHandle(g_main_thread);
        g_main_thread = nullptr;
    }
}

#else  // Unix/Linux/macOS

// Unix implementation using SIGALRM
static fl::thread::id g_main_thread_id;

inline void watchdog_signal_handler(int sig) {
    (void)sig;  // Unused

    if (!g_watchdog_active.load()) {
        return;
    }

    // Timeout occurred!
    g_watchdog_triggered.store(true);

    fprintf(stderr, "\n");
    fprintf(stderr, "================================================================================\n");
    fprintf(stderr, "INTERNAL TIMEOUT WATCHDOG TRIGGERED\n");
    fprintf(stderr, "================================================================================\n");
    fprintf(stderr, "Test exceeded internal timeout of %.1f seconds\n", g_timeout_seconds);
    fprintf(stderr, "Dumping main thread stack trace...\n");
    fprintf(stderr, "================================================================================\n");
    fprintf(stderr, "\n");
    fflush(stderr);

    // Dump stack trace
    print_stacktrace();

    fprintf(stderr, "\n");
    fprintf(stderr, "================================================================================\n");
    fprintf(stderr, "END TIMEOUT WATCHDOG\n");
    fprintf(stderr, "Exiting with code 1\n");
    fprintf(stderr, "================================================================================\n");
    fprintf(stderr, "\n");
    fflush(stderr);

    // Exit with code 1
    _exit(1);
}

inline void setup(double timeout_seconds = 20.0) {
    // Check if watchdog should be disabled
    const char* disable_watchdog = getenv("FASTLED_DISABLE_TIMEOUT_WATCHDOG");
    if (disable_watchdog && (strcmp(disable_watchdog, "1") == 0 || strcmp(disable_watchdog, "true") == 0)) {
        printf("Timeout watchdog disabled (FASTLED_DISABLE_TIMEOUT_WATCHDOG set)\n");
        return;
    }

    // Check for timeout override from environment
    const char* timeout_env = getenv("FASTLED_TEST_TIMEOUT");
    if (timeout_env) {
        double parsed_timeout = atof(timeout_env);
        if (parsed_timeout > 0.0) {
            timeout_seconds = parsed_timeout;
        }
    }

    g_timeout_seconds = timeout_seconds;
    g_watchdog_active.store(true);
    g_watchdog_triggered.store(false);
    g_main_thread_id = fl::this_thread::get_id();

    // Set up signal handler for SIGALRM
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = watchdog_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGALRM, &sa, nullptr) != 0) {
        fprintf(stderr, "Warning: Failed to install timeout watchdog signal handler\n");
        return;
    }

    // Set alarm
    alarm(static_cast<unsigned int>(timeout_seconds));

    printf("⏱️  Internal timeout watchdog enabled (%.1f seconds)\n", timeout_seconds);
}

inline void cancel() {
    if (!g_watchdog_active.load()) {
        return;
    }

    g_watchdog_active.store(false);

    // Cancel alarm
    alarm(0);

    // Restore default SIGALRM handler
    signal(SIGALRM, SIG_DFL);
}

#endif  // _WIN32

inline bool is_active() {
    return g_watchdog_active.load();
}

inline bool was_triggered() {
    return g_watchdog_triggered.load();
}

} // namespace timeout_watchdog

#endif // TIMEOUT_WATCHDOG_H
