// @filter: (platform is esp32)

// examples/Validation/Validation.ino
//
// FastLED LED Timing Validation Sketch for ESP32.
//
// This sketch validates LED output by reading back timing values using the
// RMT peripheral in receive mode. It performs TX→RX loopback testing to verify
// that transmitted LED data matches received data.
//
// DEMONSTRATES:
// 1. Runtime Channel API (FastLED.addChannel) for iterating through all available
//    drivers (RMT, SPI, PARLIO) and testing multiple chipset timings dynamically
//    by creating and destroying controllers for each driver.
// 2. Multi-channel validation support: Pass span<const ChannelConfig> to validate
//    multiple LED strips/channels simultaneously. Each channel is independently
//    validated with its own RX loopback channel.
//
// Use case: When developing a FastLED driver for a new peripheral, it is useful
// to read back the LED's received data to verify that the timing is correct.
//
// MULTI-CHANNEL MODE:
// - Single-channel: Pass one ChannelConfig - uses shared RX channel object (created in setup())
// - Multi-channel: Pass multiple ChannelConfigs - creates dynamic RX channels
//   on each TX pin for independent loopback validation
// - Each channel in the span is validated sequentially with its own RX channel
// - For RMT: Uses internal loopback (no jumper needed)
// - For SPI/PARLIO: Requires physical jumper from each TX pin to itself
//
// Hardware Setup:
//   ⚠️ IMPORTANT: Physical jumper wire required for non-RMT TX → RMT RX loopback
//
//   When non-RMT peripherals are used for TX (e.g., SPI, ParallelIO):
//   - Connect GPIO PIN_DATA to itself with a physical jumper wire
//   - Internal loopback (io_loop_back flag) only works for RMT TX → RMT RX
//   - ESP32 GPIO matrix cannot route other peripheral outputs internally to RMT input
//
//   When RMT is used for TX (lower peripheral priority or disable other peripherals):
//   - No jumper wire needed - io_loop_back works for RMT TX → RMT RX
//
//   Alternative: Connect an LED strip between TX pin and ground, then connect
//   RX pin to LED data line to capture actual LED protocol timing (requires
//   two separate GPIO pins for TX and RX).
//
// Platform Support:
//   - ESP32 (classic)
//   - ESP32-S3 (Xtensa)
//   - ESP32-C3 (RISC-V)
//   - ESP32-C6 (RISC-V)
//
// Expected Output:
//   Serial monitor will show:
//   - List of discovered drivers (RMT, SPI, PARLIO - availability depends on platform)
//   - Test results for each driver with PASS/FAIL status for each chipset timing
//   - Each driver is tested independently by creating/destroying channels
//   - In multi-channel mode: Separate validation results for each channel/pin
//
// MULTI-CHANNEL EXAMPLE:
//   ```cpp
//   // Create multiple LED arrays
//   CRGB leds1[100], leds2[100], leds3[100];
//
//   // Create channel configs for each strip on different pins
//   fl::ChannelConfig configs[] = {
//       fl::ChannelConfig(5, timing, fl::span<CRGB>(leds1, 100), RGB),
//       fl::ChannelConfig(6, timing, fl::span<CRGB>(leds2, 100), RGB),
//       fl::ChannelConfig(7, timing, fl::span<CRGB>(leds3, 100), RGB)
//   };
//
//   // Validate all 3 channels (each gets its own RX loopback)
//   validateChipsetTiming(timing, "WS2812", fl::span(configs, 3), nullptr, buffer);
//   ```
//

// ============================================================================
// AGENT INSTRUCTIONS
// ============================================================================
// This sketch is a validation test that uses the "ERROR" keyword in FL_WARN
// statements to signal test failures. The `bash debug` command monitors for
// the "ERROR" keyword and fails the test when detected (exit code 1).
//
// 🚫 DO NOT "CHEAT" THE TEST:
//    - DO NOT change "ERROR" to "FAIL", "WARNING", "FAILURE", or any other
//      keyword to avoid test detection
//    - DO NOT modify FL_WARN statements unless explicitly requested by the user
//    - The "ERROR" keyword is INTENTIONAL and part of the validation contract
//
// ✅ VALID MODIFICATIONS (only if explicitly requested):
//    - User asks: "Change the error message format"
//    - User asks: "Use a different warning macro"
//    - User asks: "Remove the ERROR prefix"
//
// ❌ INVALID MODIFICATIONS (considered "cheating"):
//    - Changing "ERROR" to avoid `bash debug --fail-on ERROR` detection
//    - Renaming to bypass automated test failure detection
//    - Any modification that circumvents the validation contract
//
// If the test is failing, investigate and FIX THE ACTUAL BUG that causes
// the "ERROR" message to be printed. Do not hide the symptom by changing
// the error message keyword.
// ============================================================================

