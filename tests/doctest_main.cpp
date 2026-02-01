// Workaround for Clang 19 AVX-512 intrinsics bug on Windows
// The clang-tool-chain version 19 has buggy AVX-512 intrinsics that use __builtin_elementwise_popcount
// which is not available in this build. We prevent those headers from being included.
#ifdef _WIN32
// Prevent immintrin.h from including the buggy AVX-512 headers
#define __AVX512BITALG__
#define __AVX512VPOPCNTDQ__
#endif

// Fix INPUT macro conflict between Arduino and Windows headers
#ifdef INPUT
#define ARDUINO_INPUT_BACKUP INPUT
#undef INPUT
#endif

// Suppress -Wpragma-pack warnings from Windows SDK headers
// These warnings come from Microsoft's own headers (winnt.h, wingdi.h, etc.) which use
// #pragma pack(push/pop) to control struct alignment. Clang warns about these alignment
// changes, but they're intentional and properly balanced in the SDK headers.
#ifdef _WIN32
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpragma-pack"
#endif
#endif

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

#ifdef _WIN32
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#endif

// Restore Arduino INPUT macro after Windows headers
#ifdef ARDUINO_INPUT_BACKUP
#define INPUT ARDUINO_INPUT_BACKUP
#undef ARDUINO_INPUT_BACKUP
#endif

#ifdef ENABLE_CRASH_HANDLER
#include "crash_handler.h"
#endif


#include "platforms/stub/task_coroutine_stub.h"
#include "platforms/stub/coroutine_runner.h"
#include "platforms/esp/32/drivers/parlio/parlio_peripheral_mock.h"
#include "fl/stl/iostream.h"  // For debug output
#include "fl/stl/algorithm.h"
#include "fl/stl/ostream.h"

// This file contains the main function for doctest
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

    // Pre-initialize CoroutineRunner singleton to avoid DLL hang on first access
    std::cout << "Pre-initializing CoroutineRunner singleton" << std::endl;
    fl::detail::CoroutineRunner::instance();
    std::cout << "CoroutineRunner singleton pre-initialized successfully" << std::endl;
    // Run doctest
    doctest::Context context(argc, argv);
    int result = context.run();
    fl_cleanup();
    return result;
}

int fl_main(int argc, char** argv) {
#ifdef ENABLE_CRASH_HANDLER
    setup_crash_handler();
#endif
    int result = doctest::Context(argc, argv).run();
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
    testing_detail::fl_main(argc, argv);
}
#endif // TEST_DLL_MODE
