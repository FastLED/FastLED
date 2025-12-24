// ValidationHelpers.h - Helper functions for Validation.ino
// Contains utility functions for driver testing, RX channel setup, and result reporting

#pragma once

#include <FastLED.h>
#include "Common.h"
#include "ValidationTest.h"
#include "fl/rx_device.h"

/// @brief Test RX channel with manual GPIO toggle
/// @param rx_channel RX channel to test
/// @param pin_tx GPIO pin to toggle
/// @param pin_rx GPIO pin for RX
/// @param hz Sampling frequency in Hz
/// @param buffer_size Size of RX buffer in symbols
/// @return true if test passes, false otherwise
bool testRxChannel(
    fl::shared_ptr<fl::RxDevice> rx_channel,
    int pin_tx,
    int pin_rx,
    uint32_t hz,
    size_t buffer_size);

/// @brief Validate that expected engines are available for this platform
/// Prints ERROR if any expected engines are missing
void validateExpectedEngines();

/// @brief Test a specific driver with given timing configuration
/// @param driver_name Driver name to test
/// @param timing_config Named timing configuration (includes timing and name)
/// @param pin_data TX pin number
/// @param num_leds Number of LEDs
/// @param leds LED array
/// @param color_order Color order
/// @param rx_channel RX channel for loopback
/// @param rx_buffer RX buffer for capture
/// @param base_strip_size Base strip size for error reporting (usually same as num_leds)
/// @param rx_type RX device type for error reporting
/// @param result Driver test result tracker (modified)
/// @note Automatically discards first frame (timing warm-up) on first run
void testDriver(
    const char* driver_name,
    const fl::NamedTimingConfig& timing_config,
    int pin_data,
    size_t num_leds,
    CRGB* leds,
    EOrder color_order,
    fl::shared_ptr<fl::RxDevice> rx_channel,
    fl::span<uint8_t> rx_buffer,
    int base_strip_size,
    fl::RxDeviceType rx_type,
    fl::DriverTestResult& result);

/// @brief Print driver validation summary table
/// @param driver_results Vector of driver test results
void printSummaryTable(const fl::vector<fl::DriverTestResult>& driver_results);

/// @brief Build test matrix configuration from preprocessor defines and available drivers
/// @param drivers_available List of available drivers from FastLED.getDriverInfos()
/// @return TestMatrixConfig with filtered drivers and configured dimensions
fl::TestMatrixConfig buildTestMatrix(const fl::vector<fl::DriverInfo>& drivers_available);

/// @brief Generate all test cases from the test matrix configuration
/// @param matrix_config Test matrix configuration
/// @param pin_tx Base TX pin for lane 0 (consecutive pins for multi-lane)
/// @return Vector of all test case configurations to run
fl::vector<fl::TestCaseConfig> generateTestCases(const fl::TestMatrixConfig& matrix_config, int pin_tx);

/// @brief Print test matrix summary (drivers, lanes, strip sizes, total cases)
/// @param matrix_config Test matrix configuration
void printTestMatrixSummary(const fl::TestMatrixConfig& matrix_config);

/// @brief Print test case results summary table
/// @param test_results Vector of test case results
void printTestCaseResultsTable(const fl::vector<fl::TestCaseResult>& test_results);

/// @brief Print final validation result banner (large, prominent PASS/FAIL indicator)
/// @param test_results Vector of test case results
void printFinalResultBanner(const fl::vector<fl::TestCaseResult>& test_results);
