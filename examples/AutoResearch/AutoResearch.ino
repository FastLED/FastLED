// @filter
// require:
//   - board: esp32s3,esp32c6,esp32p4,teensy41,teensy40
// exclude:
//   - memory: low
// @end-filter

// examples/AutoResearch/AutoResearch.ino
//
// FastLED LED Timing AutoResearch Sketch for ESP32.
//
// This sketch validates LED output by reading back timing values using the
// RMT peripheral in receive mode. It performs TX→RX loopback testing to verify
// that transmitted LED data matches received data.
//
// DEMONSTRATES:
// 1. Runtime Channel API (FastLED.add) for iterating through all available
//    drivers (RMT, SPI, PARLIO) and testing multiple chipset timings dynamically
//    by creating and destroying controllers for each driver.
// 2. Multi-channel autoresearch support: Pass span<const ChannelConfig> to validate
//    multiple LED strips/channels simultaneously. Each channel is independently
//    validated with its own RX loopback channel.
//
// Use case: When developing a FastLED driver for a new peripheral, it is useful
// to read back the LED's received data to verify that the timing is correct.
//
// MULTI-CHANNEL MODE:
// - Single-channel: Pass one ChannelConfig - uses shared RX channel object (created in setup())
// - Multi-channel: Pass multiple ChannelConfigs - creates dynamic RX channels
//   on each TX pin for independent jumper wire autoresearch
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
//   - In multi-channel mode: Separate autoresearch results for each channel/pin
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
//   autoResearchChipsetTiming(timing, "WS2812", fl::span(configs, 3), nullptr, buffer);
//   ```
//
// ASYNC ARCHITECTURE:
// The JSON-RPC system runs on an async task (10ms interval) registered during setup().
// The task calls RemoteControlSingleton::tick() and processSerialInput() automatically.
// loop() aggressively pumps fl::task::run() (100x per iteration) to ensure responsiveness.
//

// ============================================================================
// AGENT INSTRUCTIONS
// ============================================================================
// This sketch is an autoresearch test that uses the "ERROR" keyword in FL_ERROR
// statements to signal test failures. The `bash debug` command monitors for
// the "ERROR" keyword and fails the test when detected (exit code 1).
//
// 🚫 DO NOT "CHEAT" THE TEST:
//    - DO NOT change "ERROR" to "FAIL", "WARNING", "FAILURE", or any other
//      keyword to avoid test detection
//    - DO NOT modify FL_ERROR statements unless explicitly requested by the user
//    - The "ERROR" keyword is INTENTIONAL and part of the autoresearch contract
//
// ✅ VALID MODIFICATIONS (only if explicitly requested):
//    - User asks: "Change the error message format"
//    - User asks: "Use a different warning macro"
//    - User asks: "Remove the ERROR prefix"
//
// ❌ INVALID MODIFICATIONS (considered "cheating"):
//    - Changing "ERROR" to avoid `bash debug --fail-on ERROR` detection
//    - Renaming to bypass automated test failure detection
//    - Any modification that circumvents the autoresearch contract
//
// If the test is failing, investigate and FIX THE ACTUAL BUG that causes
// the "ERROR" message to be printed. Do not hide the symptom by changing
// the error message keyword.
// ============================================================================
// JSON-RPC SCRIPTING LANGUAGE:
// This sketch uses JSON-RPC for all test orchestration. Examples:
//
//   # Run autoresearch with pure JSON-RPC (no text patterns needed)
//   bash autoresearch --all
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
// 2. LANE RANGE (1-16 lanes):
//    - Uncomment to override lane range:
//    // #define MIN_LANES 1  // Minimum number of lanes to test
//    // #define MAX_LANES 16  // Maximum number of lanes to test
//    - Default: MIN_LANES=1, MAX_LANES=16 (tests all lane counts)
//    - Each lane has decreasing LED count: Lane 0=base, Lane 1=base-1, ..., Lane N=base-N
//    - Multi-lane RX autoresearch: Only Lane 0 is validated (hardware limitation)
//
// 3. STRIP SIZE (2 options):
//    - Uncomment to test ONLY a specific strip size:
//    // #define JUST_SMALL_STRIPS  // Test only short strips (10 LEDs)
//    // #define JUST_LARGE_STRIPS  // Test only long strips (300 LEDs)
//    - Default: Test both small (10 LEDs) and large (300 LEDs) strips
//
// TEST MATRIX SIZE:
// - Full matrix: 3 drivers × 16 lane counts × 2 strip sizes = 96 test cases
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

