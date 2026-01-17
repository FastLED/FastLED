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
// This sketch is a validation test that uses the "ERROR" keyword in FL_ERROR
// statements to signal test failures. The `bash debug` command monitors for
// the "ERROR" keyword and fails the test when detected (exit code 1).
//
// 🚫 DO NOT "CHEAT" THE TEST:
//    - DO NOT change "ERROR" to "FAIL", "WARNING", "FAILURE", or any other
//      keyword to avoid test detection
//    - DO NOT modify FL_ERROR statements unless explicitly requested by the user
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
//   bash debug Validation --input-on-trigger "VALIDATION_READY:START" --expect "TX Pin: 0" --expect "RX Pin: 1" --expect "DRIVER_ENABLED: RMT" --expect "DRIVER_ENABLED: SPI" --expect "DRIVER_ENABLED: PARLIO" --expect "LANE_MIN: 1" --expect "LANE_MAX: 8" --expect "STRIP_SIZE_TESTED: 10" --expect "STRIP_SIZE_TESTED: 300" --expect "TEST_CASES_GENERATED: 48" --expect "VALIDATION_READY: true" --fail-on ERROR


// Enable async PARLIO logging to prevent ISR watchdog timeout
// Async logging queues messages from ISRs and drains them from the main loop
#define FASTLED_LOG_PARLIO_ASYNC_ENABLED

// Disable PARLIO ISR logging BEFORE any FastLED includes to prevent watchdog timeout
// (Serial printing inside ISR exceeds watchdog threshold - must disable at translation unit level)
// #undef FASTLED_LOG_PARLIO_ENABLED  // COMMENTED OUT: Agent loop instructions require FL_LOG_PARLIO_ENABLED to be kept enabled

// ⚠️ IMPORTANT: Test matrix configuration moved to ValidationConfig.h
// This ensures consistent configuration across all compilation units (.ino and .cpp files)
#include "ValidationConfig.h"

// ============================================================================
// Test Matrix Configuration - Multi-Driver, Multi-Lane, Variable Strip Size
// ============================================================================
//
// This sketch now supports comprehensive matrix testing with the following dimensions:
//
// 1. DRIVER SELECTION (4 options):
//    - Uncomment to test ONLY a specific driver:
//    // #define JUST_PARLIO  // Test only PARLIO driver
//    // #define JUST_RMT     // Test only RMT driver
//    // #define JUST_SPI     // Test only SPI driver
//    // #define JUST_UART    // Test only UART driver
//    - Default: Test all available drivers (RMT, SPI, PARLIO, UART)
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

// Configuration is now in ValidationConfig.h (included above)
// Includes:
#include <FastLED.h>
#include "fl/stl/sstream.h"
#include "Common.h"
#include "ValidationTest.h"
#include "ValidationHelpers.h"
#include "ValidationRemote.h"
#include "SketchHalt.h"
#include "platforms/esp/32/watchdog_esp32.h"  // For fl::watchdog_setup()

// ============================================================================
// Configuration
// ============================================================================

// Serial port timeout (milliseconds) - wait for serial monitor to attach
#define SERIAL_TIMEOUT_MS 120000  // 120 seconds

const fl::RxDeviceType RX_TYPE = fl::RxDeviceType::RMT;

#define PIN_RX 0  // GPIO 3 to avoid conflict with 2-lane TX (GPIO 0, 1)
#define PIN_TX 1  // This pin is what we always test.
// For multi-lane testing use GPIO 2,3..., but keep in mind they won't
// be directly tested.


#define CHIPSET WS2812B
#define COLOR_ORDER RGB  // No reordering needed.

// RX buffer sized for largest possible strip (LONG_STRIP_SIZE LEDs)
// Each LED = 24 bits = 24 symbols, plus headroom for RESET pulses
#define RX_BUFFER_SIZE (LONG_STRIP_SIZE * 32 + 100)  // LEDs × 32:1 expansion + headroom

// Use PSRAM-backed vector for dynamic allocation (avoids DRAM overflow on ESP32S2)
fl::vector_psram<uint8_t> rx_buffer;  // Shared RX buffer - initialized in setup()

