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

// IWYU pragma: no_include "ios"
// IWYU pragma: no_include "iostream"
// IWYU pragma: no_include "ratio"
// IWYU pragma: no_include "test.h"

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

// This file contains the main function for doctest
// It will be compiled once and linked to all test executables
//
// When building as a DLL (TEST_DLL_MODE defined), it exports a run_tests() function
// instead of defining main()

namespace testing_detail {

// Custom reporter that warns about slow test cases
// This wraps the console reporter and adds timing warnings
struct TimingReporter : public doctest::IReporter {
    std::chrono::steady_clock::time_point start_time;
    const doctest::TestCaseData* current_test = nullptr;
    const doctest::ContextOptions& opt;
    std::unique_ptr<doctest::IReporter> console_reporter;

    explicit TimingReporter(const doctest::ContextOptions& in)
        : opt(in)
        , console_reporter(nullptr)
    {
        // Create console reporter to delegate to
        console_reporter.reset(new doctest::ConsoleReporter(in));
    }

    // Delegate all reporting to console reporter
    void report_query(const doctest::QueryData& qd) override {
        console_reporter->report_query(qd);
    }

    void test_run_start() override {
        console_reporter->test_run_start();
    }

    void test_run_end(const doctest::TestRunStats& ts) override {
        console_reporter->test_run_end(ts);
    }

    void test_case_start(const doctest::TestCaseData& data) override {
        current_test = &data;
        start_time = std::chrono::steady_clock::now();
        console_reporter->test_case_start(data);
    }

    void test_case_reenter(const doctest::TestCaseData& data) override {
        console_reporter->test_case_reenter(data);
    }

    void test_case_end(const doctest::CurrentTestCaseStats& stats) override {
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        // Warn if test case took longer than 1 second
        constexpr int WARNING_THRESHOLD_MS = 1000;
        if (duration.count() > WARNING_THRESHOLD_MS && current_test) {
            std::cout << "WARNING: Test case '"
                      << current_test->m_file << ":" << current_test->m_line
                      << " - " << current_test->m_name
                      << "' took " << (duration.count() / 1000.0) << " seconds to run "
                      << "(threshold: " << (WARNING_THRESHOLD_MS / 1000.0) << " second)"
                      << std::endl;
        }

        console_reporter->test_case_end(stats);
    }

    void test_case_exception(const doctest::TestCaseException& ex) override {
        console_reporter->test_case_exception(ex);
    }

    void subcase_start(const doctest::SubcaseSignature& sig) override {
        console_reporter->subcase_start(sig);
    }

    void subcase_end() override {
        console_reporter->subcase_end();
    }

    void log_assert(const doctest::AssertData& ad) override {
        console_reporter->log_assert(ad);
    }

    void log_message(const doctest::MessageData& md) override {
        console_reporter->log_message(md);
    }

    void test_case_skipped(const doctest::TestCaseData& data) override {
        console_reporter->test_case_skipped(data);
    }
};

// Register the custom reporter
DOCTEST_REGISTER_REPORTER("timing", 0, TimingReporter);

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

    // Run doctest with timing reporter
    doctest::Context context(argc, argv);

    // Register timing reporter to warn about slow test cases
    context.addFilter("reporters", "timing");
    context.setOption("no-colors", false);  // Enable colors for warnings

    int result = context.run();

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

    // Run doctest with timing reporter
    doctest::Context context(argc, argv);

    // Register timing reporter to warn about slow test cases
    context.addFilter("reporters", "timing");
    context.setOption("no-colors", false);  // Enable colors for warnings

    int result = context.run();

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
    testing_detail::fl_main(argc, argv);
}
#endif // TEST_DLL_MODE
