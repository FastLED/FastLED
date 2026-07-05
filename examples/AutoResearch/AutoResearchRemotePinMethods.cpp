// Pin configuration RPC bindings: get/set TX/RX/pair, GPIO connectivity
// probe, jumper-finder, debug-log toggle, help. GPIO is the implementation;
// the RPC API is "pins" (the user-facing concept).
// Extracted from AutoResearchRemote.cpp as part of #3132 / meta #3127.

#include "fl/system/sketch_macros.h"
#if !defined(FASTLED_AUTORESEARCH_LOW_MEMORY) && !FL_PLATFORM_HAS_LARGE_MEMORY
#define FASTLED_AUTORESEARCH_LOW_MEMORY 1
#endif
#if !(defined(FASTLED_AUTORESEARCH_LOW_MEMORY) && FASTLED_AUTORESEARCH_LOW_MEMORY)

// Legacy debug macros (no-ops, kept for debugTest RPC function)
#define DEBUG_PRINT(x) do {} while(0)
#define DEBUG_PRINTLN(x) do {} while(0)

#include "AutoResearchRemote.h"
#include "AutoResearchBle.h"
#include "AutoResearchNet.h"
#include "AutoResearchOta.h"
#include "fl/remote/transport/serial.h"
#include "fl/system/heap.h"
#include "Common.h"
#include "AutoResearchTest.h"
#include "AutoResearchHelpers.h"
#include "fl/stl/sstream.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/optional.h"
#include "fl/stl/json.h"
#include "fl/task/task.h"
#include "fl/task/executor.h"
#include "fl/math/wave/wave_perf_bench.h"
#include "fl/stl/atomic.h"
#include "fl/task/promise.h"
#include "fl/math/simd.h"
#include "AutoResearchSimd.h"
#include "AutoResearchAnimartrixBench.h"
#include "AutoResearchWave8Expand.h"
#include "AutoResearchParlioEncode.h"
#include "AutoResearchTimingDrift.h"
#include "AutoResearchParlioStream.h"
#include "fl/chipsets/spi.h"
#include "fl/channels/config.h"
#include <Arduino.h>

#include "fl/net/ble.h"

#include "fl/codec/h264.h"
#include "fl/codec/mp4_parser.h"
#include "fl/stl/cstdio.h"
#include "fl/stl/detail/memory_file_handle.h"
#include "fl/fx/frame.h"

