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
//   on each TX pin for independent jumper wire validation
// - Each channel in the span is validated sequentially with its own RX channel

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
// If in the fastled you can run this sketch with:
//   bash debug Validation --expect "TX Pin: 0" --expect "RX Pin: 1" --expect "DRIVER_ENABLED: RMT" --expect "DRIVER_ENABLED: SPI" --expect "DRIVER_ENABLED: PARLIO" --expect "LANE_MIN: 1" --expect "LANE_MAX: 8" --expect "STRIP_SIZE_TESTED: 10" --expect "STRIP_SIZE_TESTED: 300" --expect "TEST_CASES_GENERATED: 48" --expect "VALIDATION_READY: true" --fail-on ERROR


#include <FastLED.h>
#include "Common.h"
#include "ValidationTest.h"
#include "ValidationHelpers.h"

// ============================================================================
// Test Matrix Configuration - Multi-Driver, Multi-Lane, Variable Strip Size
// ============================================================================
//
// This sketch now supports comprehensive matrix testing with the following dimensions:
//
// 1. DRIVER SELECTION (3 options):
//    - Uncomment to test ONLY a specific driver:
//    // #define JUST_PARLIO  // Test only PARLIO driver
//    // #define JUST_RMT     // Test only RMT driver
//    // #define JUST_SPI     // Test only SPI driver
//    - Default: Test all available drivers (RMT, SPI, PARLIO)
//
// 2. LANE RANGE (1-8 lanes):
//    - Uncomment to override lane range:
//    // #define MIN_LANES 1  // Minimum number of lanes to test
//    // #define MAX_LANES 8  // Maximum number of lanes to test
//    - Default: MIN_LANES=1, MAX_LANES=8 (tests all lane counts)
//    - Each lane has decreasing LED count: Lane 0=base, Lane 1=base-1, ..., Lane N=base-N
//    - Multi-lane RX validation: Only Lane 0 is validated (hardware limitation)
//
// 3. STRIP SIZE (2 options):
//    - Uncomment to test ONLY a specific strip size:
//    // #define JUST_SMALL_STRIPS  // Test only short strips (10 LEDs)
//    // #define JUST_LARGE_STRIPS  // Test only long strips (300 LEDs)
//    - Default: Test both small (10 LEDs) and large (300 LEDs) strips
//
// TEST MATRIX SIZE:
// - Full matrix: 3 drivers × 8 lane counts × 2 strip sizes = 48 test cases
// - Use defines above to narrow scope for faster debugging
//
// EXAMPLES:
// - Test only RMT with 4 lanes on small strips:
//   #define JUST_RMT
//   #define MIN_LANES 4
//   #define MAX_LANES 4
//   #define JUST_SMALL_STRIPS
//
// - Test all drivers with 1-3 lanes on large strips:
//   #define MAX_LANES 3
//   #define JUST_LARGE_STRIPS
//
// ============================================================================

// ============================================================================
// Driver Selection Defines (uncomment to isolate specific driver)
// ============================================================================
// #define JUST_PARLIO
// #define JUST_RMT
// #define JUST_SPI

// ============================================================================
// Lane Range Defines (uncomment to override defaults)
// ============================================================================
#ifndef MIN_LANES
#define MIN_LANES 1  // Default: start at 1 lane
#endif

#ifndef MAX_LANES
#define MAX_LANES 8  // Default: test up to 8 lanes
#endif

// ============================================================================
// Strip Size Defines (uncomment to test only one size)
// ============================================================================
// #define JUST_SMALL_STRIPS
// #define JUST_LARGE_STRIPS

// Strip size constants
#define SHORT_STRIP_SIZE 10   // Short strip: 10 LEDs
#define LONG_STRIP_SIZE 300   // Long strip: 300 LEDs

// ============================================================================
// Configuration
// ============================================================================

const fl::RxDeviceType RX_TYPE = fl::RxDeviceType::RMT;

#define PIN_TX 0
#define PIN_RX 1

#define CHIPSET WS2812B
#define COLOR_ORDER RGB  // No reordering needed.

