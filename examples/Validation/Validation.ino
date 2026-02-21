// @filter: (platform is esp32) or (platform is native)

// examples/Validation/Validation.ino
//
// FastLED LED Timing Validation Sketch for ESP32.
//
// This sketch validates LED output by reading back timing values using the
// RMT peripheral in receive mode. It performs TX→RX loopback testing to verify
// that transmitted LED data matches received data.
//
// DEMONSTRATES:
// 1. Runtime Channel API (FastLED.add) for iterating through all available
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
// ASYNC ARCHITECTURE:
// The JSON-RPC system runs on an async task (10ms interval) registered during setup().
// The task calls RemoteControlSingleton::tick() and processSerialInput() automatically.
// loop() aggressively pumps fl::async_run() (100x per iteration) to ensure responsiveness.
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
// JSON-RPC SCRIPTING LANGUAGE:
// This sketch uses JSON-RPC for all test orchestration. Examples:
//
//   # Run validation with pure JSON-RPC (no text patterns needed)
//   bash validate --all
//
//   # Custom JSON-RPC workflow
//   uv run python -c "
//   from ci.rpc_client import RpcClient
//   with RpcClient('/dev/ttyUSB0') as c:
//       c.send('configure', {'driver':'PARLIO', 'laneSizes':[100]})
//       result = c.send('runTest')
//       print(result)
//   "
//
// Text output (FL_PRINT, FL_WARN) is for human diagnostics ONLY.
// Machine coordination uses ONLY JSON-RPC commands and JSONL events.
// ============================================================================


// Enable async PARLIO logging to prevent ISR watchdog timeout
// Async logging queues messages from ISRs and drains them from the main loop
#define FASTLED_LOG_PARLIO_ASYNC_ENABLED

// Disable PARLIO ISR logging BEFORE any FastLED includes to prevent watchdog timeout
// (Serial printing inside ISR exceeds watchdog threshold - must disable at translation unit level)
// #undef FASTLED_LOG_PARLIO_ENABLED  // COMMENTED OUT: Agent loop instructions require FL_LOG_PARLIO_ENABLED to be kept enabled

// ============================================================================
// Test Matrix Configuration - Multi-Driver, Multi-Lane, Variable Strip Size
// ============================================================================
//
// This sketch now supports comprehensive matrix testing with the following dimensions:
//
// 1. DRIVER SELECTION (5 options):
//    - Uncomment to test ONLY a specific driver:
//    // #define JUST_PARLIO  // Test only PARLIO driver
//    // #define JUST_RMT     // Test only RMT driver
//    // #define JUST_SPI     // Test only SPI driver
//    // #define JUST_UART    // Test only UART driver
//    // #define JUST_I2S     // Test only I2S LCD_CAM driver (ESP32-S3 only)
//    - Default: Test all available drivers (RMT, SPI, PARLIO, UART, I2S)
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

#include <Arduino.h>
#include <FastLED.h>
#include "fl/stl/sstream.h"
#include "Common.h"
#include "ValidationTest.h"
#include "ValidationHelpers.h"
#include "ValidationRemote.h"
#include "ValidationAsync.h"
#include "ValidationPlatform.h"

// ============================================================================
// Configuration
// ============================================================================

// Serial port timeout (milliseconds) - wait for serial monitor to attach
#define SERIAL_TIMEOUT_MS 120000  // 120 seconds

const fl::RxDeviceType RX_TYPE = fl::RxDeviceType::RMT;

// ============================================================================
// Platform-Specific Pin Defaults
// ============================================================================
// These defaults are chosen based on hardware constraints of each platform.
// They can be overridden at runtime via JSON-RPC (setPins, setTxPin, setRxPin).

constexpr int DEFAULT_PIN_TX = validation::defaultTxPin();
constexpr int DEFAULT_PIN_RX = validation::defaultRxPin();

// Legacy macros for backward compatibility in existing code
#define PIN_TX g_validation_state->pin_tx
#define PIN_RX g_validation_state->pin_rx

#define CHIPSET WS2812B
#define COLOR_ORDER RGB  // No reordering needed.

// RX buffer sized for maximum expected strip size
// Each LED = 24 bits = 24 symbols, plus headroom for RESET pulses
// Maximum: 3000 LEDs (hardcoded for ESP32/S3 with PSRAM support)
#define RX_BUFFER_SIZE (3000 * 32 + 100)  // LEDs × 32:1 expansion + headroom

// Factory function for creating RxDevice instances
// This allows ValidationRemoteControl to recreate the RX channel when the pin changes
fl::shared_ptr<fl::RxDevice> createRxDevice(int pin) {
    return fl::RxDevice::create<RX_TYPE>(pin);
}

// Global validation state (shared between main loop and RPC handlers)
// Use PSRAM-backed vector for RX buffer to avoid DRAM overflow on ESP32S2
fl::vector_psram<uint8_t> g_rx_buffer_storage;  // Actual buffer storage
fl::shared_ptr<ValidationState> g_validation_state;  // Shared state pointer

