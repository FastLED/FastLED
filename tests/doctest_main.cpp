// Workaround for Clang 19 AVX-512 intrinsics bug on Windows
// The clang-tool-chain version 19 has buggy AVX-512 intrinsics that use __builtin_elementwise_popcount
// which is not available in this build. We prevent those headers from being included.
#ifdef _WIN32
// Prevent immintrin.h from including the buggy AVX-512 headers
#define __AVX512BITALG__
#define __AVX512VPOPCNTDQ__
#endif

// Undef Arduino macros that conflict with Windows SDK headers
#ifdef INPUT
#undef INPUT
#endif
#ifdef OUTPUT
#undef OUTPUT
#endif

// Suppress -Wpragma-pack warnings from Windows SDK headers
#ifdef _WIN32
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpragma-pack"
#endif
#endif

// IWYU pragma: no_include "ios"
// IWYU pragma: no_include "iostream"
// IWYU pragma: no_include "ratio"
// IWYU pragma: no_include "test.h"

// Include fl_unittest.h for test registration and execution
#include "shared/fl_unittest.h"

#ifdef _WIN32
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#endif

#ifdef ENABLE_CRASH_HANDLER
#include "crash_handler.h"
#endif

// Timeout watchdog only in standalone mode (not DLL mode)
// In DLL mode, runner.exe manages timeouts externally
#ifndef TEST_DLL_MODE
#include "timeout_watchdog.h"
#endif

#include "platforms/stub/task_coroutine_stub.h"
#include "platforms/stub/coroutine_runner.h"
#include "platforms/esp/32/drivers/parlio/parlio_peripheral_mock.h"
#include "fl/stl/cstdlib.h"
#include "fl/stl/shared_ptr.h"
#include <iostream>

// This file contains the main function for the custom test framework (fl_unittest)
// It will be compiled once and linked to all test executables
//
// When building as a DLL (TEST_DLL_MODE defined), it exports a run_tests() function
// instead of defining main()

namespace testing_detail {

void fl_cleanup() {
    // Clean up all background threads before DLL unload to prevent access violations
    // This includes both coroutine threads and promise resolver threads
    fl::platforms::cleanup_coroutine_threads();

    // Clean up PARLIO mock to prevent LSAN leak false positive reports.
    fl::detail::cleanup_parlio_mock();
}

int fl_run_tests(int argc, const char** argv) {
    // NOTE: Crash handler is setup by runner.exe BEFORE loading this DLL
    // We do NOT call setup_crash_handler() here to avoid duplicate setup
    // and to keep crash handler dependencies (dbghelp, psapi) in runner.exe only

    // NOTE: Timeout watchdog is NOT enabled in DLL mode - runner.exe and test_wrapper.py
    // provide external timeout monitoring with better stack trace capabilities

    // Pre-initialize CoroutineRunner singleton to avoid DLL hang on first access
    std::cout << "Pre-initializing CoroutineRunner singleton" << std::endl;
    fl::detail::CoroutineRunner::instance();
    std::cout << "CoroutineRunner singleton pre-initialized successfully" << std::endl;

    // Run fl_unittest test framework
    fl::test::RunOptions opts = fl::test::parse_args(argc, (const char**)argv);
    int result = fl::test::run_all(opts);

    fl_cleanup();
    return result;
}

int fl_main(int argc, char** argv) {
#ifdef ENABLE_CRASH_HANDLER
    setup_crash_handler();
#endif

#ifndef TEST_DLL_MODE
    // Setup internal timeout watchdog (detects hangs from within test process)
    // Only in standalone mode - DLL mode uses external monitoring
    timeout_watchdog::setup();  // Default: 20 seconds, configurable via FASTLED_TEST_TIMEOUT
#endif

    // Run fl_unittest test framework
    fl::test::RunOptions opts = fl::test::parse_args(argc, (const char**)argv);
    int result = fl::test::run_all(opts);

#ifndef TEST_DLL_MODE
    // Cancel watchdog before cleanup (tests completed successfully)
    timeout_watchdog::cancel();
#endif

    fl_cleanup();
    return result;
}

} // namespace testing_detail

#ifdef TEST_DLL_MODE
// DLL mode: Export run_tests function
#ifdef _WIN32
    #define TEST_DLL_EXPORT __declspec(dllexport)
#else
    #define TEST_DLL_EXPORT __attribute__((visibility("default")))
#endif

extern "C" TEST_DLL_EXPORT int run_tests(int argc, const char** argv) {
    return testing_detail::fl_run_tests(argc, argv);
}

#else
// Standard mode: Define main function
int main(int argc, char** argv) {
    return testing_detail::fl_main(argc, argv);
}
#endif // TEST_DLL_MODE