namespace {

struct HelpEntry {
    const char* name;
    const char* phase;
    const char* args;
    const char* returns;
    const char* description;
};

void streamHelpEntry(fl::JsonStreamWriter& writer, const HelpEntry& entry) {
    writer.beginObject();
    writer.member("name", entry.name);
    writer.member("phase", entry.phase);
    writer.member("args", entry.args);
    writer.member("returns", entry.returns);
    writer.member("description", entry.description);
    writer.endObject();
}

static const HelpEntry kHelpEntries[] = {
    {"start", "Phase 1: Basic Control", "[]", "void", "Trigger test matrix execution"},
    {"status", "Phase 1: Basic Control", "[]", "{startReceived, testComplete, frameCounter, state}", "Query current test state"},
    {"drivers", "Phase 1: Basic Control", "[]", "[{name, priority, enabled}, ...]", "List available drivers"},
    {"getConfig", "Phase 2: Configuration", "[]", "{drivers, laneRange, stripSizes, totalTestCases}", "Query current test matrix configuration"},
    {"setDrivers", "Phase 2: Configuration", "[driver1, driver2, ...]", "{success, driversSet, testCases}", "Configure enabled drivers"},
    {"setLaneRange", "Phase 2: Configuration", "[minLanes, maxLanes]", "{success, minLanes, maxLanes, testCases}", "Configure lane range (1-16)"},
    {"setStripSizes", "Phase 2: Configuration", "[size] or [shortSize, longSize]", "{success, stripSizesSet, testCases}", "Configure strip sizes"},
    {"runTestCase", "Phase 3: Selective Execution", "[testCaseIndex]", "{success, testCaseIndex, result}", "Run single test case by index"},
    {"runDriver", "Phase 3: Selective Execution", "[driverName]", "{success, driver, testsRun, results}", "Run all tests for specific driver"},
    {"runAll", "Phase 3: Selective Execution", "[]", "{success, totalCases, passedCases, skippedCases, results}", "Run full test matrix with JSON results"},
    {"getResults", "Phase 3: Selective Execution", "[]", "[{driver, lanes, stripSize, ...}, ...]", "Return all test results"},
    {"getResult", "Phase 3: Selective Execution", "[testCaseIndex]", "{driver, lanes, stripSize, ...}", "Return specific test case result"},
    {"reset", "Phase 4: Utility", "[]", "{success, message, testCasesCleared}", "Reset test state without device reboot"},
    {"halt", "Phase 4: Utility", "[]", "{success, message}", "Trigger sketch halt"},
    {"ping", "Phase 4: Utility", "[]", "{success, message, timestamp, uptimeMs, frameCounter}", "Health check with timestamp"},
    {"getPins", "Phase 5: Pin Configuration", "[]", "{txPin, rxPin, defaults: {txPin, rxPin}, platform}", "Query current and default pin configuration"},
    {"setTxPin", "Phase 5: Pin Configuration", "[pin]", "{success, txPin, previousTxPin, testCases}", "Set TX pin (regenerates test cases)"},
    {"setRxPin", "Phase 5: Pin Configuration", "[pin]", "{success, rxPin, previousRxPin, rxChannelRecreated}", "Set RX pin (recreates RX channel)"},
    {"setPins", "Phase 5: Pin Configuration", "[{txPin, rxPin}] or [txPin, rxPin]", "{success, txPin, rxPin, rxChannelRecreated, testCases}", "Set both TX and RX pins atomically"},
    {"findConnectedPins", "Phase 5: Pin Configuration", "[{startPin, endPin, autoApply}] (all optional)", "{success, found, txPin, rxPin, autoApplied, testedPairs}", "Probe adjacent pin pairs to find jumper wire connection"},
    {"help", "Phase 4: Utility", "[]", "[{name, phase, args, returns, description}, ...]", "List all RPC functions with descriptions"},
    {"testSimd", "Phase 4: Utility", "[]", "{success, passed, totalTests, passedTests, failedTests, failures:[string]}", "Run comprehensive SIMD test suite (85 tests)"},
    {"testSimdBenchmark", "Phase 4: Utility", "[{iterations}] (optional, default 10000)", "{success, iterations, float_us, s16x16_us, simd_us}", "Benchmark multiply speed: float vs s16x16 vs s16x16x4 SIMD"},
    {"animartrixPerlinBench", "Phase 4: Utility", "[{iterations}] (optional, default 100, max 10000)", "{success, iterations, pnoise_calls_per_iter, pnoise_float_us, pnoise_i16_us, speedup_x1000}", "Animartrix-representative Perlin noise bench: scalar float pnoise vs s16x16 fixed-point pnoise2d (16x16 grid per iter, mirrors a real frame). Answers: how much does fixed-point beat float for Animartrix's hot path on this hardware?"},
    {"wave8ExpandBenchmark", "Phase 4: Utility", "[{iterations}] (optional, default 30000, max 200000)", "{success, iterations, expand_nibble_us, expand_byte_us, expand_batched_us, transpose16_nibble_us, transpose16_byte_us, sink}", "Bench PARLIO Wave8 expansion (#2526): nibble vs byte vs batched LUT, plus full per-byte-position cost (expansion + 16-lane transpose)"},
    {"ieee754CodecTest", "Phase 4: Utility", "[]", "{success, tests_run, tests_failed, first_failure, expected_bits, actual_bits}", "On-device verification of the integer-only IEEE 754 decimal codec (#3039): parse, format, round-trip, overflow/underflow, and NaN rejection without strtof/libm."},
    {"parlioEncodeBenchmark", "Phase 4: Utility", "[{iterations}] (optional, default 12000, max 200000)", "{success, iters, lanes, leds_per_lane, scratchPsramOk, outputPsramOk, perpos_ss_us, perpos_sp_us, perpos_ps_us, perpos_pp_us, frame_ss_us, frame_sp_us, frame_ps_us, frame_pp_us, sink}", "Bench full PARLIO encode hot loop (16-lane gather + BF1 pipe4 direct encode) with SRAM and optional PSRAM placements; answers PSRAM hypothesis + ISR-streaming feasibility"},
    {"timingDriftTest", "Phase 4: Utility", "[{pin, numLeds, iterations}] (all optional; default pin=4, numLeds=35, iterations=10)", "{success, pin, num_leds, iterations, cpu_mhz, show_count, show_min_us, show_max_us, show_total_us, iter_ms:[...]}", "Issue #2994 repro for compounded per-sequence timing drift on master vs 3.10.3. Replays the reporter's WS2812B-ring + millisDelay-gated fade sketch and returns per-sequence wall time (theoretical 2495 ms; on 3.10.3 rock-steady, on master 2563-2752)."},
    {"parlioStreamValidate", "Phase 4: Utility", "[{baseTxPin, txPins, numLanes, numLeds, iterations, timeoutMs}] (all optional; txPins overrides contiguous baseTxPin)", "{success, completed, baseTxPin, txPins, lanes, leds_per_lane, iterations, perIterUs:[...], steadyAvgUs, failedIter, underrunCount, txDoneCount, workerIsrCount, ringError, hardwareIdle}", "Functional test of the PARLIO ISR-chunked streaming engine (#2548). Drives N back-to-back FastLED.show() calls through the production engine (which uses BF1+pipe4 on 16-lane Wave8 since #2559) and verifies all complete within timeout. Catches hangs/stalls."},
    {"flexioRxBenchmark", "Phase 4: Utility", "[{frequency_hz=1000, duration_ms=100, tx_pin=3, rx_pin=4}] (all optional)", "{success, frequency_hz, duration_ms, tx_pin, rx_pin, edges_captured, periods, period_mean_ns, period_sigma_ns, period_min_ns, period_max_ns}", "Square-wave validation for the FlexIO RX backend (Teensy 4.x only, FastLED#2764 Phase 2). Drives tx_pin via analogWriteFrequency at 50%% duty, captures via RxBackend::FLEXIO on rx_pin, reports per-period statistics."},
    {"flexioObjectFledTest", "Phase 4: Utility", "[{test_case=0..4, tx_pin=3, rx_pin=4, capture_ms=50}] (all optional)", "{success, test_case, tx_pin, rx_pin, num_leds, expected_bytes, decoded_bytes, matched, mismatched, edges_captured}", "End-to-end ObjectFLED TX -> FlexIO RX loopback verification (Teensy 4.x only, FastLED#2764 Phase 3). Drives WS2812 patterns through Bus::FLEX_IO slot 0, captures via RxBackend::FLEXIO, decodes the bit stream, and reports byte-level match counts. Five fixed test patterns: 0=red, 1=RGB triple, 2=all zeros, 3=all ones, 4=100-LED alternating."},
};

static const fl::size kHelpEntryCount = sizeof(kHelpEntries) / sizeof(kHelpEntries[0]);

}  // namespace