#include <FastLED.h>
#include "fl/channels/all_drivers.h"  // for FastLED.enableAllDrivers() post-#2428
#include "fl/wdt/watchdog.h"  // FL_WATCHDOG_AUTO() — unified cross-platform WDT guard
#include "fl/stl/undef.h"  // Undefine Arduino macros (DEFAULT, INPUT, OUTPUT)
#include "fl/stl/sstream.h"
#include "Common.h"
#include "AutoResearchTest.h"
#include "AutoResearchHelpers.h"
#include "AutoResearchRemote.h"
#include "AutoResearchBle.h"
#include "AutoResearchNet.h"
#include "AutoResearchAsync.h"
#include "AutoResearchPlatform.h"
#include "AutoResearchSimd.h"
#include "AutoResearchWave8Expand.h"  // boot-time #2526 micro-bench
#include "AutoResearchParlioEncode.h" // full parlio encode bench (post-byte-LUT)
#include "AutoResearchParlioStream.h" // #2548 PARLIO streaming validation (RPC-driven)

// ============================================================================
// Hardware Watchdog (crash recovery)
// ============================================================================
// Use the unified cross-platform fl::Watchdog API via the FL_WATCHDOG_AUTO()
// macro at the top of loop(). It lazily arms the WDT on first call, prints
// the prior-boot reset/crash diagnostic, pauses 3 s on a crash so the
// developer can read the message, and feeds the WDT on both ctor and dtor.
// Works portably across ESP32 / Teensy 4 / Teensy 3 / RP2040 / nRF52 /
// STM32 / SAMD / Apollo3 / MGM240 / AVR (the noop fallback on platforms
// without a real WDT keeps the call safe).
//
// The previous `fl::watchdog_setup(...)` prototype is superseded by
// `fl::Watchdog::instance().begin()` and the FL_WATCHDOG_AUTO() macro.

// ============================================================================
// Configuration
// ============================================================================

// Serial port timeout (milliseconds) - wait for serial monitor to attach
static constexpr uint32_t SERIAL_TIMEOUT_MS = 120000;  // 120 seconds
#if defined(FL_IS_ESP_32S3) || defined(FL_IS_ESP_32C3) || defined(FL_IS_ESP_32C6) || defined(FL_IS_ESP_32H2) || defined(CONFIG_IDF_TARGET_ESP32P4)
static constexpr uint32_t AUTORESEARCH_SERIAL_WAIT_MS = 2000;
#else
static constexpr uint32_t AUTORESEARCH_SERIAL_WAIT_MS = SERIAL_TIMEOUT_MS;
#endif

const fl::RxBackend RX_BACKEND = fl::RxBackend::PLATFORM_DEFAULT;

// AutoResearch must recover from a wedged driver/RPC command without host
// intervention. Keep this shorter than the Python RPC timeout so a device-side
// hang becomes a watchdog reboot instead of a permanently owned/dead port.
static constexpr uint32_t AUTORESEARCH_WATCHDOG_TIMEOUT_MS = 5000;

// ============================================================================
// Platform-Specific Pin Defaults
// ============================================================================
// These defaults are chosen based on hardware constraints of each platform.
// They can be overridden at runtime via JSON-RPC (setPins, setTxPin, setRxPin).

constexpr int DEFAULT_PIN_TX = autoresearch::defaultTxPin();
constexpr int DEFAULT_PIN_RX = autoresearch::defaultRxPin();

// Legacy macros for backward compatibility in existing code
#define PIN_TX g_autoresearch_state->pin_tx
#define PIN_RX g_autoresearch_state->pin_rx

#define CHIPSET WS2812B
#define COLOR_ORDER RGB  // No reordering needed.

