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
//   - Connect GPIO PIN_TX to itself with a physical jumper wire
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
#include "Common.h"
#include "ValidationTest.h"
#include "ValidationHelpers.h"

// ============================================================================
// Configuration
// ============================================================================

const fl::RxDeviceType RX_TYPE = fl::RxDeviceType::RMT;

#define PIN_TX 0
#define PIN_RX 1

#define NUM_LEDS 255
#define CHIPSET WS2812B
#define COLOR_ORDER RGB  // No reordering needed.

#define RX_BUFFER_SIZE 8192  // 255 LEDs × 24 bits/LED = 6120 symbols, use 8192 for headroom

// ============================================================================
// Engine Filtering (Debug/Development)
// ============================================================================
// Uncomment to test ONLY a specific engine (reduces console spam)
// Valid values: "RMT", "SPI", "PARLIO", "I2S"
#define TEST_ONLY_ENGINE "RMT"
// #define TEST_ONLY_ENGINE "PARLIO"
// #define TEST_ONLY_ENGINE "SPI"

CRGB leds[NUM_LEDS];
uint8_t rx_buffer[RX_BUFFER_SIZE];  // 255 LEDs × 24 bits/LED = 6120 symbols, use 8192 for headroom

// ⚠️ CRITICAL: RMT RX channel - MUST persist across ALL loop iterations
// Created ONCE in setup(), reused for all driver tests
// DO NOT reset, destroy, or recreate this channel in loop()
fl::shared_ptr<fl::RxDevice> rx_channel;

// ============================================================================
// Global Error Tracking
// ============================================================================

// Global error tracking flags

// Sanity check failure - if true, print error and delay in loop()
bool error_sanity_check = false;


// ============================================================================
// Global Driver State
// ============================================================================

// Available drivers discovered in setup()
fl::vector<fl::DriverInfo> drivers_available;

// Failed drivers - track driver names, failure reasons, and frame number
fl::vector<fl::DriverFailureInfo> drivers_failed;

// Frame counter - tracks which iteration of loop() we're on
uint32_t frame_counter = 0;


void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);

    FL_WARN("\n=== FastLED RMT RX Validation Sketch ===");
    FL_WARN("Hardware: ESP32 (any variant)");
    FL_WARN("TX Pin (MOSI): GPIO " << PIN_TX << " (ESP32-S3: use GPIO 11 for IO_MUX)");
    FL_WARN("RX Pin: GPIO " << PIN_RX);
    FL_WARN("");
    FL_WARN("⚠️  HARDWARE SETUP REQUIRED:");
    FL_WARN("   If using non-RMT peripherals for TX (e.g., SPI, ParallelIO):");
    FL_WARN("   → Connect GPIO " << PIN_TX << " to GPIO " << PIN_RX << " with a physical jumper wire");
    FL_WARN("   → ESP32 GPIO matrix cannot route other peripheral outputs to RMT input");
    FL_WARN("");
    FL_WARN("   ESP32-S3 IMPORTANT: Use GPIO 11 (MOSI) for best performance");
    FL_WARN("   → GPIO 11 is SPI2 IO_MUX pin (bypasses GPIO matrix for 80MHz speed)");
    FL_WARN("   → Other GPIOs use GPIO matrix routing (limited to 26MHz, may see timing issues)");
    FL_WARN("");

    // ========================================================================
    // RX Channel Setup with Sanity Check
    // ========================================================================
    // ⚠️ CRITICAL: Must validate RX works before main tests run

    FL_WARN("\n[RX SETUP] Starting RX channel setup and validation");

    // Step 1: Create sanity check RX channel (lower precision for GPIO toggles)
    FL_WARN("\n[RX SETUP] Step 1: Creating sanity check RX channel");
    FL_WARN("[RX CREATE] Creating RX channel on PIN " << PIN_RX
            << " (" << (20000000 / 1000000) << "MHz, " << 256 << " symbols)");

    auto sanity_rx_channel = fl::RxDevice::create<RX_TYPE>(PIN_RX);

    if (!sanity_rx_channel) {
        FL_ERROR("[RX SETUP]: Failed to create sanity check RX channel");
        FL_ERROR("[RX SETUP]: Check that RMT peripheral is available and not in use");
        error_sanity_check = true;
        return;
    }

    FL_WARN("[RX CREATE] ✓ RX channel created successfully (will be initialized with config in begin())");
    FL_WARN("[RX CREATE] Hardware params: pin=" << PIN_RX << ", hz=" << 20000000 << ", buffer_size=" << 256);

    // Step 2: Test the sanity check RX channel
    FL_WARN("\n[RX SETUP] Step 2: Running sanity check test");
    if (!testRxChannel(sanity_rx_channel, PIN_TX, PIN_RX, 20000000, 256)) {  // 20MHz, 256 symbols
        FL_ERROR("[RX SETUP]: Sanity check FAILED - RX channel is not capturing data");
        FL_ERROR("[RX SETUP]: Main validation tests will be skipped");
        error_sanity_check = true;
        return;
    }

    FL_WARN("[RX SETUP] ✓ Sanity check PASSED");

    // Step 3: Reset sanity check RX channel (release resources)
    FL_WARN("\n[RX SETUP] Step 3: Releasing sanity check RX channel");
    sanity_rx_channel.reset();
    FL_WARN("[RX SETUP] ✓ Sanity check RX channel destroyed");

    // Step 4: Create high-precision RX channel for main LED validation
    FL_WARN("\n[RX SETUP] Step 4: Creating high-precision RX channel for LED validation");
    FL_WARN("[RX CREATE] Creating RX channel on PIN " << PIN_RX
            << " (" << (40000000 / 1000000) << "MHz, " << RX_BUFFER_SIZE << " symbols)");

    rx_channel = fl::RxDevice::create<RX_TYPE>(PIN_RX);

    if (!rx_channel) {
        FL_ERROR("[RX SETUP]: Failed to create high-precision RX channel");
        FL_ERROR("[RX SETUP]: Check that RMT peripheral is available and not in use");
        error_sanity_check = true;
        return;
    }

    FL_WARN("[RX CREATE] ✓ RX channel created successfully (will be initialized with config in begin())");
    FL_WARN("[RX CREATE] Hardware params: pin=" << PIN_RX << ", hz=" << 40000000 << ", buffer_size=" << RX_BUFFER_SIZE);
    FL_WARN("[RX SETUP] ✓ RX channel is ready for LED validation\n");

    // List all available drivers and store globally
    FL_WARN("\nDiscovering available drivers...");
    auto driver_info = FastLED.getDriverInfo();
    drivers_available.assign(driver_info.begin(), driver_info.end());
    FL_WARN("Found " << drivers_available.size() << " driver(s) available:");
    for (fl::size i = 0; i < drivers_available.size(); i++) {
        FL_WARN("  " << (i+1) << ". " << drivers_available[i].name.c_str()
                << " (priority: " << drivers_available[i].priority
                << ", enabled: " << (drivers_available[i].enabled ? "yes" : "no") << ")");
    }

    // Validate that expected engines are available for this platform
    validateExpectedEngines();

    FL_WARN("\nStarting continuous validation test loop...");
    delay(2000);
}