// ============================================================================
// Serial Initialization Helper
// ============================================================================

// Initialize serial buffers with platform-specific configuration
// Note: Some boards (ESP32S2) don't support setTxBufferSize() on USBCDC interface
void init_serial_buffers() {
#if defined(FL_IS_ESP32) && !defined(FL_IS_ESP_32S2)
    Serial.setTxBufferSize(4096);  // 4KB buffer (default is 256 bytes)
#endif
}

// ============================================================================
// Global State
// ============================================================================

// Remote RPC system for dynamic test control via JSON commands
// Using Singleton pattern for thread-safe lazy initialization
using RemoteControlSingleton = fl::Singleton<ValidationRemoteControl>;


// ============================================================================
// Global Validation State (Simplified - No Test Matrix)
// ============================================================================

// Frame counter - tracks which iteration of loop() we're on (diagnostic)
uint32_t frame_counter = 0;


void setup() {
    // Initialize serial buffers with platform-specific configuration
    // Must be called BEFORE Serial.begin()
    init_serial_buffers();
    Serial.begin(115200);
    // Watchdog disabled — investigating if it causes crash at 7.69s
    while (!Serial && millis() < SERIAL_TIMEOUT_MS);  // Wait for serial monitor (early exits when connected)

    FL_WARN("[SETUP] Validation sketch starting - serial output active");

    // Initialize RX buffer dynamically (uses PSRAM if available, falls back to heap)
    g_rx_buffer_storage.resize(RX_BUFFER_SIZE);

    // Initialize global validation state
    g_validation_state = fl::make_shared<ValidationState>();
    g_validation_state->pin_tx = DEFAULT_PIN_TX;
    g_validation_state->pin_rx = DEFAULT_PIN_RX;
    g_validation_state->default_pin_tx = DEFAULT_PIN_TX;
    g_validation_state->default_pin_rx = DEFAULT_PIN_RX;
    g_validation_state->rx_buffer = fl::span<uint8_t>(g_rx_buffer_storage.data(), g_rx_buffer_storage.size());
    g_validation_state->rx_factory = createRxDevice;

    const char* loop_back_mode = PIN_TX == PIN_RX ? "INTERNAL" : "JUMPER WIRE";

    // Build header and platform/hardware info
    fl::sstream ss;
    ss << "\n╔════════════════════════════════════════════════════════════════╗\n";
    ss << "║ FastLED Validation - Test Matrix Configuration                ║\n";
    ss << "╚════════════════════════════════════════════════════════════════╝\n";

    // Platform information
    ss << "\n[PLATFORM]\n";
    ss << "  Chip: " << validation::chipName() << "\n";

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

    g_validation_state->rx_channel = fl::RxDevice::create<RX_TYPE>(PIN_RX);

    if (!g_validation_state->rx_channel) {
        ss.clear();
        ss << "[RX SETUP]: Failed to create RX channel\n";
        ss << "[RX SETUP]: Check that RMT peripheral is available and not in use";
        FL_ERROR(ss.str());
        FL_ERROR("Sanity check failed - RX channel creation failed");
        return;
    }

    ss.clear();
    ss << "[RX CREATE] ✓ RX channel created successfully (will be initialized with config in begin())\n";
    ss << "[RX SETUP] ✓ RX channel ready for LED validation";
    FL_PRINT(ss.str());

    // ========================================================================
    // Remote RPC Function Registration (EARLY - before GPIO baseline test)
    // ========================================================================
    // IMPORTANT: Register RPC functions BEFORE the GPIO baseline test so that
    // even if setup() fails early, the testGpioConnection command can be used
    // to diagnose hardware connection issues.

    ss.clear();
    ss << "\n[REMOTE RPC] Registering JSON RPC functions for dynamic control";
    FL_PRINT(ss.str());

    // Initialize RemoteControl singleton and register all RPC functions
    RemoteControlSingleton::instance().registerFunctions(g_validation_state);

    FL_PRINT("[REMOTE RPC] ✓ RPC system initialized (testGpioConnection available)");

    // ========================================================================
    // Async Task Setup - JSON-RPC Processing
    // ========================================================================
    FL_PRINT("[ASYNC] Setting up JSON-RPC async task (10ms interval)");
    validation::setupRpcAsyncTask(RemoteControlSingleton::instance(), 10);
    FL_PRINT("[ASYNC] ✓ JSON-RPC task registered with scheduler");

    // Stub: register self-running validation client (no-op on ESP32)
    validation::maybeRegisterStubAutorun(RemoteControlSingleton::instance(),
                                          g_validation_state);

    // ========================================================================
    // GPIO Baseline Test - Verify GPIO→GPIO path works before testing PARLIO
    // ========================================================================
    ss.clear();
    ss << "\n[GPIO BASELINE TEST] Testing GPIO " << PIN_TX << " → GPIO " << PIN_RX << " connectivity";
    FL_PRINT(ss.str());

    // GPIO baseline test moved to loop() - wait for RPC start signal before testing
    // This allows JSON-RPC commands (testGpioConnection, findConnectedPins) to run first
    FL_WARN("[GPIO BASELINE TEST] Deferred to loop() - waiting for RPC start signal");

    // List all available drivers and store globally
    g_validation_state->drivers_available = FastLED.getDriverInfos();
    ss.clear();
    ss << "\n[DRIVER DISCOVERY]\n";
    ss << "  Found " << g_validation_state->drivers_available.size() << " driver(s) available:\n";
    for (fl::size i = 0; i < g_validation_state->drivers_available.size(); i++) {
        ss << "    " << (i+1) << ". " << g_validation_state->drivers_available[i].name.c_str()
           << " (priority: " << g_validation_state->drivers_available[i].priority
           << ", enabled: " << (g_validation_state->drivers_available[i].enabled ? "yes" : "no") << ")\n";
    }
    FL_PRINT(ss.str());

    // Validate that expected engines are available for this platform
    validateExpectedEngines();

    // Emit JSON-RPC ready event for Python orchestration
    fl::Json readyData = fl::Json::object();
    readyData.set("ready", true);
    readyData.set("setupTimeMs", static_cast<int64_t>(millis()));
    readyData.set("drivers", static_cast<int64_t>(g_validation_state->drivers_available.size()));
    readyData.set("pinTx", static_cast<int64_t>(PIN_TX));
    readyData.set("pinRx", static_cast<int64_t>(PIN_RX));
    printStreamRaw("ready", readyData);

    // Human-readable diagnostics (not machine-parsed)
    FL_PRINT("\n[SETUP COMPLETE] Validation ready - awaiting JSON-RPC commands");
    delay(2000);
}