// RX buffer sized for maximum expected strip size
// Each LED = 24 bits = 24 symbols, plus headroom for RESET pulses
// Maximum: 3000 LEDs (hardcoded for ESP32/S3 with PSRAM support)
#if defined(FL_IS_TEENSY_4X) || defined(FL_IS_ESP_32C6) || defined(FL_IS_ESP_32H2) || defined(FL_IS_ESP_32C5)
constexpr int RX_BUFFER_SIZE = 100 * 32 + 100;  // Memory-constrained: 100 LEDs max for autoresearch
#else
constexpr int RX_BUFFER_SIZE = 3000 * 32 + 100;  // LEDs × 32:1 expansion + headroom
#endif

// Factory function for creating RxChannel instances
// This allows AutoResearchRemoteControl to recreate the RX channel when the pin changes
fl::shared_ptr<fl::RxChannel> createRxDevice(int pin) {
    fl::RxChannelConfig config(pin, RX_BACKEND);
    return FastLED.addRx(config);
}

// Global autoresearch state (shared between main loop and RPC handlers)
// Use PSRAM-backed vector for RX buffer to avoid DRAM overflow on ESP32S2
fl::vector_psram<uint8_t> g_rx_buffer_storage;  // Actual buffer storage (falls back to SRAM without PSRAM)
fl::shared_ptr<AutoResearchState> g_autoresearch_state;  // Shared state pointer

// ============================================================================
// Serial Initialization Helper
// ============================================================================

// Initialize serial buffers with platform-specific configuration
// Note: Some boards (ESP32S2) don't support setTxBufferSize() on USBCDC interface
void init_serial_buffers() {
#if defined(FL_IS_ESP32) && !defined(FL_IS_ESP_32S2)
    Serial.setTxBufferSize(4096);  // 4KB buffer (default is 256 bytes)  // ok serial - platform-specific TX buffer sizing, no fl:: equivalent
#endif
}

// ============================================================================
// Global State
// ============================================================================

// Remote RPC system for dynamic test control via JSON commands
// Using Singleton pattern for thread-safe lazy initialization
using RemoteControlSingleton = fl::Singleton<AutoResearchRemoteControl>;


// ============================================================================
// Global AutoResearch State (Simplified - No Test Matrix)
// ============================================================================

// Frame counter - tracks which iteration of loop() we're on (diagnostic)
uint32_t frame_counter = 0;


#if defined(FL_IS_TEENSY_4X) && defined(__IMXRT1062__)
// =============================================================================
// CRITICAL: WDOG3 (RTWDOG) startup-early-hook recovery (FastLED#2731)
// =============================================================================
// The iMXRT1062 RTWDOG (WDOG3) is enabled by default at chip reset with
// TOVAL = 0x400 LPO ticks ≈ 32 ms. The Teensy 4 core does NOT disable it in
// startup.c — most sketches happen to be fast enough or stay in WAIT mode
// where WDOG3 stops counting, so it never bites in practice.
//
// But if a previous run armed WDOG3 with a short timeout, or if any
// peripheral init takes longer than 32 ms, the chip enters a reset loop
// where the new sketch can't even complete USB enumeration. Symptom: red
// LED double-blink + "Unknown USB Device (Device Descriptor Request Failed)".
//
// `startup_early_hook` is a weak symbol in Teensyduino's startup.c that runs
// very early in ResetHandler2(), before main() and before ITCM is even
// initialized. Overriding it here disables WDOG3 BEFORE any other code can
// observe a reset. Must be in FLASHMEM (ITCM not ready yet).
extern "C" FLASHMEM void startup_early_hook(void) {
    // Enable the WDOG3 clock gate (CCM_CCGR5 bits 4-5) so the WDOG3
    // peripheral registers are addressable. Per imxrt.h comment:
    // "WDOG3 requires CCM_CCGR5_WDOG3".
    CCM_CCGR5 |= CCM_CCGR5_WDOG3(3);
    // Ensure the CCM store is visible to the peripheral bus before
    // touching WDOG3.
    __asm__ volatile ("dsb" ::: "memory");
    __asm__ volatile ("isb" ::: "memory");

    // Unlock RTWDOG with the 32-bit key (works because CS.CMD32EN=1 at reset).
    WDOG3_CNT = 0xD928C520u;

    // Wait for the unlock window to open. Bounded spin — if for any reason
    // the unlock never lands, we'd rather continue and let the WDOG bite
    // than hang here forever.
    {
        volatile uint32_t spin = 0;
        while (!(WDOG3_CS & WDOG_CS_ULK)) {
            if (++spin > 200000u) break;
        }
    }

    // Disable: clear EN, keep CMD32EN+UPDATE+CLK(LPO) so future arms work.
    // Set TOVAL to max so even if the disable doesn't take, we have ~524 sec
    // before any reset can occur.
    WDOG3_TOVAL = 0xFFFFu;
    WDOG3_WIN = 0;
    WDOG3_CS = WDOG_CS_CMD32EN | WDOG_CS_UPDATE | WDOG_CS_CLK(1);  // EN=0

    // Wait for reconfigure complete (bounded).
    {
        volatile uint32_t spin = 0;
        while (!(WDOG3_CS & WDOG_CS_RCS)) {
            if (++spin > 200000u) break;
        }
    }
}
#endif

