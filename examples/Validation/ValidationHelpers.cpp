// ValidationHelpers.cpp - Helper function implementations

#include "ValidationConfig.h"  // Must be included BEFORE ValidationHelpers.h to set config macros
#include "ValidationHelpers.h"
#include "fl/stl/sstream.h"
#include "fl/pin.h"  // Platform-independent pin API

bool testRxChannel(
    fl::shared_ptr<fl::RxDevice> rx_channel,
    int pin_tx,
    int pin_rx,
    uint32_t hz,
    size_t buffer_size) {
    FL_WARN("[RX TEST] Testing RX channel with manual GPIO toggle on PIN " << pin_tx);

    // Configure PIN_TX as output using fl::pin functions (temporarily take ownership from FastLED)
    fl::pinMode(pin_tx, fl::PinMode::Output);
    fl::digitalWrite(pin_tx, fl::PinValue::Low);  // Start LOW

    // Generate fast toggles (RMT max signal_range is ~819 μs, so use 100 μs pulses)
    const int num_toggles = 10;
    const int toggle_delay_us = 100;  // 100 μs pulses = 5 kHz square wave

    // Initialize RX channel with signal range for fast GPIO toggles
    // RMT peripheral max is ~819 μs, so use 200 μs (2x our pulse width for safety)
    fl::RxConfig rx_config;
    rx_config.buffer_size = buffer_size;
    rx_config.hz = hz;
    rx_config.signal_range_min_ns = 100;    // min=100ns
    rx_config.signal_range_max_ns = 200000;  // max=200μs
    rx_config.start_low = true;

    if (!rx_channel->begin(rx_config)) {
        FL_ERROR("[RX TEST]: Failed to begin RX channel");
        fl::pinMode(pin_tx, fl::PinMode::Input);  // Release pin
        return false;
    }
    delayMicroseconds(50);  // Let RX stabilize

    // Generate toggle pattern: HIGH -> LOW -> HIGH -> LOW ...
    for (int i = 0; i < num_toggles; i++) {
        fl::digitalWrite(pin_tx, fl::PinValue::High);
        delayMicroseconds(toggle_delay_us);
        fl::digitalWrite(pin_tx, fl::PinValue::Low);
        delayMicroseconds(toggle_delay_us);
    }

    // Wait for RX to finish capturing (timeout = total toggle time + headroom)
    uint32_t timeout_ms = 100;  // 10 toggles * 200μs = 2ms, use 100ms for safety
    fl::RxWaitResult wait_result = rx_channel->wait(timeout_ms);

    // Release PIN_TX for FastLED use using fl::pin functions
    fl::pinMode(pin_tx, fl::PinMode::Input);

    // Check if we successfully captured data
    if (wait_result != fl::RxWaitResult::SUCCESS) {
        FL_ERROR("[RX TEST]: RX channel wait failed (result: " << static_cast<int>(wait_result) << ")");
        FL_ERROR("[RX TEST]: RX may not be working - check PIN_RX (" << pin_rx << ") and RMT peripheral");
        FL_ERROR("[RX TEST]: If using non-RMT TX, ensure physical jumper from PIN " << pin_tx << " to PIN " << pin_rx);
        return false;
    }

    FL_WARN("[RX TEST] ✓ RX channel captured data from " << num_toggles << " toggles");
    FL_WARN("[RX TEST] ✓ RX channel is functioning correctly");

    return true;
}


/// NOTE TO AI: TODO Fix this function
/// FL_IS_* macros are probably not defined correctly because i don't think FastLED.h
/// is being included. Please check for
///   [VALIDATION] Platform: Unknown ESP32 variant - skipping engine validation
/// please fix this sometime in the future