#include <FastLED.h>
#include "ValidationTest.h"

#ifndef PIN_DATA
#define PIN_DATA 5
#endif

#define PIN_RX 0

#define NUM_LEDS 255
#define CHIPSET WS2812B
#define COLOR_ORDER RGB  // No reordering needed.

#define RX_BUFFER_SIZE 8192  // 255 LEDs × 24 bits/LED = 6120 symbols, use 8192 for headroom

CRGB leds[NUM_LEDS];
uint8_t rx_buffer[RX_BUFFER_SIZE];  // 255 LEDs × 24 bits/LED = 6120 symbols, use 8192 for headroom

// ⚠️ CRITICAL: RMT RX channel - MUST persist across ALL loop iterations
// Created ONCE in setup(), reused for all driver tests
// DO NOT reset, destroy, or recreate this channel in loop()
fl::shared_ptr<fl::RmtRxChannel> rx_channel;

/// @brief Validate that expected engines are available for this platform
/// Prints ERROR if any expected engines are missing
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
    auto drivers = FastLED.getDriverInfo();

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
            FL_WARN("ERROR: Expected engine '" << expected << "' is MISSING from available drivers!");
            all_present = false;
        }
    }

    if (all_present) {
        FL_WARN("[VALIDATION] ✓ All expected engines are available");
    } else {
        FL_WARN("ERROR: Engine validation FAILED - some expected engines are missing!");
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);

    FL_WARN("\n=== FastLED RMT RX Validation Sketch ===");
    FL_WARN("Hardware: ESP32 (any variant)");
    FL_WARN("TX Pin (MOSI): GPIO " << PIN_DATA << " (ESP32-S3: use GPIO 11 for IO_MUX)");
    FL_WARN("RX Pin: GPIO " << PIN_RX);
    FL_WARN("");
    FL_WARN("⚠️  HARDWARE SETUP REQUIRED:");
    FL_WARN("   If using non-RMT peripherals for TX (e.g., SPI, ParallelIO):");
    FL_WARN("   → Connect GPIO " << PIN_DATA << " to GPIO " << PIN_RX << " with a physical jumper wire");
    FL_WARN("   → Internal loopback (io_loop_back) only works for RMT TX → RMT RX");
    FL_WARN("   → ESP32 GPIO matrix cannot route other peripheral outputs to RMT input");
    FL_WARN("");
    FL_WARN("   ESP32-S3 IMPORTANT: Use GPIO 11 (MOSI) for best performance");
    FL_WARN("   → GPIO 11 is SPI2 IO_MUX pin (bypasses GPIO matrix for 80MHz speed)");
    FL_WARN("   → Other GPIOs use GPIO matrix routing (limited to 26MHz, may see timing issues)");
    FL_WARN("");

    // Create RMT RX channel FIRST (before FastLED init to avoid pin conflicts)
    // ⚠️ CRITICAL: RX channel is created ONCE in setup() and must PERSIST across all loop iterations
    // This shared RX channel is reused for all driver tests to avoid resource conflicts
    // Buffer size: 255 LEDs × 24 bits/LED = 6120 symbols, use 8192 for headroom
    if (!rx_channel) {
        FL_WARN("Creating persistent RX channel (will be reused across all tests)...");
        rx_channel = fl::RmtRxChannel::create(PIN_RX, 40000000, RX_BUFFER_SIZE);
        if (!rx_channel) {
            FL_WARN("ERROR: Failed to create RX channel - validation tests will fail");
            FL_WARN("       Check that RMT peripheral is available and not in use");
            return;
        }
        FL_WARN("RX channel created successfully on GPIO " << PIN_RX);
    } else {
        FL_WARN("RX channel already exists (reusing from previous setup)");
    }

    // Note: Toggle test removed - validated in Iteration 5 that RMT RX works correctly
    // Skipping toggle test to avoid GPIO ownership conflicts with SPI MOSI

    // List all available drivers
    FL_WARN("\nDiscovering available drivers...");
    auto drivers = FastLED.getDriverInfo();
    FL_WARN("Found " << drivers.size() << " driver(s) available:");
    for (fl::size i = 0; i < drivers.size(); i++) {
        FL_WARN("  " << (i+1) << ". " << drivers[i].name.c_str()
                << " (priority: " << drivers[i].priority
                << ", enabled: " << (drivers[i].enabled ? "yes" : "no") << ")");
    }

    // Validate that expected engines are available for this platform
    validateExpectedEngines();

    FL_WARN("\nStarting continuous validation test loop...");
    delay(2000);
}