void setup() {
    // Initialize serial buffers with platform-specific configuration
    // Must be called BEFORE Serial.begin()
    init_serial_buffers();
    fl::serial_begin(115200);
#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
    // Make HWCDC writes drop instead of block when no host is reading.
    // Without this, Serial.print() on ESP32-C3/C6/H2 can stall up to ~2s
    // per call once the TX ring fills with no host draining it.
    // See FastLED issue #2668 and arduino-esp32 PR #7583.
    // Redundant with fl::serial_begin() which already sets this on HWCDC,
    // kept as defense-in-depth for sketches that bypass fl::serial_begin.
    Serial.setTxTimeoutMs(0);  // ok serial - platform-specific TX timeout, no fl:: equivalent
#endif
    uint32_t serial_wait_start = millis();
    while (!fl::serial_ready() && (millis() - serial_wait_start) < AUTORESEARCH_SERIAL_WAIT_MS);  // Wait for serial monitor (early exits when connected)

    FL_WARN("[SETUP] AutoResearch sketch starting - serial output active");

    // Note: the unified watchdog is armed lazily by FL_WATCHDOG_AUTO() at the
    // top of loop() — no explicit setup() call needed. The macro prints the
    // prior-boot reset info via ResetInfo::describe() and pauses 3 s on crash
    // before the new timer arms. Per-platform boot diagnostics (Teensy 4
    // SRC_SRSR bit decode + bundled CrashReport, ESP32 panic backtrace, etc.)
    // are emitted by the platform watchdog impl from Watchdog::begin() — see
    // src/fl/wdt/watchdog.h and src/platforms/*/watchdog_*.impl.hpp.

    // Initialize RX buffer dynamically (uses PSRAM if available, falls back to heap)
    g_rx_buffer_storage.resize(RX_BUFFER_SIZE);

    // Initialize global autoresearch state
    g_autoresearch_state = fl::make_shared<AutoResearchState>();
    g_autoresearch_state->pin_tx = DEFAULT_PIN_TX;
    g_autoresearch_state->pin_rx = DEFAULT_PIN_RX;
    g_autoresearch_state->default_pin_tx = DEFAULT_PIN_TX;
    g_autoresearch_state->default_pin_rx = DEFAULT_PIN_RX;
    g_autoresearch_state->rx_buffer = g_rx_buffer_storage;
    g_autoresearch_state->rx_factory = createRxDevice;

    const char* loop_back_mode = PIN_TX == PIN_RX ? "INTERNAL" : "JUMPER WIRE";

    // Build header and platform/hardware info
    fl::sstream ss;
    ss << "\n╔════════════════════════════════════════════════════════════════╗\n";
    ss << "║ FastLED AutoResearch - Test Matrix Configuration                ║\n";
    ss << "╚════════════════════════════════════════════════════════════════╝\n";

    // Platform information
    ss << "\n[PLATFORM]\n";
    ss << "  Chip: " << autoresearch::chipName() << "\n";

    // Hardware configuration - machine-parseable for --expect autoresearch
    ss << "\n[HARDWARE]\n";
    ss << "  TX Pin: " << PIN_TX << "\n";  // --expect "TX Pin: 0"
    ss << "  RX Pin: " << PIN_RX << "\n";  // --expect "RX Pin: 1"
    ss << "  RX Device: " << fl::toString(RX_BACKEND) << "\n";
    ss << "  Loopback Mode: " << loop_back_mode << "\n";
    ss << "  Color Order: RGB\n";
    ss << "  RX Buffer Size: " << RX_BUFFER_SIZE << " bytes";
    FL_PRINT(ss.str());

    // SIMD validation suite, Wave8 expansion micro-bench, and full PARLIO
    // encode bench are RPC-driven — invoke via the `testSimd`,
    // `wave8ExpandBenchmark`, and `parlioEncodeBenchmark` handlers in
    // AutoResearchRemote.cpp. Do not add boot-time invocation blocks here;
    // if RPC routing is broken on a platform, fix the routing (see #2541).

    // ========================================================================
    // RX Channel Setup
    // ========================================================================

    ss.clear();
    ss << "\n[RX SETUP] Creating RX channel for LED autoresearch\n";
    ss << "[RX CREATE] Creating RX channel on PIN " << PIN_RX
       << " (" << (40000000 / 1000000) << "MHz, " << RX_BUFFER_SIZE << " symbols)";
    FL_PRINT(ss.str());

    g_autoresearch_state->rx_channel = createRxDevice(PIN_RX);

    if (!g_autoresearch_state->rx_channel) {
        ss.clear();
        ss << "[RX SETUP]: Failed to create RX channel\n";
        ss << "[RX SETUP]: Check that RMT peripheral is available and not in use";
        FL_ERROR(ss.str());
        FL_ERROR("Sanity check failed - RX channel creation failed");
        return;
    }

    ss.clear();
    ss << "[RX CREATE] ✓ RX channel created successfully (will be initialized with config in begin())\n";
    ss << "[RX SETUP] ✓ RX channel ready for LED autoresearch";
    FL_PRINT(ss.str());

    // PARLIO streaming validation (#2548) is now RPC-driven via the
    // `parlioStreamValidate` handler registered in AutoResearchRemote.cpp.

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
    RemoteControlSingleton::instance().registerFunctions(g_autoresearch_state);

    FL_PRINT("[REMOTE RPC] ✓ RPC system initialized (testGpioConnection available)");

    // ========================================================================
    // Async Task Setup - JSON-RPC Processing
    // ========================================================================
    FL_PRINT("[ASYNC] Setting up JSON-RPC async task (10ms interval)");
    autoresearch::setupRpcAsyncTask(RemoteControlSingleton::instance(), 10);
    FL_PRINT("[ASYNC] ✓ JSON-RPC task registered with scheduler");

    // Stub: register self-running autoresearch client (no-op on ESP32)
    autoresearch::maybeRegisterStubAutorun(RemoteControlSingleton::instance(),
                                          g_autoresearch_state);

    // ========================================================================
    // GPIO Baseline Test - Verify GPIO→GPIO path works before testing PARLIO
    // ========================================================================
    ss.clear();
    ss << "\n[GPIO BASELINE TEST] Testing GPIO " << PIN_TX << " → GPIO " << PIN_RX << " connectivity";
    FL_PRINT(ss.str());

    // GPIO baseline test moved to loop() - wait for RPC start signal before testing
    // This allows JSON-RPC commands (testGpioConnection, findConnectedPins) to run first
    FL_WARN("[GPIO BASELINE TEST] Deferred to loop() - waiting for RPC start signal");

    // Post-#2428 the channel driver registry no longer auto-populates. This
    // example needs every available driver enrolled with ChannelManager so the
    // discovery + autoresearch loop below can enumerate them.
    FastLED.enableAllDrivers();

    // List all available drivers and store globally
    g_autoresearch_state->drivers_available = FastLED.getDriverInfos();
    ss.clear();
    ss << "\n[DRIVER DISCOVERY]\n";
    ss << "  Found " << g_autoresearch_state->drivers_available.size() << " driver(s) available:\n";
    for (fl::size i = 0; i < g_autoresearch_state->drivers_available.size(); i++) {
        ss << "    " << (i+1) << ". " << g_autoresearch_state->drivers_available[i].name.c_str()
           << " (priority: " << g_autoresearch_state->drivers_available[i].priority
           << ", enabled: " << (g_autoresearch_state->drivers_available[i].enabled ? "yes" : "no") << ")\n";
    }
    FL_PRINT(ss.str());

    // Validate that expected drivers are available for this platform
    autoResearchExpectedEngines();

    // Emit JSON-RPC ready event for Python orchestration
    fl::json readyData = fl::json::object();
    readyData.set("ready", true);
    readyData.set("setupTimeMs", static_cast<int64_t>(millis()));
    readyData.set("drivers", static_cast<int64_t>(g_autoresearch_state->drivers_available.size()));
    readyData.set("pinTx", static_cast<int64_t>(PIN_TX));
    readyData.set("pinRx", static_cast<int64_t>(PIN_RX));
    printStreamRaw("ready", readyData);

    // Human-readable diagnostics (not machine-parsed)
    FL_PRINT("\n[SETUP COMPLETE] AutoResearch ready - awaiting JSON-RPC commands");
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
    // Unified watchdog guard. First iteration: arms the WDT, prints any
    // prior-boot reset/crash info, pauses 3 s on a crash. Every iteration:
    // feeds on construction (now) and again on destruction (end of scope).
    FL_WATCHDOG_AUTO(AUTORESEARCH_WATCHDOG_TIMEOUT_MS);

    // Aggressively pump async tasks (including JSON-RPC task)
    // This ensures RPC commands are processed frequently even without delay() calls
    for (int i = 0; i < 100; i++) {
        fl::task::run();
    }

    // ========================================================================
    // Watchdog autoresearch trigger (FastLED#2731) — when the host RPC sends
    // `deliberateHang`, the handler flips this flag. We let one more pass of
    // task::run() drain the response queue so the host sees the ACK, then
    // we disable interrupts (so no async machinery can keep the WDT happy)
    // and spin forever. The next watchdog tick must fire and reset the chip.
    // The host's recovery test passes if the device re-enumerates afterward
    // and is reflashable.
    // ========================================================================
    if (g_autoresearch_state->deliberate_hang_requested) {
        FL_WARN("[deliberateHang] entering forced-hang loop NOW");
        delay(200);  // give Serial TX FIFO time to flush
        // noInterrupts() is an Arduino-only macro; guard it so the host stub
        // build (which compiles this .ino for the unit-test framework) doesn't
        // hit an undeclared identifier. On host the while(1) below still spins
        // and prevents feed(), which is the actual hang we want to test.
#if !defined(FL_IS_STUB) && !defined(FL_IS_WASM)
        noInterrupts();
#endif
        while (true) { /* deliberate hang */ }
    }

    // Feed the watchdog at the end of every loop iteration. On platforms
    // where begin() was a no-op the feed is also a no-op. The mark-clean
    // shutdown zeros the consecutive-crash counter so a transient hang
    // doesn't eventually push the board into safe mode.
    FastLED.watchdog().feed();
    FastLED.watchdog().markCleanShutdown();

    // Run GPIO baseline test once after device is ready (allows JSON-RPC to be operational first)
    // This test is informational only - we continue regardless of pass/fail
    if (!g_autoresearch_state->gpio_baseline_test_done) {
        // Wait 500ms after boot to ensure JSON-RPC is fully operational
        if (millis() > 500) {
            g_autoresearch_state->gpio_baseline_test_done = true;

            FL_PRINT("\n[GPIO BASELINE TEST] Testing GPIO " << PIN_TX << " → GPIO " << PIN_RX << " connectivity");

            // Test RX channel with manual GPIO toggle to confirm hardware path works
            // This isolates GPIO/hardware issues from PARLIO driver issues
            // Buffer size = 100 symbols, hz = 40MHz (same as LED autoresearch)
            if (!testRxChannel(g_autoresearch_state->rx_channel, PIN_TX, PIN_RX, 40000000, 100)) {
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
        fl::json status = fl::json::object();
        status.set("ready", true);
        status.set("uptimeMs", static_cast<int64_t>(now));
        printStreamRaw("status", status);
        last_status_ms = now;
    }
}