void validateExpectedEngines() {
    // Define expected engines based on platform
    fl::vector<const char*> expected_engines;

#if defined(FL_IS_ESP_32C6)
    // ESP32-C6 should have: PARLIO, RMT (SPI disabled - only 1 host, RMT5 preferred)
    expected_engines.push_back("PARLIO");
    expected_engines.push_back("RMT");
    FL_WARN("\n[VALIDATION] Platform: ESP32-C6");
#elif defined(FL_IS_ESP_32S3)
    // ESP32-S3 should have: SPI, RMT, I2S (I2S uses LCD_CAM peripheral)
    expected_engines.push_back("SPI");
    expected_engines.push_back("RMT");
    expected_engines.push_back("I2S");
    FL_WARN("\n[VALIDATION] Platform: ESP32-S3");
#elif defined(FL_IS_ESP_32C3)
    // ESP32-C3 should have: SPI, RMT (no PARLIO)
    expected_engines.push_back("RMT");
    FL_WARN("\n[VALIDATION] Platform: ESP32-C3");
#elif defined(FL_IS_ESP_32DEV)
    // Original ESP32 should have: SPI, RMT, I2S (no PARLIO)
    expected_engines.push_back("SPI");
    expected_engines.push_back("RMT");
    // expected_engines.push_back("I2S");
    FL_WARN("\n[VALIDATION] Platform: ESP32 (classic)");
#else
    FL_WARN("\n[VALIDATION] Platform: Unknown ESP32 variant - skipping engine validation");
    return;
#endif

    fl::sstream ss;
    ss << "[VALIDATION] Expected engines: " << expected_engines.size() << "\n";
    for (fl::size i = 0; i < expected_engines.size(); i++) {
        ss << "  - " << expected_engines[i] << "\n";
    }
    FL_WARN(ss.str());

    // Get available drivers
    auto drivers = FastLED.getDriverInfos();

    // Check if all expected engines are available
    bool all_present = true;
    for (fl::size i = 0; i < expected_engines.size(); i++) {
        const char* expected = expected_engines[i];
        bool found = false;

        for (fl::size j = 0; j < drivers.size(); j++) {
            if (fl::strcmp(drivers[j].name.c_str(), expected) == 0) {
                found = true;
                break;
            }
        }

        if (!found) {
            FL_ERROR("Expected engine '" << expected << "' is MISSING from available drivers!");
            all_present = false;
        }
    }

    if (all_present) {
        FL_WARN("[VALIDATION] ✓ All expected engines are available");
    } else {
        FL_ERROR("Engine validation FAILED - some expected engines are missing!");
    }
}

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
    fl::DriverTestResult& result) {

    // Set this driver as exclusive for testing
    if (!FastLED.setExclusiveDriver(driver_name)) {
        FL_ERROR("Failed to set " << driver_name << " as exclusive driver");
        result.skipped = true;
        return;
    }
    FL_WARN(driver_name << " driver enabled exclusively\n");

    FL_WARN("[CONFIG] Driver: " << driver_name << " (physical jumper required)\n");

    // Create TX configuration for validation tests
    fl::ChannelConfig tx_config(pin_data, timing_config.timing, fl::span<CRGB>(leds, num_leds), color_order);

    FL_WARN("[INFO] Testing " << timing_config.name << " timing\n");

    // Create validation configuration with all input parameters
    fl::ValidationConfig validation_config(
        timing_config.timing,
        timing_config.name,
        fl::span<fl::ChannelConfig>(&tx_config, 1),
        driver_name,
        rx_channel,
        rx_buffer,
        base_strip_size,
        rx_type
    );

    // FIRST RUN: Discard results (timing warm-up)
    // TX channel construction may have extra latency on first run
    FL_WARN("[INFO] Running warm-up frame (results will be discarded)");
    int warmup_total = 0, warmup_passed = 0;
    validateChipsetTiming(validation_config, warmup_total, warmup_passed);
    FL_WARN("[INFO] Warm-up complete (" << warmup_passed << "/" << warmup_total << " passed - discarding)");

    // SECOND RUN: Keep results (actual test)
    FL_WARN("[INFO] Running actual test frame");
    validateChipsetTiming(validation_config, result.total_tests, result.passed_tests);

    FL_WARN("\n[INFO] All timing tests complete for " << driver_name << " driver");
}

void printSummaryTable(const fl::vector<fl::DriverTestResult>& driver_results) {
    fl::sstream ss;
    ss << "\n╔════════════════════════════════════════════════════════════════╗\n";
    ss << "║ DRIVER VALIDATION SUMMARY                                      ║\n";
    ss << "╠════════════════════════════════════════════════════════════════╣\n";
    ss << "║ Driver       │ Status      │ Tests Passed │ Total Tests       ║\n";
    ss << "╠══════════════╪═════════════╪══════════════╪═══════════════════╣";
    FL_WARN(ss.str());

    for (fl::size i = 0; i < driver_results.size(); i++) {
        const auto& result = driver_results[i];
        const char* status;
        if (result.skipped) {
            status = "SKIPPED    ";
        } else if (result.allPassed()) {
            status = "PASS ✓     ";
        } else if (result.anyFailed()) {
            status = "FAIL ✗     ";
        } else {
            status = "NO TESTS   ";
        }

        // Build table row using sstream
        fl::sstream row;
        row << "║ ";

        // Driver name (12 chars, left-aligned)
        fl::string driver_name = result.driver_name;
        if (driver_name.length() > 12) {
            driver_name = driver_name.substr(0, 12);
        }
        row << driver_name;
        for (size_t i = driver_name.length(); i < 12; i++) {
            row << " ";
        }
        row << " │ " << status << " │ ";

        // Tests passed (12 chars, left-aligned)
        if (result.skipped) {
            row << "-";
            for (int i = 1; i < 12; i++) row << " ";
        } else {
            fl::string passed = fl::to_string(result.passed_tests);
            row << passed;
            for (size_t i = passed.length(); i < 12; i++) row << " ";
        }
        row << " │ ";

        // Total tests (17 chars, left-aligned)
        if (result.skipped) {
            row << "-";
            for (int i = 1; i < 17; i++) row << " ";
        } else {
            fl::string total = fl::to_string(result.total_tests);
            row << total;
            for (size_t i = total.length(); i < 17; i++) row << " ";
        }
        row << " ║";

        FL_WARN(row.str().c_str());
    }

    FL_WARN("╚══════════════╧═════════════╧══════════════╧═══════════════════╝");
}