// ============================================================================
// Main Loop
// ============================================================================

void loop() {
    // Increment frame counter
    frame_counter++;

    // If sanity check failed, print error continuously and delay
    if (error_sanity_check) {
        FL_ERROR("Sanity check failed - RX channel is not working");
        delay(2000);
        return;
    }

    // Check if any drivers failed in previous iterations
    if (!drivers_failed.empty()) {
        FL_WARN("\n╔════════════════════════════════════════════════════════════════╗");
        FL_WARN("║ DRIVER FAILURES DETECTED - CANNOT CONTINUE                     ║");
        FL_WARN("╚════════════════════════════════════════════════════════════════╝");

        for (fl::size i = 0; i < drivers_failed.size(); i++) {
            const auto& failure = drivers_failed[i];
            FL_ERROR("Driver '" << failure.driver_name.c_str() << "' failed on frame " << failure.frame_number);
            FL_ERROR(failure.failure_details.c_str());
        }

        FL_WARN("\nFix the above errors before continuing. Retrying in 5 seconds...");
        delay(5000);
        return;  // Exit early - don't run tests again
    }

    FL_WARN("\n╔════════════════════════════════════════════════════════════════╗");
    FL_WARN("║ FRAME " << frame_counter << " - Runtime Channel API Driver Testing");
    FL_WARN("╚════════════════════════════════════════════════════════════════╝\n");

#ifdef TEST_ONLY_ENGINE
    FL_WARN("⚠️  ENGINE FILTER ACTIVE: Testing ONLY '" << TEST_ONLY_ENGINE << "' driver");
    FL_WARN("    (Uncomment #define TEST_ONLY_ENGINE in source to disable)\n");
#endif

    FL_WARN("Starting validation tests for each driver...");
    FL_WARN("(Using persistent RX channel from setup() - not recreated)\n");

    // Track driver-level test results
    fl::vector<fl::DriverTestResult> driver_results;

    // Iterate through all available drivers and test each one
    for (fl::size i = 0; i < drivers_available.size(); i++) {
        const auto& driver = drivers_available[i];

#ifdef TEST_ONLY_ENGINE
        // Filter: Only test the specified engine
        if (fl::strcmp(driver.name.c_str(), TEST_ONLY_ENGINE) != 0) {
            FL_WARN("[FILTER] Skipping driver '" << driver.name.c_str() << "' (TEST_ONLY_ENGINE=" << TEST_ONLY_ENGINE << ")");
            continue;
        }
#endif

        fl::DriverTestResult result(driver.name.c_str());

        FL_WARN("\n╔════════════════════════════════════════════════════════════════╗");
        FL_WARN("║ TESTING DRIVER " << (i+1) << "/" << drivers_available.size() << ": " << driver.name.c_str() << " (priority: " << driver.priority << ")");
        FL_WARN("╚════════════════════════════════════════════════════════════════╝\n");

        // Test driver with WS2812B-V5 timing (filtered for focused debugging)
        // Note: testDriver() now handles first-frame discard internally
        FL_WARN("[FILTER] Testing only WS2812B-V5 timing (skipping WS2812 Standard and SK6812)\n");
        fl::NamedTimingConfig timing_config(fl::makeTimingConfig<fl::TIMING_WS2812B_V5>(), "WS2812B-V5");
        testDriver(driver.name.c_str(),
                   timing_config,
                   PIN_TX,
                   NUM_LEDS,
                   leds,
                   COLOR_ORDER,
                   rx_channel,
                   fl::span<uint8_t>(rx_buffer, RX_BUFFER_SIZE),
                   result);

        // If driver test failed, add to drivers_failed list with details
        if (result.anyFailed()) {
            // Build failure details message
            fl::sstream failure_msg;
            failure_msg << "Validation failed: " << result.passed_tests << "/" << result.total_tests << " tests passed";

            drivers_failed.push_back(fl::DriverFailureInfo(
                driver.name.c_str(),
                failure_msg.str().c_str(),
                frame_counter
            ));

            FL_ERROR("Driver " << driver.name.c_str() << " FAILED on frame " << frame_counter);
            FL_ERROR(failure_msg.str().c_str());
        }

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