// RX buffer sized for largest possible strip (LONG_STRIP_SIZE = 300 LEDs)
// Each LED = 24 bits = 24 symbols, plus headroom for RESET pulses
#define RX_BUFFER_SIZE (LONG_STRIP_SIZE * 32 + 100)  // 300 LEDs × 32:1 expansion + headroom

uint8_t rx_buffer[RX_BUFFER_SIZE];  // Shared RX buffer for all test cases

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
// Global Test Matrix State
// ============================================================================

// Available drivers discovered in setup()
fl::vector<fl::DriverInfo> drivers_available;

// Test matrix configuration (built from defines and available drivers)
fl::TestMatrixConfig test_matrix;

// All test cases generated from matrix configuration
fl::vector<fl::TestCaseConfig> test_cases;

// Test case results (one per test case)
fl::vector<fl::TestCaseResult> test_results;

// Frame counter - tracks which iteration of loop() we're on
uint32_t frame_counter = 0;

// Test completion flag - set to true after first test matrix run
bool test_matrix_complete = false;


void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);
    const char* loop_back_mode = PIN_TX == PIN_RX ? "INTERNAL" : "JUMPER WIRE";

    FL_WARN("\n╔════════════════════════════════════════════════════════════════╗");
    FL_WARN("║ FastLED Validation - Test Matrix Configuration                ║");
    FL_WARN("╚════════════════════════════════════════════════════════════════╝");

    // Platform information
    FL_WARN("\n[PLATFORM]");
    #if defined(FL_IS_ESP_32C6)
        FL_WARN("  Chip: ESP32-C6 (RISC-V)");
    #elif defined(FL_IS_ESP_32S3)
        FL_WARN("  Chip: ESP32-S3 (Xtensa)");
    #elif defined(FL_IS_ESP_32C3)
        FL_WARN("  Chip: ESP32-C3 (RISC-V)");
    #elif defined(FL_IS_ESP_32DEV)
        FL_WARN("  Chip: ESP32 (Xtensa)");
    #else
        FL_WARN("  Chip: Unknown ESP32 variant");
    #endif

    // Hardware configuration - machine-parseable for --expect validation
    FL_WARN("\n[HARDWARE]");
    FL_WARN("  TX Pin: " << PIN_TX);  // --expect "TX Pin: 0"
    FL_WARN("  RX Pin: " << PIN_RX);  // --expect "RX Pin: 1"
    FL_WARN("  RX Device: " << (RX_TYPE == fl::RxDeviceType::RMT ? "RMT" : "ISR"));
    FL_WARN("  Loopback Mode: " << loop_back_mode);
    FL_WARN("  Color Order: RGB");
    FL_WARN("  RX Buffer Size: " << RX_BUFFER_SIZE << " bytes");

    // ========================================================================
    // RX Channel Setup
    // ========================================================================

    FL_WARN("\n[RX SETUP] Creating RX channel for LED validation");
    FL_WARN("[RX CREATE] Creating RX channel on PIN " << PIN_RX
            << " (" << (40000000 / 1000000) << "MHz, " << RX_BUFFER_SIZE << " symbols)");

    rx_channel = fl::RxDevice::create<RX_TYPE>(PIN_RX);

    if (!rx_channel) {
        FL_ERROR("[RX SETUP]: Failed to create RX channel");
        FL_ERROR("[RX SETUP]: Check that RMT peripheral is available and not in use");
        error_sanity_check = true;
        return;
    }

    FL_WARN("[RX CREATE] ✓ RX channel created successfully (will be initialized with config in begin())");
    FL_WARN("[RX SETUP] ✓ RX channel ready for LED validation\n");

    // List all available drivers and store globally
    FL_WARN("\n[DRIVER DISCOVERY]");
    drivers_available = FastLED.getDriverInfos();
    FL_WARN("  Found " << drivers_available.size() << " driver(s) available:");
    for (fl::size i = 0; i < drivers_available.size(); i++) {
        FL_WARN("    " << (i+1) << ". " << drivers_available[i].name.c_str()
                << " (priority: " << drivers_available[i].priority
                << ", enabled: " << (drivers_available[i].enabled ? "yes" : "no") << ")");
    }

    // Validate that expected engines are available for this platform
    validateExpectedEngines();

    // Test matrix configuration
    FL_WARN("\n[TEST MATRIX CONFIGURATION]");

    // Driver filtering
    #if defined(JUST_PARLIO)
        FL_WARN("  Driver Filter: JUST_PARLIO (testing PARLIO only)");
    #elif defined(JUST_RMT)
        FL_WARN("  Driver Filter: JUST_RMT (testing RMT only)");
    #elif defined(JUST_SPI)
        FL_WARN("  Driver Filter: JUST_SPI (testing SPI only)");
    #else
        FL_WARN("  Driver Filter: None (testing all available drivers)");
    #endif

    // Build test matrix and show which drivers will be tested
    test_matrix = buildTestMatrix(drivers_available);
    FL_WARN("  Drivers to Test (" << test_matrix.enabled_drivers.size() << "):");

    // Machine-parseable driver validation prints (for --expect flags)
    for (fl::size i = 0; i < test_matrix.enabled_drivers.size(); i++) {
        const char* driver_name = test_matrix.enabled_drivers[i].c_str();
        FL_WARN("    → " << driver_name);
        // Explicit validation print: "DRIVER_ENABLED: <name>"
        FL_WARN("  DRIVER_ENABLED: " << driver_name);
    }

    // Lane range - machine-parseable for --expect validation
    FL_WARN("  Lane Range: " << MIN_LANES << "-" << MAX_LANES << " lanes");
    FL_WARN("    → Lane N has base_size - N LEDs (decreasing pattern)");
    FL_WARN("    → Multi-lane: Only Lane 0 validated (hardware limitation)");
    // Explicit validation prints: "LANE_MIN: X" and "LANE_MAX: Y"
    FL_WARN("  LANE_MIN: " << MIN_LANES);
    FL_WARN("  LANE_MAX: " << MAX_LANES);

    // Strip sizes - machine-parseable for --expect validation
    #if defined(JUST_SMALL_STRIPS) && !defined(JUST_LARGE_STRIPS)
        FL_WARN("  Strip Sizes: Short only (" << SHORT_STRIP_SIZE << " LEDs)");
        FL_WARN("  STRIP_SIZE_TESTED: " << SHORT_STRIP_SIZE);
    #elif defined(JUST_LARGE_STRIPS) && !defined(JUST_SMALL_STRIPS)
        FL_WARN("  Strip Sizes: Long only (" << LONG_STRIP_SIZE << " LEDs)");
        FL_WARN("  STRIP_SIZE_TESTED: " << LONG_STRIP_SIZE);
    #else
        FL_WARN("  Strip Sizes: Both (Short=" << SHORT_STRIP_SIZE << ", Long=" << LONG_STRIP_SIZE << ")");
        FL_WARN("  STRIP_SIZE_TESTED: " << SHORT_STRIP_SIZE);
        FL_WARN("  STRIP_SIZE_TESTED: " << LONG_STRIP_SIZE);
    #endif

    // Bit patterns
    FL_WARN("  Bit Patterns: 4 mixed RGB patterns (MSB/LSB testing)");
    FL_WARN("    → Pattern A: R=0xF0, G=0x0F, B=0xAA");
    FL_WARN("    → Pattern B: R=0x55, G=0xFF, B=0x00");
    FL_WARN("    → Pattern C: R=0x0F, G=0xAA, B=0xF0");
    FL_WARN("    → Pattern D: RGB Solid Alternating");

    // Total test cases
    int total_test_cases = test_matrix.getTotalTestCases();
    int total_validation_tests = total_test_cases * 4;  // 4 bit patterns per test case
    FL_WARN("  Total Test Cases: " << total_test_cases);
    FL_WARN("  Total Validation Tests: " << total_validation_tests << " (" << total_test_cases << " cases × 4 patterns)");

    FL_WARN("\n⚠️  [HARDWARE SETUP REQUIRED]");
    FL_WARN("  If using non-RMT peripherals for TX (e.g., SPI, ParallelIO):");
    FL_WARN("  → Connect GPIO " << PIN_TX << " to GPIO " << PIN_RX << " with a physical jumper wire");
    FL_WARN("  → ESP32 GPIO matrix cannot route other peripheral outputs to RMT input");
    FL_WARN("");
    FL_WARN("  ESP32-S3 IMPORTANT: Use GPIO 11 (MOSI) for best performance");
    FL_WARN("  → GPIO 11 is SPI2 IO_MUX pin (bypasses GPIO matrix for 80MHz speed)");
    FL_WARN("  → Other GPIOs use GPIO matrix routing (limited to 26MHz, may see timing issues)");
    FL_WARN("");

    // Generate all test cases from the matrix
    FL_WARN("\n[TEST CASE GENERATION]");
    test_cases = generateTestCases(test_matrix, PIN_TX);
    FL_WARN("  Generated " << test_cases.size() << " test case(s)");
    // Machine-parseable: "TEST_CASES_GENERATED: X"
    FL_WARN("  TEST_CASES_GENERATED: " << test_cases.size());

    // Initialize result tracking for each test case
    for (fl::size i = 0; i < test_cases.size(); i++) {
        const auto& test_case = test_cases[i];
        test_results.push_back(fl::TestCaseResult(
            test_case.driver_name.c_str(),
            test_case.lane_count,
            test_case.base_strip_size
        ));
    }

    FL_WARN("\n[SETUP COMPLETE]");
    FL_WARN("  VALIDATION_READY: true");
    FL_WARN("Starting test matrix validation loop...");
    delay(2000);
}