// Build test matrix configuration from preprocessor defines and available drivers
fl::TestMatrixConfig buildTestMatrix(const fl::vector<fl::DriverInfo>& drivers_available) {
    fl::TestMatrixConfig matrix;

    // Filter drivers based on JUST_* defines
    for (fl::size i = 0; i < drivers_available.size(); i++) {
        const char* driver_name = drivers_available[i].name.c_str();
        bool include = false;

        #if defined(JUST_PARLIO)
            if (fl::strcmp(driver_name, "PARLIO") == 0) include = true;
        #elif defined(JUST_RMT)
            if (fl::strcmp(driver_name, "RMT") == 0) include = true;
        #elif defined(JUST_SPI)
            if (fl::strcmp(driver_name, "SPI") == 0) include = true;
        #elif defined(JUST_UART)
            if (fl::strcmp(driver_name, "UART") == 0) include = true;
        #elif defined(JUST_I2S)
            if (fl::strcmp(driver_name, "I2S") == 0) include = true;
        #else
            // No filter - include all drivers
            include = true;
        #endif

        if (include) {
            matrix.enabled_drivers.push_back(fl::string(driver_name));
        }
    }

    // Set lane range from defines
    matrix.min_lanes = MIN_LANES;
    matrix.max_lanes = MAX_LANES;

    // Set strip size flags from defines
    #if defined(JUST_SMALL_STRIPS) && !defined(JUST_LARGE_STRIPS)
        matrix.test_small_strips = true;
        matrix.test_large_strips = false;
    #elif defined(JUST_LARGE_STRIPS) && !defined(JUST_SMALL_STRIPS)
        matrix.test_small_strips = false;
        matrix.test_large_strips = true;
    #else
        // Default: test both sizes
        matrix.test_small_strips = true;
        matrix.test_large_strips = true;
    #endif

    return matrix;
}

// Generate all test cases from the test matrix configuration
fl::vector<fl::TestCaseConfig> generateTestCases(const fl::TestMatrixConfig& matrix_config, int pin_tx) {
    fl::vector<fl::TestCaseConfig> test_cases;

    // NEW: If lane_sizes is explicitly set, use variable lane sizing
    if (!matrix_config.lane_sizes.empty()) {
        // Generate one test case per driver with the specified lane sizes
        for (fl::size driver_idx = 0; driver_idx < matrix_config.enabled_drivers.size(); driver_idx++) {
            const char* driver_name = matrix_config.enabled_drivers[driver_idx].c_str();

            // Create test case with variable lane sizes
            fl::TestCaseConfig test_case(driver_name, matrix_config.lane_sizes, pin_tx);

            test_cases.push_back(test_case);
        }

        return test_cases;
    }

    // LEGACY: Uniform sizing - iterate through all combinations: drivers × lane counts × strip sizes
    for (fl::size driver_idx = 0; driver_idx < matrix_config.enabled_drivers.size(); driver_idx++) {
        const char* driver_name = matrix_config.enabled_drivers[driver_idx].c_str();

        for (int lane_count = matrix_config.min_lanes; lane_count <= matrix_config.max_lanes; lane_count++) {
            // Generate test cases for enabled strip sizes
            if (matrix_config.test_small_strips) {
                fl::TestCaseConfig test_case(driver_name, lane_count, SHORT_STRIP_SIZE);
                // Assign consecutive GPIO pins for multi-lane
                for (int lane_idx = 0; lane_idx < lane_count; lane_idx++) {
                    test_case.lanes[lane_idx].pin = pin_tx + lane_idx;
                }
                test_cases.push_back(test_case);
            }

            if (matrix_config.test_large_strips) {
                fl::TestCaseConfig test_case(driver_name, lane_count, LONG_STRIP_SIZE);
                // Assign consecutive GPIO pins for multi-lane
                for (int lane_idx = 0; lane_idx < lane_count; lane_idx++) {
                    test_case.lanes[lane_idx].pin = pin_tx + lane_idx;
                }
                test_cases.push_back(test_case);
            }
        }
    }

    return test_cases;
}