void AutoResearchRemoteControl::bindPinMethods(fl::Remote& remote) {
    // Register "setDebug" function - enable/disable runtime debug logging
    remote.bind("setDebug", [this](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();

        // Validate args: expects [enabled: bool]
        if (!args.is_array() || args.size() != 1) {
            response.set("success", false);
            response.set("error", "InvalidArgs");
            response.set("message", "Expected [enabled: bool]");
            return response;
        }

        if (!args[0].is_bool()) {
            response.set("success", false);
            response.set("error", "InvalidType");
            response.set("message", "Argument must be boolean");
            return response;
        }

        bool enabled = args[0].as_bool().value();
        mState->debug_enabled = enabled;
        fl::setLogLevel(static_cast<fl::u8>(
            enabled ? fl::LogLevel::FL_LOG_LEVEL_DEBUG
                    : fl::LogLevel::FL_LOG_LEVEL_NONE));

        response.set("success", true);
        response.set("debug_enabled", enabled);
        response.set("message", enabled ? "Debug logging enabled" : "Debug logging disabled");

        return response;
    });

    // Register "testGpioConnection" function - test if TX and RX pins are electrically connected
    // This is a pre-test to diagnose hardware connection issues before running validation
    remote.bind("testGpioConnection", [](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();

        // Validate args: expects [txPin, rxPin]
        if (!args.is_array() || args.size() != 2) {
            response.set("error", "InvalidArgs");
            response.set("message", "Expected [txPin, rxPin]");
            return response;
        }

        if (!args[0].is_int() || !args[1].is_int()) {
            response.set("error", "InvalidPinType");
            response.set("message", "Pin numbers must be integers");
            return response;
        }

        int tx_pin = static_cast<int>(args[0].as_int().value());
        int rx_pin = static_cast<int>(args[1].as_int().value());

        // Test 1: TX drives LOW, RX has pullup → RX should read LOW if connected
        pinMode(tx_pin, OUTPUT);
        pinMode(rx_pin, INPUT_PULLUP);
        digitalWrite(tx_pin, LOW);
        delay(5);  // Allow signal to settle
        int rx_when_tx_low = digitalRead(rx_pin);

        // Test 2: TX drives HIGH → RX should read HIGH if connected
        digitalWrite(tx_pin, HIGH);
        delay(5);  // Allow signal to settle
        int rx_when_tx_high = digitalRead(rx_pin);

        // Restore pins to safe state
        pinMode(tx_pin, INPUT);
        pinMode(rx_pin, INPUT);

        // Analyze results
        bool connected = (rx_when_tx_low == LOW) && (rx_when_tx_high == HIGH);

        response.set("txPin", static_cast<int64_t>(tx_pin));
        response.set("rxPin", static_cast<int64_t>(rx_pin));
        response.set("rxWhenTxLow", rx_when_tx_low == LOW ? "LOW" : "HIGH");
        response.set("rxWhenTxHigh", rx_when_tx_high == HIGH ? "HIGH" : "LOW");
        response.set("connected", connected);

        if (connected) {
            response.set("success", true);
            response.set("message", "GPIO pins are connected");
        } else {
            response.set("success", false);
            if (rx_when_tx_low == HIGH && rx_when_tx_high == HIGH) {
                response.set("message", "RX pin stuck HIGH - no connection detected (check jumper wire)");
            } else if (rx_when_tx_low == LOW && rx_when_tx_high == LOW) {
                response.set("message", "RX pin stuck LOW - possible short to ground");
            } else {
                response.set("message", "Unexpected GPIO behavior - check wiring");
            }
        }

        return response;
    });

    // ========================================================================
    // Pin Configuration RPC Functions (Dynamic TX/RX Pin Support)
    // ========================================================================

    // Register "getPins" function - query current and default pin configuration
    remote.bind("getPins", [this](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();
        response.set("success", true);
        response.set("txPin", static_cast<int64_t>(mState->pin_tx));
        response.set("rxPin", static_cast<int64_t>(mState->pin_rx));

        fl::json defaults = fl::json::object();
        defaults.set("txPin", static_cast<int64_t>(mState->default_pin_tx));
        defaults.set("rxPin", static_cast<int64_t>(mState->default_pin_rx));
        response.set("defaults", defaults);

        #if defined(FL_IS_ESP_32S3)
            response.set("platform", "ESP32-S3");
        #elif defined(FL_IS_ESP_32S2)
            response.set("platform", "ESP32-S2");
        #elif defined(FL_IS_ESP_32C6)
            response.set("platform", "ESP32-C6");
        #elif defined(FL_IS_ESP_32C3)
            response.set("platform", "ESP32-C3");
        #else
            response.set("platform", "unknown");
        #endif

        return response;
    });

    // Register "setTxPin" function - set TX pin (regenerates test cases)
    remote.bind("setTxPin", [this](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();

        if (!args.is_array() || args.size() != 1 || !args[0].is_int()) {
            response.set("error", "InvalidArgs");
            response.set("message", "Expected [pin: int]");
            return response;
        }

        int new_pin = static_cast<int>(args[0].as_int().value());

        // Validate pin range (ESP32 GPIO range)
        if (new_pin < 0 || new_pin > 48) {
            response.set("error", "InvalidPin");
            response.set("message", "Pin must be 0-48");
            return response;
        }

        int old_pin = mState->pin_tx;
        mState->pin_tx = new_pin;

        response.set("success", true);
        response.set("txPin", static_cast<int64_t>(new_pin));
        response.set("previousTxPin", static_cast<int64_t>(old_pin));
        return response;
    });

    // Register "setRxPin" function - set RX pin (recreates RX channel)
    remote.bind("setRxPin", [this](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();

        if (!args.is_array() || args.size() != 1 || !args[0].is_int()) {
            response.set("error", "InvalidArgs");
            response.set("message", "Expected [pin: int]");
            return response;
        }

        int new_pin = static_cast<int>(args[0].as_int().value());

        // Validate pin range (ESP32 GPIO range)
        if (new_pin < 0 || new_pin > 48) {
            response.set("error", "InvalidPin");
            response.set("message", "Pin must be 0-48");
            return response;
        }

        int old_pin = mState->pin_rx;
        bool pin_changed = (new_pin != old_pin);
        bool rx_recreated = false;

        if (pin_changed) {
            mState->pin_rx = new_pin;

            // Recreate RX channel with new pin
            // Destroy old RX channel
            mState->rx_channel.reset();

            // Create new RX channel on new pin using factory
            mState->rx_channel = mState->rx_factory(new_pin);

            if (mState->rx_channel) {
                rx_recreated = true;
            } else {
                response.set("error", "RxChannelCreationFailed");
                response.set("message", "Failed to create RX channel on new pin");
                response.set("pinRx", static_cast<int64_t>(new_pin));
                // Restore old pin value
                mState->pin_rx = old_pin;
                return response;
            }
        }

        response.set("success", true);
        response.set("rxPin", static_cast<int64_t>(new_pin));
        response.set("previousRxPin", static_cast<int64_t>(old_pin));
        response.set("rxChannelRecreated", rx_recreated);
        return response;
    });

    // Register "setPins" function - set both TX and RX pins atomically
    remote.bind("setPins", [this](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();

        // Accept either {txPin, rxPin} object or [txPin, rxPin] array
        int new_tx_pin = -1;
        int new_rx_pin = -1;

        if (args.is_array() && args.size() == 1 && args[0].is_object()) {
            // Object form: [{txPin: int, rxPin: int}]
            fl::json config = args[0];
            if (config.contains("txPin") && config["txPin"].is_int()) {
                new_tx_pin = static_cast<int>(config["txPin"].as_int().value());
            }
            if (config.contains("rxPin") && config["rxPin"].is_int()) {
                new_rx_pin = static_cast<int>(config["rxPin"].as_int().value());
            }
        } else if (args.is_array() && args.size() == 2) {
            // Array form: [txPin, rxPin]
            if (args[0].is_int()) {
                new_tx_pin = static_cast<int>(args[0].as_int().value());
            }
            if (args[1].is_int()) {
                new_rx_pin = static_cast<int>(args[1].as_int().value());
            }
        } else {
            response.set("error", "InvalidArgs");
            response.set("message", "Expected [{txPin, rxPin}] or [txPin, rxPin]");
            return response;
        }

        // Validate pin ranges
        if (new_tx_pin < 0 || new_tx_pin > 48) {
            response.set("error", "InvalidTxPin");
            response.set("message", "TX pin must be 0-48");
            return response;
        }
        if (new_rx_pin < 0 || new_rx_pin > 48) {
            response.set("error", "InvalidRxPin");
            response.set("message", "RX pin must be 0-48");
            return response;
        }

        int old_tx_pin = mState->pin_tx;
        int old_rx_pin = mState->pin_rx;
        bool rx_pin_changed = (new_rx_pin != old_rx_pin);
        bool rx_recreated = false;

        // Update TX pin
        mState->pin_tx = new_tx_pin;

        // Update RX pin and recreate channel if changed
        if (rx_pin_changed) {
            mState->pin_rx = new_rx_pin;

            // Destroy old RX channel
            mState->rx_channel.reset();

            // Create new RX channel using factory
            mState->rx_channel = mState->rx_factory(new_rx_pin);

            if (mState->rx_channel) {
                rx_recreated = true;
            } else {
                // Rollback both pins
                mState->pin_tx = old_tx_pin;
                mState->pin_rx = old_rx_pin;
                response.set("error", "RxChannelCreationFailed");
                response.set("message", "Failed to create RX channel - pins restored to previous values");
                response.set("pinRx", static_cast<int64_t>(new_rx_pin));
                return response;
            }
        } else {
            mState->pin_rx = new_rx_pin;
        }

        response.set("success", true);
        response.set("txPin", static_cast<int64_t>(new_tx_pin));
        response.set("rxPin", static_cast<int64_t>(new_rx_pin));
        response.set("previousTxPin", static_cast<int64_t>(old_tx_pin));
        response.set("previousRxPin", static_cast<int64_t>(old_rx_pin));
        response.set("rxChannelRecreated", rx_recreated);
        return response;
    });

    // Register "findConnectedPins" function - probe adjacent pin pairs to find a jumper wire connection
    // This allows automatic discovery of TX/RX pin pair without requiring user to specify them
    remote.bind("findConnectedPins", [this](const fl::json& args) -> fl::json {
        return findConnectedPinsImpl(args);
    });

    // Register "help" function - list all RPC functions with descriptions
    remote.bindStreaming("help", [](fl::JsonStreamWriter& writer, const fl::json& args, const fl::json& requestId) {
        (void)args;
        (void)requestId;

        writer.beginObject();
        writer.member("success", true);
        writer.member("totalFunctions", static_cast<fl::i64>(kHelpEntryCount));
        writer.key("functions");
        writer.beginArray();
        for (fl::size i = 0; i < kHelpEntryCount; ++i) {
            streamHelpEntry(writer, kHelpEntries[i]);
        }
        writer.endArray();
        writer.endObject();
    });
}

#endif // !(FASTLED_AUTORESEARCH_LOW_MEMORY)
