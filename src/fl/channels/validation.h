// src/fl/channels/validation.h
//
// Validation test logic - stateless single-test execution
// Extracted from examples/validation for unit testing

#pragma once

#include "fl/stl/vector.h"
#include "fl/stl/string.h"
#include "fl/stl/optional.h"
#include "fl/int.h"

// Forward declarations for detail modules
namespace fl {
namespace validation {
    class RxTest;
    class ResultFormatter;
    class Platform;
}
}

namespace fl {

/// @brief Single test configuration - fully stateless
struct SingleTestConfig {
    string driver_name;           ///< Driver to test (e.g., "PARLIO", "RMT")
    vector<int> lane_sizes;       ///< LED count per lane [100, 100, 200]
    string pattern;               ///< Test pattern name (default: "MSB_LSB_A")
    int iterations;               ///< Number of test iterations (default: 1)
    int pin_tx;                   ///< TX pin (base pin for multi-lane)
    int pin_rx;                   ///< RX pin

    SingleTestConfig()
        : pattern("MSB_LSB_A")
        , iterations(1)
        , pin_tx(1)
        , pin_rx(0) {}
};

/// @brief Single test result - comprehensive pass/fail information
struct SingleTestResult {
    bool success;                 ///< RPC execution succeeded
    bool passed;                  ///< All validation tests passed
    int total_tests;              ///< Total validation tests run
    int passed_tests;             ///< Number of tests that passed
    u32 duration_ms;              ///< Test execution time (milliseconds)
    string driver;                ///< Driver tested
    int lane_count;               ///< Number of lanes tested
    vector<int> lane_sizes;       ///< LED counts per lane
    string pattern;               ///< Pattern tested

    // Optional failure info
    optional<string> error_message;       ///< Error message if !success
    optional<string> failure_pattern;     ///< Pattern that failed if !passed
    optional<string> failure_details;     ///< Failure details

    SingleTestResult()
        : success(false)
        , passed(false)
        , total_tests(0)
        , passed_tests(0)
        , duration_ms(0)
        , lane_count(0) {}
};

/// @brief Driver test result tracking (moved from ValidationTest.h)
struct DriverTestResult {
    fl::string driver_name;  ///< Driver name (e.g., "RMT", "SPI", "PARLIO")
    int total_tests;         ///< Total test count across all chipset timings
    int passed_tests;        ///< Passed test count across all chipset timings
    bool skipped;            ///< True if driver was skipped (e.g., failed to set exclusive)

    DriverTestResult(const char* name)
        : driver_name(name)
        , total_tests(0)
        , passed_tests(0)
        , skipped(false) {}

    DriverTestResult()
        : total_tests(0)
        , passed_tests(0)
        , skipped(false) {}

    /// @brief Check if all tests passed
    bool allPassed() const { return !skipped && total_tests > 0 && passed_tests == total_tests; }

    /// @brief Check if any tests failed
    bool anyFailed() const { return !skipped && total_tests > 0 && passed_tests < total_tests; }
};

/// @brief Run a single stateless validation test
/// @param config Test configuration
/// @return Test result with pass/fail information
SingleTestResult runSingleValidationTest(const SingleTestConfig& config);

}  // namespace fl