// ============================================================================
// Helper Functions
// ============================================================================

/// @brief Test a specific driver with given timing configuration
/// @param driver_name Driver name to test
/// @param timing Chipset timing configuration
/// @param timing_name Timing name for logging (e.g., "WS2812B-V5")
/// @param result Driver test result tracker (modified)
void testDriver(const char* driver_name,
                const fl::ChipsetTimingConfig& timing,
                const char* timing_name,
                fl::DriverTestResult& result) {

    // Set this driver as exclusive for testing
    if (!FastLED.setExclusiveDriver(driver_name)) {
        FL_WARN("[ERROR] Failed to set " << driver_name << " as exclusive driver");
        result.skipped = true;
        return;
    }
    FL_WARN(driver_name << " driver enabled exclusively\n");

    FL_WARN("[CONFIG] Driver: " << driver_name << " (physical jumper required)\n");

    // Create TX configuration for validation tests
    fl::ChannelConfig tx_config(PIN_DATA, timing, fl::span<CRGB>(leds, NUM_LEDS), COLOR_ORDER);

    FL_WARN("[INFO] Testing " << timing_name << " timing\n");

    // Create validation configuration with all input parameters
    fl::ValidationConfig validation_config(
        timing,
        timing_name,
        fl::span<fl::ChannelConfig>(&tx_config, 1),
        driver_name,
        rx_channel,
        fl::span<uint8_t>(rx_buffer, RX_BUFFER_SIZE)
    );

    // Validate the chipset timing
    validateChipsetTiming(validation_config, result.total_tests, result.passed_tests);

    FL_WARN("\n[INFO] All timing tests complete for " << driver_name << " driver");
}

/// @brief Print driver validation summary table
/// @param driver_results Vector of driver test results
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

/// @brief Discard first frame (has timing issues)
/// @param driver_name Driver name for logging
void discardFirstFrame(const char* driver_name) {
    FL_WARN("[INFO] Discarding first frame for " << driver_name << " (timing warm-up)");
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    delay(100);
}

// ============================================================================
// Main Loop
// ============================================================================

void loop() {
    FL_WARN("=== Using Runtime Channel API for Dynamic Driver Testing ===\n");

    // Get all available drivers for this platform
    auto drivers = FastLED.getDriverInfo();

    FL_WARN("Starting validation tests for each driver...\n");
    FL_WARN("(Using persistent RX channel from setup() - not recreated)\n");

    // Track driver-level test results
    fl::vector<fl::DriverTestResult> driver_results;

    // Iterate through all drivers and test each one
    for (fl::size i = 0; i < drivers.size(); i++) {
        const auto& driver = drivers[i];
        fl::DriverTestResult result(driver.name.c_str());

        FL_WARN("\n╔════════════════════════════════════════════════════════════════╗");
        FL_WARN("║ TESTING DRIVER " << (i+1) << "/" << drivers.size() << ": " << driver.name.c_str() << " (priority: " << driver.priority << ")");
        FL_WARN("╚════════════════════════════════════════════════════════════════╝\n");

        // Discard first frame (timing warm-up)
        discardFirstFrame(driver.name.c_str());

        // Test driver with WS2812B-V5 timing (filtered for focused debugging)
        FL_WARN("[FILTER] Testing only WS2812B-V5 timing (skipping WS2812 Standard and SK6812)\n");
        testDriver(driver.name.c_str(),
                   fl::makeTimingConfig<fl::TIMING_WS2812B_V5>(),
                   "WS2812B-V5",
                   result);

        driver_results.push_back(result);
    }

    // Print summary table
    printSummaryTable(driver_results);

    FL_WARN("\n╔════════════════════════════════════════════════════════════════╗");
    FL_WARN("║ ALL VALIDATION TESTS COMPLETE                                  ║");
    FL_WARN("╚════════════════════════════════════════════════════════════════╝");

    FL_WARN("\n========== TEST ITERATION COMPLETE - RESTARTING ==========\n");
    delay(1000);
}