// ⚠️ CRITICAL: RMT RX channel - MUST persist across ALL loop iterations
// Created ONCE in setup(), reused for all driver tests
// DO NOT reset, destroy, or recreate this channel in loop()
fl::shared_ptr<fl::RxDevice> rx_channel;

// ============================================================================
// Global Error Tracking and Halt Control
// ============================================================================

// Sketch halt controller - handles safe halting without watchdog timer resets
SketchHalt halt;

// Remote RPC system for dynamic test control via JSON commands
// Using Singleton pattern for thread-safe lazy initialization
using RemoteControlSingleton = fl::Singleton<ValidationRemoteControl>;


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

// START command flag - set to true when "START" received on serial
bool start_command_received = false;

// Last "waiting for START" message timestamp (milliseconds)
uint32_t last_wait_message_ms = 0;


void setup() {
    Serial.begin(115200);
    // Initialize watchdog with 5 second timeout (ESP32 only)
    // Provides automatic proof-of-life monitoring and USB disconnect fix for Windows
    // DISABLED FOR DEBUGGING: Investigating if watchdog causes crash at 7.69s
    // fl::watchdog_setup(5000);
    while (!Serial && millis() < 10000);  // Wait max 10 seconds for serial

    // Initialize RX buffer dynamically (uses PSRAM if available, falls back to heap)
    rx_buffer.resize(RX_BUFFER_SIZE);



    const char* loop_back_mode = PIN_TX == PIN_RX ? "INTERNAL" : "JUMPER WIRE";

    // Build header and platform/hardware info
    fl::sstream ss;
    ss << "\n╔════════════════════════════════════════════════════════════════╗\n";
    ss << "║ FastLED Validation - Test Matrix Configuration                ║\n";
    ss << "╚════════════════════════════════════════════════════════════════╝\n";

    // Platform information
    ss << "\n[PLATFORM]\n";
    #if defined(FL_IS_ESP_32C6)
        ss << "  Chip: ESP32-C6 (RISC-V)\n";
    #elif defined(FL_IS_ESP_32S3)
        ss << "  Chip: ESP32-S3 (Xtensa)\n";
    #elif defined(FL_IS_ESP_32C3)
        ss << "  Chip: ESP32-C3 (RISC-V)\n";
    #elif defined(FL_IS_ESP_32DEV)
        ss << "  Chip: ESP32 (Xtensa)\n";
    #else
        ss << "  Chip: Unknown ESP32 variant\n";
    #endif

    // Hardware configuration - machine-parseable for --expect validation
    ss << "\n[HARDWARE]\n";
    ss << "  TX Pin: " << PIN_TX << "\n";  // --expect "TX Pin: 0"
    ss << "  RX Pin: " << PIN_RX << "\n";  // --expect "RX Pin: 1"
    ss << "  RX Device: " << (RX_TYPE == fl::RxDeviceType::RMT ? "RMT" : "ISR") << "\n";
    ss << "  Loopback Mode: " << loop_back_mode << "\n";
    ss << "  Color Order: RGB\n";
    ss << "  RX Buffer Size: " << RX_BUFFER_SIZE << " bytes";
    FL_PRINT(ss.str());

    // ========================================================================
    // RX Channel Setup
    // ========================================================================

    ss.clear();
    ss << "\n[RX SETUP] Creating RX channel for LED validation\n";
    ss << "[RX CREATE] Creating RX channel on PIN " << PIN_RX
       << " (" << (40000000 / 1000000) << "MHz, " << RX_BUFFER_SIZE << " symbols)";
    FL_PRINT(ss.str());

    rx_channel = fl::RxDevice::create<RX_TYPE>(PIN_RX);

    if (!rx_channel) {
        ss.clear();
        ss << "[RX SETUP]: Failed to create RX channel\n";
        ss << "[RX SETUP]: Check that RMT peripheral is available and not in use";
        FL_ERROR(ss.str());
        halt.error("Sanity check failed - RX channel creation failed");
        return;
    }

    ss.clear();
    ss << "[RX CREATE] ✓ RX channel created successfully (will be initialized with config in begin())\n";
    ss << "[RX SETUP] ✓ RX channel ready for LED validation";
    FL_PRINT(ss.str());

    // ========================================================================
    // GPIO Baseline Test - Verify GPIO→GPIO path works before testing PARLIO
    // ========================================================================
    ss.clear();
    ss << "\n[GPIO BASELINE TEST] Testing GPIO " << PIN_TX << " → GPIO " << PIN_RX << " connectivity";
    FL_PRINT(ss.str());

    // Test RX channel with manual GPIO toggle to confirm hardware path works
    // This isolates GPIO/hardware issues from PARLIO driver issues
    // Buffer size = 100 symbols, hz = 40MHz (same as LED validation)
    if (!testRxChannel(rx_channel, PIN_TX, PIN_RX, 40000000, 100)) {
        FL_ERROR("[GPIO BASELINE TEST] FAILED - RX did not capture manual GPIO toggles");
        FL_ERROR("[GPIO BASELINE TEST] Possible causes:");
        FL_ERROR("  1. GPIO " << PIN_TX << " and GPIO " << PIN_RX << " are not physically connected");
        FL_ERROR("  2. RX channel initialization failed");
        FL_ERROR("  3. GPIO conflict with other peripherals (USB Serial JTAG on C6 uses certain GPIOs)");
        halt.error("GPIO baseline test failed - check hardware connections");
        return;
    }

    FL_WARN("\n[GPIO BASELINE TEST] ✓ PASSED - GPIO path confirmed working");
    FL_WARN("[GPIO BASELINE TEST] ✓ RX successfully captured manual GPIO toggles");
    FL_WARN("[GPIO BASELINE TEST] ✓ Hardware connectivity verified (GPIO " << PIN_TX << " → GPIO " << PIN_RX << ")");

    // List all available drivers and store globally
    drivers_available = FastLED.getDriverInfos();
    ss.clear();
    ss << "\n[DRIVER DISCOVERY]\n";
    ss << "  Found " << drivers_available.size() << " driver(s) available:\n";
    for (fl::size i = 0; i < drivers_available.size(); i++) {
        ss << "    " << (i+1) << ". " << drivers_available[i].name.c_str()
           << " (priority: " << drivers_available[i].priority
           << ", enabled: " << (drivers_available[i].enabled ? "yes" : "no") << ")\n";
    }
    FL_PRINT(ss.str());

    // Validate that expected engines are available for this platform
    validateExpectedEngines();

    // Test matrix configuration
    ss.clear();
    ss << "\n[TEST MATRIX CONFIGURATION]\n";

    // Driver filtering
    #if defined(JUST_PARLIO)
        ss << "  Driver Filter: JUST_PARLIO (testing PARLIO only)\n";
    #elif defined(JUST_RMT)
        ss << "  Driver Filter: JUST_RMT (testing RMT only)\n";
    #elif defined(JUST_SPI)
        ss << "  Driver Filter: JUST_SPI (testing SPI only)\n";
    #elif defined(JUST_UART)
        ss << "  Driver Filter: JUST_UART (testing UART only)\n";
    #else
        ss << "  Driver Filter: None (testing all available drivers)\n";
    #endif

    // Build test matrix and show which drivers will be tested
    test_matrix = buildTestMatrix(drivers_available);
    ss << "  Drivers to Test (" << test_matrix.enabled_drivers.size() << "):\n";

    // Machine-parseable driver validation prints (for --expect flags)
    for (fl::size i = 0; i < test_matrix.enabled_drivers.size(); i++) {
        const char* driver_name = test_matrix.enabled_drivers[i].c_str();
        ss << "    → " << driver_name << "\n";
        // Explicit validation print: "DRIVER_ENABLED: <name>"
        ss << "  DRIVER_ENABLED: " << driver_name << "\n";
    }
    FL_PRINT(ss.str());

    // Lane range - machine-parseable for --expect validation
    ss.clear();
    ss << "  Lane Range: " << MIN_LANES << "-" << MAX_LANES << " lanes\n";
    ss << "    → Lane N has base_size - N LEDs (decreasing pattern)\n";
    ss << "    → Multi-lane: Only Lane 0 validated (hardware limitation)\n";
    // Explicit validation prints: "LANE_MIN: X" and "LANE_MAX: Y"
    ss << "  LANE_MIN: " << MIN_LANES << "\n";
    ss << "  LANE_MAX: " << MAX_LANES << "\n";

    // Strip sizes - machine-parseable for --expect validation
    #if defined(JUST_SMALL_STRIPS) && !defined(JUST_LARGE_STRIPS)
        ss << "  Strip Sizes: Short only (" << SHORT_STRIP_SIZE << " LEDs)\n";
        ss << "  STRIP_SIZE_TESTED: " << SHORT_STRIP_SIZE << "\n";
        ss << "  JUST_SMALL_STRIPS\n";
    #elif defined(JUST_LARGE_STRIPS) && !defined(JUST_SMALL_STRIPS)
        ss << "  Strip Sizes: Long only (" << LONG_STRIP_SIZE << " LEDs)\n";
        ss << "  STRIP_SIZE_TESTED: " << LONG_STRIP_SIZE << "\n";
        ss << "  JUST_LARGE_STRIPS\n";
    #else
        ss << "  Strip Sizes: Both (Short=" << SHORT_STRIP_SIZE << ", Long=" << LONG_STRIP_SIZE << ")\n";
        ss << "  STRIP_SIZE_TESTED: " << SHORT_STRIP_SIZE << "\n";
        ss << "  STRIP_SIZE_TESTED: " << LONG_STRIP_SIZE << "\n";
    #endif

    // Bit patterns
    ss << "  Bit Patterns: 4 mixed RGB patterns (MSB/LSB testing)\n";
    ss << "    → Pattern A: R=0xF0, G=0x0F, B=0xAA\n";
    ss << "    → Pattern B: R=0x55, G=0xFF, B=0x00\n";
    ss << "    → Pattern C: R=0x0F, G=0xAA, B=0xF0\n";
    ss << "    → Pattern D: RGB Solid Alternating\n";

    // Total test cases
    int total_test_cases = test_matrix.getTotalTestCases();
    int total_validation_tests = total_test_cases * 4;  // 4 bit patterns per test case
    ss << "  Total Test Cases: " << total_test_cases << "\n";
    ss << "  Total Validation Tests: " << total_validation_tests << " (" << total_test_cases << " cases × 4 patterns)";
    FL_PRINT(ss.str());

    ss.clear();
    ss << "\n⚠️  [HARDWARE SETUP REQUIRED]\n";
    ss << "  If using non-RMT peripherals for TX (e.g., SPI, ParallelIO):\n";
    ss << "  → Connect GPIO " << PIN_TX << " (Lane 0 TX) to GPIO " << PIN_RX << " (RX) with a physical jumper wire\n";
    ss << "  → Multi-lane: Only Lane 0 is validated via RX (other lanes transmit but not verified)\n";
    ss << "  → ESP32 GPIO matrix cannot route other peripheral outputs to RMT input\n";
    ss << "\n";
    ss << "  ESP32-S3 IMPORTANT: Use GPIO 11 (MOSI) for best performance\n";
    ss << "  → GPIO 11 is SPI2 IO_MUX pin (bypasses GPIO matrix for 80MHz speed)\n";
    ss << "  → Other GPIOs use GPIO matrix routing (limited to 26MHz, may see timing issues)\n";
    FL_PRINT(ss.str());

    // Generate all test cases from the matrix
    test_cases = generateTestCases(test_matrix, PIN_TX);
    ss.clear();
    ss << "\n[TEST CASE GENERATION]\n";
    ss << "  Generated " << test_cases.size() << " test case(s)\n";
    // Machine-parseable: "TEST_CASES_GENERATED: X"
    ss << "  TEST_CASES_GENERATED: " << test_cases.size();
    FL_PRINT(ss.str());

    // Initialize result tracking for each test case
    for (fl::size i = 0; i < test_cases.size(); i++) {
        const auto& test_case = test_cases[i];
        test_results.push_back(fl::TestCaseResult(
            test_case.driver_name.c_str(),
            test_case.lane_count,
            test_case.base_strip_size
        ));
    }

    // ========================================================================
    // Remote RPC Function Registration
    // ========================================================================

    ss.clear();
    ss << "\n[REMOTE RPC] Registering JSON RPC functions for dynamic control";
    FL_PRINT(ss.str());

    // Initialize RemoteControl singleton and register all RPC functions
    RemoteControlSingleton::instance().registerFunctions(
        drivers_available,
        test_matrix,
        test_cases,
        test_results,
        start_command_received,
        test_matrix_complete,
        frame_counter,
        rx_channel,
        rx_buffer
    );

    ss.clear();
    ss << "[REMOTE RPC] ✓ Registered 16 RPC functions:\n";
    ss << "  Phase 1 - Basic Control:\n";
    ss << "    - start()         : Trigger test matrix execution\n";
    ss << "    - status()        : Query current test state\n";
    ss << "    - drivers()       : List available drivers\n";
    ss << "  Phase 2 - Configuration:\n";
    ss << "    - getConfig()     : Query current test matrix configuration\n";
    ss << "    - setDrivers()    : Configure enabled drivers\n";
    ss << "    - setLaneRange()  : Configure lane range\n";
    ss << "    - setStripSizes() : Configure strip sizes\n";
    ss << "  Phase 3 - Selective Execution:\n";
    ss << "    - runTestCase()   : Run single test case by index\n";
    ss << "    - runDriver()     : Run all tests for specific driver\n";
    ss << "    - runAll()        : Run full test matrix with JSON results\n";
    ss << "    - getResults()    : Return all test results\n";
    ss << "    - getResult()     : Return specific test case result\n";
    ss << "  Phase 4 - Utility:\n";
    ss << "    - reset()         : Reset test state without device reboot\n";
    ss << "    - halt()          : Trigger sketch halt\n";
    ss << "    - ping()          : Health check with timestamp\n";
    ss << "    - help()          : List all RPC functions with descriptions";
    FL_PRINT(ss.str());

    ss.clear();
    ss << "\n[SETUP COMPLETE]\n";
    ss << "  VALIDATION_READY: true\n";
    ss << "Starting test matrix validation loop...";
    FL_PRINT(ss.str());
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

    fl::sstream ss;
    ss << "\n╔════════════════════════════════════════════════════════════════╗\n";
    ss << "║ TEST CASE: " << test_case.driver_name.c_str()
       << " | " << test_case.lane_count << " lane(s)"
       << " | " << test_case.base_strip_size << " LEDs\n";
    ss << "╚════════════════════════════════════════════════════════════════╝";
    FL_PRINT(ss.str());

    // Set this driver as exclusive for testing
    if (!FastLED.setExclusiveDriver(test_case.driver_name.c_str())) {
        FL_ERROR("Failed to set " << test_case.driver_name.c_str() << " as exclusive driver");
        test_result.skipped = true;
        return;
    }
    FL_PRINT(test_case.driver_name.c_str() << " driver enabled exclusively");

    // Build TX channel configs for all lanes
    fl::vector<fl::ChannelConfig> tx_configs;
    for (int lane_idx = 0; lane_idx < test_case.lane_count; lane_idx++) {
        auto& lane = test_case.lanes[lane_idx];
        FL_PRINT("[Lane " << lane_idx << "] Pin: " << lane.pin << ", LEDs: " << lane.num_leds);

        // Create channel config for this lane
        tx_configs.push_back(fl::ChannelConfig(
            lane.pin,
            timing_config.timing,
            lane.leds,
            COLOR_ORDER
        ));
    }

    // Create validation configuration
    fl::ValidationConfig validation_config(
        timing_config.timing,
        timing_config.name,
        tx_configs,
        test_case.driver_name.c_str(),
        rx_channel,
        rx_buffer,
        test_case.base_strip_size,
        RX_TYPE
    );

    // Run warm-up frame (discard results)
    FL_PRINT("\n[INFO] Running warm-up frame (results will be discarded)");
    int warmup_total = 0, warmup_passed = 0;
    validateChipsetTiming(validation_config, warmup_total, warmup_passed);
    FL_PRINT("[INFO] Warm-up complete (" << warmup_passed << "/" << warmup_total << " passed - discarding)");

    // Run actual test frame (keep results)
    FL_PRINT("\n[INFO] Running actual test frame");
    validateChipsetTiming(validation_config, test_result.total_tests, test_result.passed_tests);

    // Log test case result
    if (test_result.allPassed()) {
        FL_PRINT("\n[PASS] Test case " << test_case.driver_name.c_str()
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
    // IMPORTANT: Must be first line - handles halt state and prevents watchdog resets
    if (halt.check()) return;

    // Process scheduled RPC commands
    RemoteControlSingleton::instance().tick(millis());

    // Check for START command or JSON RPC commands on serial input
    if (!start_command_received) {
        // Process serial input via remote control
        RemoteControlSingleton::instance().processSerialInput();

        // DEBUGGING: Auto-send START command after 1 second to skip idle waiting
        // This helps isolate whether crash is timing-related or test-execution-related
        static bool auto_start_sent = false;
        if (!auto_start_sent && millis() > 1000) {
            FL_PRINT("[DEBUG] Auto-sending START command after 1 second");
            start_command_received = true;
            auto_start_sent = true;
        }

        // If START not received yet, print waiting message every 5 seconds
        if (!start_command_received) {
            uint32_t now = millis();
            if (now - last_wait_message_ms >= 5000) {
                FL_PRINT("[WAITING] Waiting for START command on serial input...");
                last_wait_message_ms = now;
            }
            delay(100);  // Small delay to prevent tight loop
            return;
        }
    }

    // If test matrix already completed, halt with success message
    if (test_matrix_complete) {
        halt.finish("Test matrix complete");
        return;
    }

    // Increment frame counter
    frame_counter++;

    fl::sstream ss;
    ss << "\n╔════════════════════════════════════════════════════════════════╗\n";
    ss << "║ FRAME " << frame_counter << " - Test Matrix Validation\n";
    ss << "╚════════════════════════════════════════════════════════════════╝\n";
    ss << "\nTest Matrix: " << test_cases.size() << " test case(s)\n";
    ss << "Using persistent RX channel from setup() - not recreated";
    FL_PRINT(ss.str());

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
        ss.clear();
        ss << "\n════════════════════════════════════════════════════════════════\n";
        ss << "TEST CASE " << (i+1) << "/" << test_cases.size() << "\n";
        ss << "════════════════════════════════════════════════════════════════";
        FL_PRINT(ss.str());

        // Run this test case
        runSingleTestCase(
            test_cases[i],
            test_results[i],
            timing_config,
            rx_channel,
            rx_buffer
        );

        // Short delay between test cases
        delay(500);
    }

    // Flush async PARLIO logs accumulated during test execution
    FL_LOG_PARLIO_ASYNC_FLUSH();

    // Print final results table
    printTestCaseResultsTable(test_results);

    // Print prominent final result banner
    printFinalResultBanner(test_results);

    // Check for failures
    int failed_count = 0;
    for (fl::size i = 0; i < test_results.size(); i++) {
        if (test_results[i].anyFailed()) {
            failed_count++;
        }
    }

    if (failed_count > 0) {
        halt.error("[TEST MATRIX] See results table and summary above for details");
    } else {
        FL_PRINT("\n[TEST MATRIX] ✓ All test cases PASSED");
        // Signal successful completion for early exit via --stop pattern
        FL_PRINT("\nVALIDATION_SUITE_COMPLETE");
    }

    ss.clear();
    ss << "\n╔════════════════════════════════════════════════════════════════╗\n";
    ss << "║ TEST MATRIX COMPLETE                                           ║\n";
    ss << "╚════════════════════════════════════════════════════════════════╝\n";
    ss << "\n========== TEST MATRIX COMPLETE - HALTING ==========";
    FL_PRINT(ss.str());

    // Flush any remaining async PARLIO logs before halting
    FL_LOG_PARLIO_ASYNC_FLUSH();

    // Mark test matrix as complete - will halt on next loop() iteration
    test_matrix_complete = true;
}