// ============================================================================
// Main Loop - Pure Command Runner (Phase 6 Refactoring)
// ============================================================================
// The main loop is simplified to be a pure JSON-RPC command runner.
// All test orchestration is handled by Python via RPC commands like:
//   - testGpioConnection: Pre-flight hardware check
//   - runQuickTest: Fast single-test execution
//   - runAll: Full test matrix execution
//   - configure: Setup test parameters
//
// This architecture enables:
//   - Fast test iteration (<100ms per test case)
//   - Python-controlled test sequencing
//   - Easy retry logic and error recovery

void loop() {
    // Aggressively pump async tasks (including JSON-RPC task)
    // This ensures RPC commands are processed frequently even without delay() calls
    for (int i = 0; i < 100; i++) {
        fl::async_run();
    }

    // Run GPIO baseline test once after device is ready (allows JSON-RPC to be operational first)
    // This test is informational only - we continue regardless of pass/fail
    if (!g_validation_state->gpio_baseline_test_done) {
        // Wait 500ms after boot to ensure JSON-RPC is fully operational
        if (millis() > 500) {
            g_validation_state->gpio_baseline_test_done = true;

            FL_PRINT("\n[GPIO BASELINE TEST] Testing GPIO " << PIN_TX << " → GPIO " << PIN_RX << " connectivity");

            // Test RX channel with manual GPIO toggle to confirm hardware path works
            // This isolates GPIO/hardware issues from PARLIO driver issues
            // Buffer size = 100 symbols, hz = 40MHz (same as LED validation)
            if (!testRxChannel(g_validation_state->rx_channel, PIN_TX, PIN_RX, 40000000, 100)) {
                FL_WARN("[GPIO BASELINE TEST] FAILED - RX did not capture manual GPIO toggles");
                FL_WARN("[GPIO BASELINE TEST] Possible causes:");
                FL_WARN("  1. GPIO " << PIN_TX << " and GPIO " << PIN_RX << " are not physically connected");
                FL_WARN("  2. RX channel initialization failed");
                FL_WARN("  3. GPIO conflict with other peripherals (USB Serial JTAG on C6 uses certain GPIOs)");
                FL_WARN("[GPIO BASELINE TEST] Continuing - JSON-RPC pin discovery/testing available");
            } else {
                FL_WARN("\n[GPIO BASELINE TEST] ✓ PASSED - GPIO path confirmed working");
                FL_WARN("[GPIO BASELINE TEST] ✓ RX successfully captured manual GPIO toggles");
                FL_WARN("[GPIO BASELINE TEST] ✓ Hardware connectivity verified (GPIO " << PIN_TX << " → GPIO " << PIN_RX << ")");
            }
        }
    }

    // Emit periodic ready status (every 5 seconds) for Python connection detection
    static uint32_t last_status_ms = 0;
    uint32_t now = millis();
    if (now - last_status_ms >= 5000) {
        fl::Json status = fl::Json::object();
        status.set("ready", true);
        status.set("uptimeMs", static_cast<int64_t>(now));
        printStreamRaw("status", status);
        last_status_ms = now;
    }
}