// ============================================================================
// Helper Functions
// ============================================================================

/// @brief Run a single test case (one driver × lane count × strip size combination)
/// @param test_case Test case configuration (driver, lanes, strip sizes)
/// @param test_result Test result tracker (modified with pass/fail counts)
/// @param timing_config Chipset timing to test
/// @param rx_channel RX channel for loopback validation
/// @param rx_buffer RX buffer for capture
void runSingleTestCase(
    fl::TestCaseConfig& test_case,
    fl::TestCaseResult& test_result,
    const fl::NamedTimingConfig& timing_config,
    fl::shared_ptr<fl::RxDevice> rx_channel,
    fl::span<uint8_t> rx_buffer) {

    FL_WARN("\n╔════════════════════════════════════════════════════════════════╗");
    FL_WARN("║ TEST CASE: " << test_case.driver_name.c_str()
            << " | " << test_case.lane_count << " lane(s)"
            << " | " << test_case.base_strip_size << " LEDs");
    FL_WARN("╚════════════════════════════════════════════════════════════════╝");

    // Set this driver as exclusive for testing
    if (!FastLED.setExclusiveDriver(test_case.driver_name.c_str())) {
        FL_ERROR("Failed to set " << test_case.driver_name.c_str() << " as exclusive driver");
        test_result.skipped = true;
        return;
    }
    FL_WARN(test_case.driver_name.c_str() << " driver enabled exclusively");

    // Build TX channel configs for all lanes
    fl::vector<fl::ChannelConfig> tx_configs;
    for (int lane_idx = 0; lane_idx < test_case.lane_count; lane_idx++) {
        auto& lane = test_case.lanes[lane_idx];
        FL_WARN("[Lane " << lane_idx << "] Pin: " << lane.pin << ", LEDs: " << lane.num_leds);

        // Create channel config for this lane
        tx_configs.push_back(fl::ChannelConfig(
            lane.pin,
            timing_config.timing,
            fl::span<CRGB>(lane.leds.data(), lane.num_leds),
            COLOR_ORDER
        ));
    }

    // Create validation configuration
    fl::ValidationConfig validation_config(
        timing_config.timing,
        timing_config.name,
        fl::span<fl::ChannelConfig>(tx_configs.data(), tx_configs.size()),
        test_case.driver_name.c_str(),
        rx_channel,
        rx_buffer,
        test_case.base_strip_size,
        RX_TYPE
    );

    // Run warm-up frame (discard results)
    FL_WARN("\n[INFO] Running warm-up frame (results will be discarded)");
    int warmup_total = 0, warmup_passed = 0;
    validateChipsetTiming(validation_config, warmup_total, warmup_passed);
    FL_WARN("[INFO] Warm-up complete (" << warmup_passed << "/" << warmup_total << " passed - discarding)");

    // Run actual test frame (keep results)
    FL_WARN("\n[INFO] Running actual test frame");
    validateChipsetTiming(validation_config, test_result.total_tests, test_result.passed_tests);

    // Log test case result
    if (test_result.allPassed()) {
        FL_WARN("\n[PASS] Test case " << test_case.driver_name.c_str()
                << " (" << test_case.lane_count << " lanes, "
                << test_case.base_strip_size << " LEDs) completed successfully");
    } else {
        FL_ERROR("[FAIL] Test case " << test_case.driver_name.c_str()
                << " (" << test_case.lane_count << " lanes, "
                << test_case.base_strip_size << " LEDs) FAILED: "
                << test_result.passed_tests << "/" << test_result.total_tests << " tests passed");
    }
}