// Print test matrix summary (drivers, lanes, strip sizes, total cases)
void printTestMatrixSummary(const fl::TestMatrixConfig& matrix_config) {
    fl::sstream ss;
    ss << "\n╔════════════════════════════════════════════════════════════════╗\n";
    ss << "║ TEST MATRIX CONFIGURATION                                      ║\n";
    ss << "╚════════════════════════════════════════════════════════════════╝\n";

    // Drivers
    ss << "Drivers (" << matrix_config.enabled_drivers.size() << "):\n";
    for (fl::size i = 0; i < matrix_config.enabled_drivers.size(); i++) {
        ss << "  - " << matrix_config.enabled_drivers[i].c_str() << "\n";
    }
    FL_WARN(ss.str());

    // Lane range
    int lane_range = matrix_config.max_lanes - matrix_config.min_lanes + 1;
    FL_WARN("Lane Range: " << matrix_config.min_lanes << "-" << matrix_config.max_lanes
            << " (" << lane_range << " configurations)");

    // Strip sizes
    fl::sstream strip_info;
    if (matrix_config.test_small_strips && matrix_config.test_large_strips) {
        strip_info << "Both (Short=" << SHORT_STRIP_SIZE << ", Long=" << LONG_STRIP_SIZE << ")";
    } else if (matrix_config.test_small_strips) {
        strip_info << "Short only (" << SHORT_STRIP_SIZE << " LEDs)";
    } else if (matrix_config.test_large_strips) {
        strip_info << "Long only (" << LONG_STRIP_SIZE << " LEDs)";
    } else {
        strip_info << "None (ERROR)";
    }
    FL_WARN("Strip Sizes: " << strip_info.str().c_str());

    // Total test cases
    int total_cases = matrix_config.getTotalTestCases();
    FL_WARN("Total Test Cases: " << total_cases);

    FL_WARN("");
}

// Print test case results summary table
void printTestCaseResultsTable(const fl::vector<fl::TestCaseResult>& test_results) {
    fl::sstream ss;
    ss << "\n╔════════════════════════════════════════════════════════════════════════════╗\n";
    ss << "║ TEST MATRIX RESULTS SUMMARY                                                ║\n";
    ss << "╠════════════════════════════════════════════════════════════════════════════╣\n";
    ss << "║ Driver  │ Lanes │ Strip │ Status     │ Tests Passed │ Total Tests        ║\n";
    ss << "╠═════════╪═══════╪═══════╪════════════╪══════════════╪════════════════════╣";
    FL_WARN(ss.str());

    int total_passed = 0;
    int total_tests = 0;

    for (fl::size i = 0; i < test_results.size(); i++) {
        const auto& result = test_results[i];
        const char* status;
        if (result.skipped) {
            status = "SKIP      ";
        } else if (result.allPassed()) {
            status = "PASS ✓    ";
        } else if (result.anyFailed()) {
            status = "FAIL ✗    ";
        } else {
            status = "NO TESTS  ";
        }

        // Build table row using sstream
        fl::sstream row;
        row << "║ ";

        // Driver name (7 chars, left-aligned)
        fl::string driver_name = result.driver_name;
        if (driver_name.length() > 7) {
            driver_name = driver_name.substr(0, 7);
        }
        row << driver_name;
        for (size_t j = driver_name.length(); j < 7; j++) {
            row << " ";
        }
        row << " │ ";

        // Lane count (5 chars, right-aligned)
        fl::string lanes = fl::to_string(result.lane_count);
        for (size_t j = lanes.length(); j < 5; j++) {
            row << " ";
        }
        row << lanes << " │ ";

        // Strip size (5 chars, right-aligned)
        fl::string strip_size = fl::to_string(result.base_strip_size);
        for (size_t j = strip_size.length(); j < 5; j++) {
            row << " ";
        }
        row << strip_size << " │ ";

        // Status (10 chars)
        row << status << " │ ";

        // Tests passed (12 chars, left-aligned)
        if (result.skipped) {
            row << "-";
            for (int j = 1; j < 12; j++) row << " ";
        } else {
            fl::string passed = fl::to_string(result.passed_tests);
            row << passed;
            for (size_t j = passed.length(); j < 12; j++) row << " ";
            total_passed += result.passed_tests;
        }
        row << " │ ";

        // Total tests (18 chars, left-aligned)
        if (result.skipped) {
            row << "-";
            for (int j = 1; j < 18; j++) row << " ";
        } else {
            fl::string total = fl::to_string(result.total_tests);
            row << total;
            for (size_t j = total.length(); j < 18; j++) row << " ";
            total_tests += result.total_tests;
        }
        row << " ║";

        FL_WARN(row.str().c_str());
    }

    FL_WARN("╠═════════╧═══════╧═══════╧════════════╧══════════════╧════════════════════╣");

    // Overall summary
    fl::sstream summary;
    summary << "║ OVERALL: ";
    if (total_tests > 0) {
        summary << total_passed << "/" << total_tests << " tests passed";
        double pass_rate = 100.0 * total_passed / total_tests;
        summary << " (" << static_cast<int>(pass_rate) << "%)";

        // Pad to 68 chars
        fl::string summary_str = summary.str();
        while (summary_str.length() < 68) {
            summary_str += " ";
        }
        summary_str += "║";
        FL_WARN(summary_str.c_str());
    } else {
        FL_WARN("║ OVERALL: No tests run                                                     ║");
    }

    FL_WARN("╚════════════════════════════════════════════════════════════════════════════╝");
}

