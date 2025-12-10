// ValidationHelpers.cpp - Helper function implementations

#include "ValidationHelpers.h"

bool testRxChannel(
    fl::shared_ptr<fl::RxDevice> rx_channel,
    int pin_tx,
    int pin_rx,
    uint32_t hz,
    size_t buffer_size) {
    FL_WARN("[RX TEST] Testing RX channel with manual GPIO toggle on PIN " << pin_tx);

    // Configure PIN_TX as output (temporarily take ownership from FastLED)
    pinMode(pin_tx, OUTPUT);
    digitalWrite(pin_tx, LOW);  // Start LOW

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
        pinMode(pin_tx, INPUT);  // Release pin
        return false;
    }
    delayMicroseconds(50);  // Let RX stabilize

    // Generate toggle pattern: HIGH -> LOW -> HIGH -> LOW ...
    for (int i = 0; i < num_toggles; i++) {
        digitalWrite(pin_tx, HIGH);
        delayMicroseconds(toggle_delay_us);
        digitalWrite(pin_tx, LOW);
        delayMicroseconds(toggle_delay_us);
    }

    // Wait for RX to finish capturing (timeout = total toggle time + headroom)
    uint32_t timeout_ms = 100;  // 10 toggles * 200μs = 2ms, use 100ms for safety
    fl::RxWaitResult wait_result = rx_channel->wait(timeout_ms);

    // Release PIN_TX for FastLED use
    pinMode(pin_tx, INPUT);

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

void validateExpectedEngines() {
    // Define expected engines based on platform
    fl::vector<const char*> expected_engines;

#if defined(FL_IS_ESP_32C6)
    // ESP32-C6 should have: SPI, PARLIO, RMT
    expected_engines.push_back("SPI");
    expected_engines.push_back("PARLIO");
    expected_engines.push_back("RMT");
    FL_WARN("\n[VALIDATION] Platform: ESP32-C6");
#elif defined(FL_IS_ESP_32S3)
    // ESP32-S3 should have: SPI, PARLIO, RMT
    expected_engines.push_back("SPI");
    expected_engines.push_back("RMT");
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

    FL_WARN("[VALIDATION] Expected engines: " << expected_engines.size());
    for (fl::size i = 0; i < expected_engines.size(); i++) {
        FL_WARN("  - " << expected_engines[i]);
    }

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
        rx_buffer
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
    FL_WARN("\n╔════════════════════════════════════════════════════════════════╗");
    FL_WARN("║ DRIVER VALIDATION SUMMARY                                      ║");
    FL_WARN("╠════════════════════════════════════════════════════════════════╣");
    FL_WARN("║ Driver       │ Status      │ Tests Passed │ Total Tests       ║");
    FL_WARN("╠══════════════╪═════════════╪══════════════╪═══════════════════╣");

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