// ============================================================================
// Main Loop
// ============================================================================

void loop() {
    // If test matrix already completed, halt forever
    if (test_matrix_complete) {
        SKETCH_HALT_OK("Test matrix complete");
        return;
    }

    // Increment frame counter
    frame_counter++;

    // If sanity check failed, print error continuously and delay
    if (error_sanity_check) {
        SKETCH_HALT("Sanity check failed - RX channel is not working");
        return;
    }

    FL_WARN("\n╔════════════════════════════════════════════════════════════════╗");
    FL_WARN("║ FRAME " << frame_counter << " - Test Matrix Validation");
    FL_WARN("╚════════════════════════════════════════════════════════════════╝");

    FL_WARN("\nTest Matrix: " << test_cases.size() << " test case(s)");
    FL_WARN("Using persistent RX channel from setup() - not recreated\n");

    // Timing configuration to test (WS2812B-V5)
    fl::NamedTimingConfig timing_config(fl::makeTimingConfig<fl::TIMING_WS2812B_V5>(), "WS2812B-V5");

    // Reset all test results for this iteration
    for (fl::size i = 0; i < test_results.size(); i++) {
        test_results[i].total_tests = 0;
        test_results[i].passed_tests = 0;
        test_results[i].skipped = false;
    }

    // Iterate through all test cases
    for (fl::size i = 0; i < test_cases.size(); i++) {
        FL_WARN("\n════════════════════════════════════════════════════════════════");
        FL_WARN("TEST CASE " << (i+1) << "/" << test_cases.size());
        FL_WARN("════════════════════════════════════════════════════════════════");

        // Run this test case
        runSingleTestCase(
            test_cases[i],
            test_results[i],
            timing_config,
            rx_channel,
            fl::span<uint8_t>(rx_buffer, RX_BUFFER_SIZE)
        );

        // Short delay between test cases
        delay(500);
    }

    // Print final results table
    printTestCaseResultsTable(test_results);

    // Check for failures
    int failed_count = 0;
    for (fl::size i = 0; i < test_results.size(); i++) {
        if (test_results[i].anyFailed()) {
            failed_count++;
        }
    }

    if (failed_count > 0) {
        FL_ERROR("\n[TEST MATRIX] " << failed_count << " test case(s) FAILED");
        SKETCH_HALT("[TEST MATRIX] See results table above for details");
    } else {
        FL_WARN("\n[TEST MATRIX] ✓ All test cases PASSED");
    }

    FL_WARN("\n╔════════════════════════════════════════════════════════════════╗");
    FL_WARN("║ TEST MATRIX COMPLETE                                           ║");
    FL_WARN("╚════════════════════════════════════════════════════════════════╝");

    // Mark test matrix as complete - will halt on next loop() iteration
    test_matrix_complete = true;

    FL_WARN("\n========== TEST MATRIX COMPLETE - HALTING ==========\n");

    SKETCH_HALT_OK("Test matrix complete");
}