// Print final validation result banner (large, prominent PASS/FAIL indicator)
void printFinalResultBanner(const fl::vector<fl::TestCaseResult>& test_results) {
    // Calculate overall statistics
    int total_passed = 0;
    int total_tests = 0;
    int failed_cases = 0;
    int passed_cases = 0;
    int skipped_cases = 0;

    for (fl::size i = 0; i < test_results.size(); i++) {
        const auto& result = test_results[i];
        if (!result.skipped) {
            total_passed += result.passed_tests;
            total_tests += result.total_tests;

            if (result.anyFailed()) {
                failed_cases++;
            } else if (result.allPassed()) {
                passed_cases++;
            }
        } else {
            skipped_cases++;
        }
    }

    bool all_passed = (failed_cases == 0 && total_tests > 0 && skipped_cases == 0);
    double pass_rate = (total_tests > 0) ? (100.0 * total_passed / total_tests) : 0.0;

    // Print the banner
    fl::sstream ss;
    ss << "\n╔════════════════════════════════════════════════════════════════════════════╗\n";
    ss << "║                                                                            ║\n";

    if (all_passed) {
        ss << "║                         ✓✓✓ VALIDATION PASSED ✓✓✓                         ║\n";
    } else {
        ss << "║                         ✗✗✗ VALIDATION FAILED ✗✗✗                         ║\n";
    }

    ss << "║                                                                            ║\n";

    // Summary statistics
    if (all_passed) {
        fl::sstream line;
        line << "║  " << passed_cases << " test case(s) PASSED";
        fl::string line_str = line.str();
        while (line_str.length() < 76) {
            line_str += " ";
        }
        line_str += "║\n";
        ss << line_str;
    } else {
        // Failed cases
        fl::sstream line;
        line << "║  " << failed_cases << " test case(s) FAILED";
        if (passed_cases > 0) {
            line << ", " << passed_cases << " passed";
        }
        if (skipped_cases > 0) {
            line << ", " << skipped_cases << " skipped";
        }
        fl::string line_str = line.str();
        while (line_str.length() < 76) {
            line_str += " ";
        }
        line_str += "║\n";
        ss << line_str;
    }

    // Total tests statistics
    fl::sstream line2;
    line2 << "║  " << total_passed << " out of " << total_tests << " validation tests passed (" << static_cast<int>(pass_rate) << "%)";
    fl::string line2_str = line2.str();
    while (line2_str.length() < 76) {
        line2_str += " ";
    }
    line2_str += "║\n";
    ss << line2_str;

    if (!all_passed && total_tests > 0) {
        ss << "║                                                                            ║\n";
        ss << "║  See detailed results table above for specifics                            ║\n";
    }

    ss << "║                                                                            ║\n";
    ss << "╚════════════════════════════════════════════════════════════════════════════╝";

    // Use FL_WARN for passed tests, FL_ERROR for failures (to trigger --fail-on ERROR detection)
    if (all_passed) {
        FL_WARN(ss.str());
    } else {
        // Print the banner as an error so it's clearly visible
        FL_ERROR(ss.str());
    }
}
