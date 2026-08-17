// src/fl/channels/validation.h
//
// Validation test logic - stateless single-test execution
// Extracted from examples/validation for unit testing

#pragma once

#include "fl/stl/vector.h"
#include "fl/stl/string.h"
#include "fl/stl/optional.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "fl/channels/rx/types.h"

// Forward declarations for detail modules
namespace fl {
namespace validation {
    class RxTest;  // IWYU pragma: keep
    class ResultFormatter;  // IWYU pragma: keep
    class Platform;  // IWYU pragma: keep
}
}

namespace fl {

namespace validation {

/// @brief Whether an RMT validation capture should use ESP-IDF loopback.
///
/// ESP-IDF's internal RMT path only connects TX and RX channels configured on
/// the same GPIO. Distinct pins require a physical jumper and must leave the
/// RX channel attached to its external GPIO input.
inline bool useRmtInternalLoopback(bool is_rmt_driver, int tx_pin,
                                   int rx_pin) FL_NO_EXCEPT {
    return is_rmt_driver && tx_pin == rx_pin;
}

/// @brief Select the independent capture backend used by AutoResearch.
///
/// ESP32-C6 RMT RX does not preserve PARLIO's sub-microsecond low phases even
/// though the same pad's GPIO input sees them. Use the GPIO ISR timestamp
/// backend for the default C6 PARLIO validation path. An explicit caller
/// override remains authoritative for diagnostics.
inline RxBackend resolveCaptureBackend(RxBackend requested_backend,
                                       bool has_explicit_override,
                                       bool is_parlio_driver,
                                       bool is_esp32_c6) FL_NO_EXCEPT {
    if (!has_explicit_override && is_parlio_driver && is_esp32_c6) {
        return RxBackend::ISR;
    }
    return requested_backend;
}

/// @brief Compute the RX edge-buffer capacity for a validation frame.
///
/// GPIO ISR RX requires a power-of-two circular buffer. Size that buffer from
/// the frame under test (two edges per bit plus framing headroom), rather than
/// from AutoResearch's oversized shared byte buffer.
inline size_t captureEdgeCapacity(size_t shared_buffer_bytes,
                                  size_t expected_data_bytes,
                                  RxBackend backend) FL_NO_EXCEPT {
    constexpr size_t kEdgesPerByte = 16;
    constexpr size_t kFramingEdges = 2;
    const size_t max_size = static_cast<size_t>(-1);

    if (backend != RxBackend::ISR) {
        if (shared_buffer_bytes > max_size / 8) {
            return 0;
        }
        return shared_buffer_bytes * 8;
    }

    if (expected_data_bytes > (max_size - kFramingEdges) / kEdgesPerByte) {
        return 0;
    }
    const size_t required =
        expected_data_bytes * kEdgesPerByte + kFramingEdges;
    size_t capacity = 1;
    while (capacity < required) {
        if (capacity > max_size / 2) {
            return 0;
        }
        capacity *= 2;
    }
    return capacity;
}

}  // namespace validation

/// @brief Single test configuration - fully stateless
struct SingleTestConfig {
    string driver_name;           ///< Driver to test (e.g., "PARLIO", "RMT")
    vector<int> lane_sizes;       ///< LED count per lane [100, 100, 200]
    string pattern;               ///< Test pattern name (default: "MSB_LSB_A")
    int iterations;               ///< Number of test iterations (default: 1)
    int pin_tx;                   ///< TX pin (base pin for multi-lane)
    int pin_rx;                   ///< RX pin

    SingleTestConfig() FL_NO_EXCEPT
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

    SingleTestResult() FL_NO_EXCEPT
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

    DriverTestResult() FL_NO_EXCEPT
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
