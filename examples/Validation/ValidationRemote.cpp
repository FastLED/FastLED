// examples/Validation/ValidationRemote.cpp
//
// Remote RPC control system implementation for Validation sketch.
//
// ARCHITECTURE:
// - RPC responses use printJsonRaw()/printStreamRaw() which bypass fl::println
// - Test execution wrapped in fl::ScopedLogDisable to suppress debug noise
// - This provides clean, parseable JSON output without FL_DBG/FL_PRINT spam

// Legacy debug macros (no-ops, kept for debugTest RPC function)
#define DEBUG_PRINT(x) do {} while(0)
#define DEBUG_PRINTLN(x) do {} while(0)

#include "ValidationRemote.h"
#include "ValidationBle.h"
#include "ValidationNet.h"
#include "ValidationOta.h"
#include "fl/remote/transport/serial.h"
#include "fl/memory.h"
#include "Common.h"
#include "ValidationTest.h"
#include "ValidationHelpers.h"
#include "fl/stl/sstream.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/optional.h"
#include "fl/stl/json.h"
#include "fl/stl/task.h"
#include "fl/stl/async.h"
#include "fl/stl/atomic.h"
#include "fl/promise.h"
#include "fl/simd.h"
#include "ValidationSimd.h"
#include "fl/memory.h"
#include <Arduino.h>

#include "fl/stl/asio/ble.h"

// Codec headers for decodeFile RPC
#include "fl/codec/h264.h"
#include "fl/codec/mp4_parser.h"
#include "fl/stl/detail/memory_file_handle.h"
#include "fl/fx/frame.h"

// ============================================================================
// Raw Serial Output Functions (bypass fl::println and ScopedLogDisable)
// ============================================================================

void printJsonRaw(const fl::json& json, const char* prefix) {
    // Serialize and print response
    fl::string formatted = fl::formatJsonResponse(json, prefix);
    fl::println(formatted.c_str());
    fl::flush();
}

void printStreamRaw(const char* messageType, const fl::json& data) {
    // Build pure JSONL message: RESULT: {"type":"...", ...data}
    fl::json output = fl::json::object();
    output.set("type", messageType);

    // Copy all fields from data into output
    if (data.is_object()) {
        auto keys = data.keys();
        for (fl::size i = 0; i < keys.size(); i++) {
            output.set(keys[i].c_str(), data[keys[i]]);
        }
    }

    // Use fl:: serial transport for consistent formatting
    fl::string formatted = fl::formatJsonResponse(output, "RESULT: ");
    fl::println(formatted.c_str());
}

// ============================================================================
// Standard JSON-RPC Response Format (Phase 4 Refactoring)
// ============================================================================
// Return codes:
//   0 = SUCCESS
//   1 = TEST_FAILED
//   2 = HARDWARE_ERROR (GPIO not connected)
//   3 = INVALID_ARGS

enum class ReturnCode : int {
    SUCCESS = 0,
    TEST_FAILED = 1,
    HARDWARE_ERROR = 2,
    INVALID_ARGS = 3
};

fl::json makeResponse(bool success, ReturnCode returnCode, const char* message,
                      const fl::json& data = fl::json()) {
    fl::json r = fl::json::object();
    r.set("success", success);
    r.set("returnCode", static_cast<int64_t>(static_cast<int>(returnCode)));
    r.set("message", message);
    if (!data.is_null() && data.has_value()) {
        r.set("data", data);
    }
    return r;
}

// No forward declarations needed - using one-test-per-RPC architecture

// ============================================================================
// ValidationRemoteControl Private Helper Functions
// ============================================================================

fl::json ValidationRemoteControl::runSingleTestImpl(const fl::json& args) {
    fl::json response = fl::json::object();

    // RPC system unwraps single-element arrays, so args is the config object directly
    if (!args.is_object()) {
        response.set("success", false);
        response.set("error", "InvalidArgs");
        response.set("message", "Expected {driver, laneSizes, pattern?, iterations?, pinTx?, pinRx?, timing?}");
        return response;
    }

    fl::json config = args;

    // ========== REQUIRED PARAMETERS ==========

    // 1. Extract driver (required)
    if (!config.contains("driver") || !config["driver"].is_string()) {
        response.set("success", false);
        response.set("error", "MissingDriver");
        response.set("message", "Required field 'driver' (string) missing");
        return response;
    }
    fl::string driver_name = config["driver"].as_string().value();

    // Validate driver exists
    bool driver_found = false;
    for (fl::size i = 0; i < mState->drivers_available.size(); i++) {
        if (mState->drivers_available[i].name == driver_name) {
            driver_found = true;
            break;
        }
    }
    if (!driver_found) {
        response.set("success", false);
        response.set("error", "UnknownDriver");
        fl::sstream msg;
        msg << "Driver '" << driver_name.c_str() << "' not available";
        response.set("message", msg.str().c_str());
        return response;
    }

    // 2. Extract laneSizes (required)
    if (!config.contains("laneSizes") || !config["laneSizes"].is_array()) {
        response.set("success", false);
        response.set("error", "MissingLaneSizes");
        response.set("message", "Required field 'laneSizes' (array) missing");
        return response;
    }

    fl::json lane_sizes_json = config["laneSizes"];
    if (lane_sizes_json.size() == 0 || lane_sizes_json.size() > 8) {
        response.set("success", false);
        response.set("error", "InvalidLaneCount");
        response.set("message", "laneSizes must have 1-8 elements");
        return response;
    }

    fl::vector<int> lane_sizes;
    const int max_leds_per_lane = mState->rx_buffer.size() / 32;
    for (fl::size i = 0; i < lane_sizes_json.size(); i++) {
        if (!lane_sizes_json[i].is_int()) {
            response.set("success", false);
            response.set("error", "InvalidLaneSizeType");
            fl::sstream msg;
            msg << "laneSizes[" << i << "] must be integer";
            response.set("message", msg.str().c_str());
            return response;
        }
        int size = static_cast<int>(lane_sizes_json[i].as_int().value());
        if (size <= 0) {
            response.set("success", false);
            response.set("error", "InvalidLaneSize");
            fl::sstream msg;
            msg << "laneSizes[" << i << "] = " << size << " must be > 0";
            response.set("message", msg.str().c_str());
            return response;
        }
        if (size > max_leds_per_lane) {
            response.set("success", false);
            response.set("error", "LaneSizeTooLarge");
            fl::sstream msg;
            msg << "laneSizes[" << i << "] = " << size << " exceeds max " << max_leds_per_lane;
            response.set("message", msg.str().c_str());
            return response;
        }
        lane_sizes.push_back(size);
    }

    // ========== OPTIONAL PARAMETERS ==========

    // 3. Extract pattern (optional, default: "MSB_LSB_A")
    fl::string pattern = "MSB_LSB_A";
    if (config.contains("pattern") && config["pattern"].is_string()) {
        pattern = config["pattern"].as_string().value();
    }

    // 4. Extract iterations (optional, default: 1)
    int iterations = 1;
    if (config.contains("iterations") && config["iterations"].is_int()) {
        iterations = static_cast<int>(config["iterations"].as_int().value());
        if (iterations < 1) {
            response.set("success", false);
            response.set("error", "InvalidIterations");
            response.set("message", "iterations must be >= 1");
            return response;
        }
    }

    // 5. Extract pinTx (optional, default: use mState->pin_tx)
    int pin_tx = mState->pin_tx;
    if (config.contains("pinTx") && config["pinTx"].is_int()) {
        pin_tx = static_cast<int>(config["pinTx"].as_int().value());
        if (pin_tx < 0 || pin_tx > 48) {
            response.set("success", false);
            response.set("error", "InvalidPinTx");
            response.set("message", "pinTx must be 0-48");
            return response;
        }
    }

    // 6. Extract pinRx (optional, default: use mState->pin_rx)
    int pin_rx = mState->pin_rx;
    if (config.contains("pinRx") && config["pinRx"].is_int()) {
        pin_rx = static_cast<int>(config["pinRx"].as_int().value());
        if (pin_rx < 0 || pin_rx > 48) {
            response.set("success", false);
            response.set("error", "InvalidPinRx");
            response.set("message", "pinRx must be 0-48");
            return response;
        }
    }

    // 7. Extract timing (optional, default: "WS2812B-V5")
    fl::string timing_name = "WS2812B-V5";
    if (config.contains("timing") && config["timing"].is_string()) {
        timing_name = config["timing"].as_string().value();
    }

    // 8. Extract useLegacyApi (optional, default: false)
    bool use_legacy_api = false;
    if (config.contains("useLegacyApi") && config["useLegacyApi"].is_bool()) {
        use_legacy_api = config["useLegacyApi"].as_bool().value();
    }

    // Legacy API: all pins must be in range 0-8 (compile-time template range)
    // Multi-lane uses consecutive pins starting at pin_tx
    if (use_legacy_api) {
        int max_pin = pin_tx + (int)lane_sizes.size() - 1;
        if (pin_tx < 0 || max_pin > 8) {
            response.set("success", false);
            response.set("error", "LegacyApiPinRange");
            fl::sstream msg;
            msg << "Legacy template API requires all pins in range 0-8, got pins "
                << pin_tx << "-" << max_pin;
            response.set("message", msg.str().c_str());
            return response;
        }
    }

    // ========== EXECUTION ==========

    uint32_t start_ms = millis();

    // Set driver as exclusive
    if (!FastLED.setExclusiveDriver(driver_name.c_str())) {
        response.set("success", false);
        response.set("error", "DriverSetupFailed");
        fl::sstream msg;
        msg << "Failed to set " << driver_name.c_str() << " as exclusive driver";
        response.set("message", msg.str().c_str());
        return response;
    }

    // Get timing configuration
    // Legacy API: WS2812B<PIN> template uses TIMING_WS2812_800KHZ (T1=250, T2=625, T3=375)
    // Channel API: Uses timing_name from RPC (default: WS2812B-V5)
    // RX decode timing MUST match actual TX timing for correct capture
    fl::ChipsetTimingConfig resolved_timing;
    if (use_legacy_api) {
        resolved_timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
        timing_name = "WS2812-800KHZ";
    } else if (timing_name == "UCS7604-800KHZ") {
        resolved_timing = fl::makeTimingConfig<fl::TIMING_UCS7604_800KHZ>();
    } else {
        resolved_timing = fl::makeTimingConfig<fl::TIMING_WS2812B_V5>();
    }
    fl::NamedTimingConfig timing_config(resolved_timing, timing_name.c_str());

    // Dynamically allocate LED arrays for each lane
    fl::vector<fl::unique_ptr<fl::vector<CRGB>>> led_arrays;
    fl::vector<fl::ChannelConfig> tx_configs;

    for (fl::size i = 0; i < lane_sizes.size(); i++) {
        auto leds = fl::make_unique<fl::vector<CRGB>>(lane_sizes[i]);
        tx_configs.push_back(fl::ChannelConfig(
            pin_tx + i,  // Consecutive pins for multi-lane
            timing_config.timing,
            fl::span<CRGB>(leds->data(), leds->size()),
            RGB  // Default color order
        ));
        led_arrays.push_back(fl::move(leds));
    }

    // Create temporary RX channel if pinRx differs from default
    fl::shared_ptr<fl::RxDevice> rx_channel_to_use = mState->rx_channel;

    if (pin_rx != mState->pin_rx && mState->rx_factory) {
        rx_channel_to_use = mState->rx_factory(pin_rx);
        if (!rx_channel_to_use) {
            response.set("success", false);
            response.set("error", "RxChannelCreationFailed");
            response.set("message", "Failed to create RX channel on custom pin");
            return response;
        }
    }

    // Create validation configuration
    fl::ValidationConfig validation_config(
        timing_config.timing,
        timing_config.name,
        tx_configs,
        driver_name.c_str(),
        rx_channel_to_use,
        mState->rx_buffer,
        lane_sizes[0],  // base_strip_size (used for logging)
        fl::RxDeviceType::RMT  // Default RX device type
    );

    // Run test with debug output suppressed
    int total_tests = 0;
    int passed_tests = 0;
    bool passed = false;
    uint32_t show_duration_ms = 0;
    fl::vector<fl::RunResult> run_results;

    {
        // Note: ScopedLogDisable removed to enable diagnostic output during test execution.
        // This allows capture/decode debug messages (e.g., RX timing, edge dumps) to appear
        // on serial, which is critical for diagnosing PARLIO failures at different LED counts.

        if (use_legacy_api) {
            // Legacy API path: WS2812B<PIN> template instantiation
            for (int iter = 0; iter < iterations; iter++) {
                int iter_total = 0, iter_passed = 0;
                validateChipsetTimingLegacy(validation_config, iter_total, iter_passed, show_duration_ms, &run_results);
                total_tests += iter_total;
                passed_tests += iter_passed;
            }
        } else {
            // Channel API path: FastLED.add(channel_config)
            for (int iter = 0; iter < iterations; iter++) {
                int iter_total = 0, iter_passed = 0;
                validateChipsetTiming(validation_config, iter_total, iter_passed, show_duration_ms, &run_results);
                total_tests += iter_total;
                passed_tests += iter_passed;
            }
        }

        passed = (total_tests > 0) && (passed_tests == total_tests);
    }

    uint32_t duration_ms = millis() - start_ms;

    // ========== RESPONSE ==========
    response.set("success", true);
    response.set("passed", passed);
    response.set("totalTests", static_cast<int64_t>(total_tests));
    response.set("passedTests", static_cast<int64_t>(passed_tests));
    response.set("duration_ms", static_cast<int64_t>(duration_ms));
    response.set("show_duration_ms", static_cast<int64_t>(show_duration_ms));
    response.set("driver", driver_name.c_str());
    response.set("laneCount", static_cast<int64_t>(lane_sizes.size()));

    fl::json sizes_response = fl::json::array();
    for (int size : lane_sizes) {
        sizes_response.push_back(static_cast<int64_t>(size));
    }
    response.set("laneSizes", sizes_response);
    response.set("pattern", pattern.c_str());
    response.set("useLegacyApi", use_legacy_api);

    // Free run_results before building response to reclaim heap
    // Only serialize pattern details when tests FAIL (saves heap on passing tests)
    if (!passed && !run_results.empty()) {
        fl::json patterns = fl::json::array();
        for (fl::size ri = 0; ri < run_results.size(); ri++) {
            const auto& rr = run_results[ri];
            if (rr.passed) continue;  // Skip passing patterns
            fl::json pat = fl::json::object();
            pat.set("totalLeds", static_cast<int64_t>(rr.total_leds));
            pat.set("mismatchedLeds", static_cast<int64_t>(rr.mismatches));
            pat.set("mismatchedBytes", static_cast<int64_t>(rr.mismatchedBytes));
            pat.set("lsbOnlyErrors", static_cast<int64_t>(rr.lsbOnlyErrors));

            // Serialize first N LED errors (limit to prevent OOM on small MCUs)
            constexpr fl::size kMaxSerializedErrors = 5;
            if (!rr.errors.empty()) {
                fl::json errs = fl::json::array();
                const fl::size errLimit = rr.errors.size() < kMaxSerializedErrors
                                              ? rr.errors.size()
                                              : kMaxSerializedErrors;
                for (fl::size ei = 0; ei < errLimit; ei++) {
                    const auto& e = rr.errors[ei];
                    fl::json err = fl::json::object();
                    err.set("led", static_cast<int64_t>(e.led_index));
                    fl::json expected = fl::json::array();
                    expected.push_back(static_cast<int64_t>(e.expected_r));
                    expected.push_back(static_cast<int64_t>(e.expected_g));
                    expected.push_back(static_cast<int64_t>(e.expected_b));
                    err.set("expected", expected);
                    fl::json actual = fl::json::array();
                    actual.push_back(static_cast<int64_t>(e.actual_r));
                    actual.push_back(static_cast<int64_t>(e.actual_g));
                    actual.push_back(static_cast<int64_t>(e.actual_b));
                    err.set("actual", actual);
                    errs.push_back(err);
                }
                pat.set("errors", errs);
                pat.set("totalErrors", static_cast<int64_t>(rr.errors.size()));
            }
            patterns.push_back(pat);
        }
        response.set("patterns", patterns);
    }
    run_results.clear();  // Free memory before serialization

    // Return the response — the lambda wrapper will call sendAsyncResponse
    return response;
}

fl::json ValidationRemoteControl::runParallelTestImpl(const fl::json& args) {
    fl::json response = fl::json::object();

    // Expects: {drivers: [{driver: "PARLIO", laneSizes: [100]}, {driver: "LCD_RGB", laneSizes: [100]}],
    //           pattern?: "MSB_LSB_A", iterations?: 1, timing?: "WS2812B-V5"}
    if (!args.is_object()) {
        response.set("success", false);
        response.set("error", "InvalidArgs");
        response.set("message", "Expected {drivers: [{driver, laneSizes}, ...]}");
        return response;
    }

    fl::json config = args;

    // 1. Extract drivers array (required)
    if (!config.contains("drivers") || !config["drivers"].is_array()) {
        response.set("success", false);
        response.set("error", "MissingDrivers");
        response.set("message", "Required field 'drivers' (array of {driver, laneSizes}) missing");
        return response;
    }

    fl::json drivers_json = config["drivers"];
    if (drivers_json.size() < 2) {
        response.set("success", false);
        response.set("error", "TooFewDrivers");
        response.set("message", "Parallel test requires at least 2 drivers");
        return response;
    }

    // 2. Extract shared optional parameters
    fl::string pattern = "MSB_LSB_A";
    if (config.contains("pattern") && config["pattern"].is_string()) {
        pattern = config["pattern"].as_string().value();
    }

    int iterations = 1;
    if (config.contains("iterations") && config["iterations"].is_int()) {
        iterations = static_cast<int>(config["iterations"].as_int().value());
        if (iterations < 1) iterations = 1;
    }

    fl::string timing_name = "WS2812B-V5";
    if (config.contains("timing") && config["timing"].is_string()) {
        timing_name = config["timing"].as_string().value();
    }

    // Get timing configuration
    fl::ChipsetTimingConfig resolved_timing;
    if (timing_name == "UCS7604-800KHZ") {
        resolved_timing = fl::makeTimingConfig<fl::TIMING_UCS7604_800KHZ>();
    } else {
        resolved_timing = fl::makeTimingConfig<fl::TIMING_WS2812B_V5>();
    }
    fl::NamedTimingConfig timing_config(resolved_timing, timing_name.c_str());

    // 3. Parse each driver entry and validate
    struct DriverEntry {
        fl::string name;
        fl::vector<int> lane_sizes;
        int pin_tx;
    };
    fl::vector<DriverEntry> driver_entries;

    int next_pin = mState->pin_tx;  // Start from the configured TX pin
    for (fl::size i = 0; i < drivers_json.size(); i++) {
        if (!drivers_json[i].is_object()) {
            response.set("success", false);
            response.set("error", "InvalidDriverEntry");
            fl::sstream msg;
            msg << "drivers[" << i << "] must be an object {driver, laneSizes}";
            response.set("message", msg.str().c_str());
            return response;
        }

        fl::json entry = drivers_json[i];

        // Validate driver name
        if (!entry.contains("driver") || !entry["driver"].is_string()) {
            response.set("success", false);
            response.set("error", "MissingDriverName");
            fl::sstream msg;
            msg << "drivers[" << i << "] missing 'driver' (string) field";
            response.set("message", msg.str().c_str());
            return response;
        }
        fl::string driver_name = entry["driver"].as_string().value();

        // Validate driver exists
        bool driver_found = false;
        for (fl::size j = 0; j < mState->drivers_available.size(); j++) {
            if (mState->drivers_available[j].name == driver_name) {
                driver_found = true;
                break;
            }
        }
        if (!driver_found) {
            response.set("success", false);
            response.set("error", "UnknownDriver");
            fl::sstream msg;
            msg << "Driver '" << driver_name.c_str() << "' not available";
            response.set("message", msg.str().c_str());
            return response;
        }

        // Validate laneSizes
        if (!entry.contains("laneSizes") || !entry["laneSizes"].is_array()) {
            response.set("success", false);
            response.set("error", "MissingLaneSizes");
            fl::sstream msg;
            msg << "drivers[" << i << "] missing 'laneSizes' (array) field";
            response.set("message", msg.str().c_str());
            return response;
        }

        fl::json lane_sizes_json = entry["laneSizes"];
        fl::vector<int> lane_sizes;
        for (fl::size li = 0; li < lane_sizes_json.size(); li++) {
            if (!lane_sizes_json[li].is_int()) {
                response.set("success", false);
                response.set("error", "InvalidLaneSizeType");
                return response;
            }
            int size = static_cast<int>(lane_sizes_json[li].as_int().value());
            if (size <= 0) {
                response.set("success", false);
                response.set("error", "InvalidLaneSize");
                return response;
            }
            lane_sizes.push_back(size);
        }

        // Extract optional pinTx per driver (default: auto-assign consecutive pins)
        int pin_tx = next_pin;
        if (entry.contains("pinTx") && entry["pinTx"].is_int()) {
            pin_tx = static_cast<int>(entry["pinTx"].as_int().value());
        }

        DriverEntry de;
        de.name = driver_name;
        de.lane_sizes = lane_sizes;
        de.pin_tx = pin_tx;
        driver_entries.push_back(de);

        // Advance pin for next driver (skip past this driver's lanes)
        next_pin = pin_tx + (int)lane_sizes.size();
    }

    // ========== EXECUTION ==========
    uint32_t start_ms = millis();

    // Step 1: Enable all requested drivers (not exclusive)
    // First disable all, then enable only the ones we want
    for (fl::size i = 0; i < mState->drivers_available.size(); i++) {
        FastLED.setDriverEnabled(mState->drivers_available[i].name.c_str(), false);
    }
    for (fl::size i = 0; i < driver_entries.size(); i++) {
        FastLED.setDriverEnabled(driver_entries[i].name.c_str(), true);
    }

    // Step 2: Create channels for each driver using affinity binding
    fl::vector<fl::unique_ptr<fl::vector<CRGB>>> all_led_arrays;
    fl::vector<fl::ChannelPtr> all_channels;

    for (fl::size di = 0; di < driver_entries.size(); di++) {
        const auto& de = driver_entries[di];
        for (fl::size li = 0; li < de.lane_sizes.size(); li++) {
            auto leds = fl::make_unique<fl::vector<CRGB>>(de.lane_sizes[li]);

            // Set up channel with driver affinity
            fl::ChannelOptions opts;
            opts.mAffinity = de.name;

            fl::ChannelConfig channel_config(
                de.pin_tx + (int)li,  // Consecutive pins per lane
                timing_config.timing,
                fl::span<CRGB>(leds->data(), leds->size()),
                RGB,
                opts
            );

            auto channel = FastLED.add(channel_config);
            if (!channel) {
                // Clean up already-created channels
                FastLED.clear(ClearFlags::CHANNELS);
                response.set("success", false);
                response.set("error", "ChannelCreationFailed");
                fl::sstream msg;
                msg << "Failed to create channel for driver '" << de.name.c_str()
                    << "' lane " << li;
                response.set("message", msg.str().c_str());
                return response;
            }

            all_channels.push_back(channel);
            all_led_arrays.push_back(fl::move(leds));
        }
    }

    // Step 3: Set LED data patterns and call show()
    // Use a simple pattern: fill each driver's LEDs with a known color
    int array_idx = 0;
    for (fl::size di = 0; di < driver_entries.size(); di++) {
        const auto& de = driver_entries[di];
        for (fl::size li = 0; li < de.lane_sizes.size(); li++) {
            auto& leds = *all_led_arrays[array_idx];
            for (int led = 0; led < de.lane_sizes[li]; led++) {
                // Pattern: alternate colors per driver for visual distinction
                if (di == 0) {
                    leds[led] = CRGB(0xFF, 0x00, 0x00);  // Red for first driver
                } else {
                    leds[led] = CRGB(0x00, 0xFF, 0x00);  // Green for second driver
                }
            }
            array_idx++;
        }
    }

    // Step 4: Call show() - both drivers transmit simultaneously
    bool show_success = true;
    uint32_t show_start = micros();
    for (int iter = 0; iter < iterations; iter++) {
        FastLED.show();
        FastLED.wait(5000);  // 5 second timeout for DMA completion
    }
    uint32_t show_duration_us = micros() - show_start;

    // Step 5: Validate first driver's output via RX loopback (if available)
    // Only the first driver (PARLIO) is typically connected to the RX pin
    bool rx_validation_passed = true;
    bool rx_validation_attempted = false;

    if (mState->rx_channel && driver_entries.size() > 0) {
        const auto& primary_driver = driver_entries[0];

        // Only attempt RX validation if the primary driver's pin matches our TX pin
        if (primary_driver.pin_tx == mState->pin_tx) {
            rx_validation_attempted = true;

            // Create validation config for the primary driver
            fl::vector<fl::ChannelConfig> tx_configs;
            int led_array_offset = 0;
            for (fl::size li = 0; li < primary_driver.lane_sizes.size(); li++) {
                tx_configs.push_back(fl::ChannelConfig(
                    primary_driver.pin_tx + (int)li,
                    timing_config.timing,
                    fl::span<CRGB>(all_led_arrays[led_array_offset + li]->data(),
                                   all_led_arrays[led_array_offset + li]->size()),
                    RGB
                ));
            }

            fl::ValidationConfig validation_config(
                timing_config.timing,
                timing_config.name,
                tx_configs,
                primary_driver.name.c_str(),
                mState->rx_channel,
                mState->rx_buffer,
                primary_driver.lane_sizes[0],
                fl::RxDeviceType::RMT
            );

            int total_tests = 0;
            int passed_tests = 0;
            uint32_t val_show_duration_ms = 0;

            validateChipsetTiming(validation_config, total_tests, passed_tests,
                                  val_show_duration_ms, nullptr);

            rx_validation_passed = (total_tests > 0) && (passed_tests == total_tests);
        }
    }

    // Step 6: Clean up channels
    FastLED.clear(ClearFlags::CHANNELS);

    uint32_t duration_ms = millis() - start_ms;

    // ========== RESPONSE ==========
    response.set("success", true);
    response.set("passed", show_success && rx_validation_passed);
    response.set("duration_ms", static_cast<int64_t>(duration_ms));
    response.set("show_duration_us", static_cast<int64_t>(show_duration_us));
    response.set("iterations", static_cast<int64_t>(iterations));
    response.set("rx_validation_attempted", rx_validation_attempted);
    response.set("rx_validation_passed", rx_validation_passed);

    // List drivers tested
    fl::json drivers_tested = fl::json::array();
    for (fl::size i = 0; i < driver_entries.size(); i++) {
        fl::json drv = fl::json::object();
        drv.set("driver", driver_entries[i].name.c_str());
        drv.set("pinTx", static_cast<int64_t>(driver_entries[i].pin_tx));
        fl::json sizes = fl::json::array();
        for (int s : driver_entries[i].lane_sizes) {
            sizes.push_back(static_cast<int64_t>(s));
        }
        drv.set("laneSizes", sizes);
        drv.set("laneCount", static_cast<int64_t>(driver_entries[i].lane_sizes.size()));
        drivers_tested.push_back(drv);
    }
    response.set("drivers", drivers_tested);
    response.set("pattern", pattern.c_str());

    return response;
}

fl::json ValidationRemoteControl::findConnectedPinsImpl(const fl::json& args) {
    fl::json response = fl::json::object();

    // Parse optional arguments: [{startPin: int, endPin: int, autoApply: bool}]
    int start_pin = 0;
    int end_pin = 8;  // Default range: GPIO 0-8 (safe range, avoids USB/flash/strapping pins)
    bool auto_apply = true;  // If true, automatically apply found pins

    if (args.is_array() && args.size() >= 1 && args[0].is_object()) {
        fl::json config = args[0];
        if (config.contains("startPin") && config["startPin"].is_int()) {
            start_pin = static_cast<int>(config["startPin"].as_int().value());
        }
        if (config.contains("endPin") && config["endPin"].is_int()) {
            end_pin = static_cast<int>(config["endPin"].as_int().value());
        }
        if (config.contains("autoApply") && config["autoApply"].is_bool()) {
            auto_apply = config["autoApply"].as_bool().value();
        }
    }

    // Validate range
    if (start_pin < 0 || start_pin > 48 || end_pin < 0 || end_pin > 48 || start_pin >= end_pin) {
        response.set("error", "InvalidRange");
        response.set("message", "Pin range must be 0-48 with startPin < endPin");
        return response;
    }

    FL_DBG("[PIN PROBE] Searching for connected pin pairs in range " << start_pin << "-" << end_pin);

    // Helper lambda to test if two pins are connected
    auto testPinPair = [](int tx, int rx) -> bool {
        // Test 1: TX drives LOW, RX has pullup → RX should read LOW if connected
        pinMode(tx, OUTPUT);
        pinMode(rx, INPUT_PULLUP);
        digitalWrite(tx, LOW);
        delay(2);  // Allow signal to settle
        int rx_when_tx_low = digitalRead(rx);

        // Test 2: TX drives HIGH → RX should read HIGH if connected
        digitalWrite(tx, HIGH);
        delay(2);  // Allow signal to settle
        int rx_when_tx_high = digitalRead(rx);

        // Restore pins to safe state
        pinMode(tx, INPUT);
        pinMode(rx, INPUT);

        return (rx_when_tx_low == LOW) && (rx_when_tx_high == HIGH);
    };

    // Search for connected adjacent pin pairs (n, n+1)
    int found_tx = -1;
    int found_rx = -1;
    fl::json tested_pairs = fl::json::array();

    for (int pin = start_pin; pin < end_pin; pin++) {
        int tx_candidate = pin;
        int rx_candidate = pin + 1;

        // No pin skip logic needed - default range (0-8) is safe for all platforms
        // Higher pins (USB, flash, strapping) are excluded by the reduced default range

        fl::json pair = fl::json::object();
        pair.set("tx", static_cast<int64_t>(tx_candidate));
        pair.set("rx", static_cast<int64_t>(rx_candidate));

        // Test TX→RX direction
        bool connected_forward = testPinPair(tx_candidate, rx_candidate);
        if (connected_forward) {
            pair.set("connected", true);
            pair.set("direction", "forward");
            tested_pairs.push_back(pair);
            found_tx = tx_candidate;
            found_rx = rx_candidate;
            FL_DBG("[PIN PROBE] Found connected pair: TX=" << found_tx << " -> RX=" << found_rx);
            break;
        }

        // Test RX→TX direction (reversed)
        bool connected_reverse = testPinPair(rx_candidate, tx_candidate);
        if (connected_reverse) {
            pair.set("connected", true);
            pair.set("direction", "reverse");
            tested_pairs.push_back(pair);
            found_tx = rx_candidate;  // Swap since reversed
            found_rx = tx_candidate;
            FL_DBG("[PIN PROBE] Found connected pair (reversed): TX=" << found_tx << " -> RX=" << found_rx);
            break;
        }

        pair.set("connected", false);
        tested_pairs.push_back(pair);
    }

    // NOTE: testedPairs array omitted - causes heap exhaustion on ESP32 (21+ objects = ~1500 bytes)
    // Validation script doesn't use this data, only needs {found, txPin, rxPin}
    // response.set("testedPairs", tested_pairs);
    fl::json search_range = fl::json::array();
    search_range.push_back(static_cast<int64_t>(start_pin));
    search_range.push_back(static_cast<int64_t>(end_pin));
    response.set("searchRange", search_range);

    if (found_tx >= 0 && found_rx >= 0) {
        response.set("success", true);
        response.set("found", true);
        response.set("txPin", static_cast<int64_t>(found_tx));
        response.set("rxPin", static_cast<int64_t>(found_rx));

        // Auto-apply the found pins if requested
        if (auto_apply) {
            int old_tx = mState->pin_tx;
            int old_rx = mState->pin_rx;
            bool rx_changed = (found_rx != old_rx);

            mState->pin_tx = found_tx;
            mState->pin_rx = found_rx;

            // Recreate RX channel if pin changed
            if (rx_changed && mState->rx_factory) {
                mState->rx_channel.reset();
                mState->rx_channel = mState->rx_factory(found_rx);
                if (!mState->rx_channel) {
                    FL_ERROR("[PIN PROBE] Failed to recreate RX channel on GPIO " << found_rx);
                    // Restore old values
                    mState->pin_tx = old_tx;
                    mState->pin_rx = old_rx;
                    response.set("error", "RxChannelCreationFailed");
                    response.set("autoApplied", false);
                    return response;
                }
            }

            FL_DBG("[PIN PROBE] Auto-applied pins: TX=" << found_tx << ", RX=" << found_rx);
            response.set("autoApplied", true);
            response.set("previousTxPin", static_cast<int64_t>(old_tx));
            response.set("previousRxPin", static_cast<int64_t>(old_rx));
        } else {
            response.set("autoApplied", false);
            response.set("message", "Use setPins to apply the found pins");
        }
    } else {
        response.set("success", true);  // Function succeeded, just no pins found
        response.set("found", false);
        response.set("message", "No connected pin pairs found. Please connect a jumper wire between adjacent GPIO pins.");
    }

    return response;
}

ValidationRemoteControl::ValidationRemoteControl()
    : mRemote(fl::make_unique<fl::Remote>(
        fl::createSerialRequestSource(),
        fl::createSerialResponseSink("REMOTE: ")
    )) {
    // mState will be set by registerFunctions()
}

ValidationRemoteControl::~ValidationRemoteControl() = default;

void ValidationRemoteControl::registerFunctions(fl::shared_ptr<ValidationState> state) {
    // Store shared state
    mState = state;

    // NOTE: All RPC callbacks use const fl::json& for efficient parameter passing.
    // The RPC system strips const/reference qualifiers and stores values in the tuple,
    // then passes them as references to the function. This avoids copies while
    // maintaining clean const-correct API.

    // Register "status" function - device readiness check
    mRemote->bind("status", [this](const fl::json& args) -> fl::json {
        fl::json status = fl::json::object();
        status.set("ready", true);
        status.set("pinTx", static_cast<int64_t>(mState->pin_tx));
        status.set("pinRx", static_cast<int64_t>(mState->pin_rx));
        return status;
    });

    // NOTE: getSchema is no longer needed - use built-in "rpc.discover" instead
    // The rpc.discover method is automatically available via Remote->Rpc and returns
    // the full OpenRPC schema. Our custom getSchema was causing stack overflow on ESP32-C6.

    // Register "debugTest" function - test RPC argument passing
    mRemote->bind("debugTest", [](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();
        response.set("success", true);
        response.set("received", args);
        return response;
    });

    // Register "drivers" function - list available drivers
    mRemote->bind("drivers", [this](const fl::json& args) -> fl::json {
        fl::json drivers = fl::json::array();
        for (fl::size i = 0; i < mState->drivers_available.size(); i++) {
            fl::json driver = fl::json::object();
            driver.set("name", mState->drivers_available[i].name.c_str());
            driver.set("priority", static_cast<int64_t>(mState->drivers_available[i].priority));
            driver.set("enabled", mState->drivers_available[i].enabled);
            drivers.push_back(driver);
        }
        return drivers;
    });

    // Returns: {success, passed, totalTests, passedTests, duration_ms, driver,
    //          laneCount, laneSizes, pattern, firstFailure?}
    // ASYNC: Sends ACK immediately, final response sent via sendAsyncResponse()
    // NOTE: runSingleTestImpl() may return early (error cases) without calling
    // sendAsyncResponse(). This wrapper ensures a response is ALWAYS sent so
    // the Python client never times out waiting 120s for a missing response.
    mRemote->bind("runSingleTest", [this](const fl::json& args) -> fl::json {
        fl::json result = this->runSingleTestImpl(args);
        // If runSingleTestImpl returned a non-null response, it exited early without
        // calling sendAsyncResponse(). Send it now so the client gets a response.
        if (!result.is_null()) {
            mRemote->sendAsyncResponse("runSingleTest", result);
        }
        return fl::json(nullptr);
    }, fl::RpcMode::ASYNC);

    // Register "runParallelTest" - test multiple drivers simultaneously
    // Args: {drivers: [{driver: "PARLIO", laneSizes: [100]}, {driver: "LCD_RGB", laneSizes: [100]}],
    //         pattern?: "MSB_LSB_A", iterations?: 1, timing?: "WS2812B-V5"}
    // Returns: {success, passed, duration_ms, show_duration_us, drivers: [...],
    //           rx_validation_attempted, rx_validation_passed}
    mRemote->bind("runParallelTest", [this](const fl::json& args) -> fl::json {
        fl::json result = this->runParallelTestImpl(args);
        if (!result.is_null()) {
            mRemote->sendAsyncResponse("runParallelTest", result);
        }
        return fl::json(nullptr);
    }, fl::RpcMode::ASYNC);

    // ========================================================================
    // Phase 4 Functions: Utility and Control
    // ========================================================================

    // Register "ping" function - health check with timestamp
    mRemote->bind("ping", [this](const fl::json& args) -> fl::json {
        uint32_t now = millis();

        fl::json response = fl::json::object();
        response.set("success", true);
        response.set("message", "pong");
        response.set("timestamp", static_cast<int64_t>(now));
        response.set("uptimeMs", static_cast<int64_t>(now));
        return response;
    });

    // TEST: Simple RPC without Serial to verify task context works
    mRemote->bind("testNoSerial", [this](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();
        response.set("success", true);
        response.set("message", "RPC works from task context");
        response.set("serial_safe", false);
        return response;
    });

    // Register "setDebug" function - enable/disable runtime debug logging
    mRemote->bind("setDebug", [this](const fl::json& args) -> fl::json {
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

        response.set("success", true);
        response.set("debug_enabled", enabled);
        response.set("message", enabled ? "Debug logging enabled" : "Debug logging disabled");

        return response;
    });

    // Register "testGpioConnection" function - test if TX and RX pins are electrically connected
    // This is a pre-test to diagnose hardware connection issues before running validation
    mRemote->bind("testGpioConnection", [](const fl::json& args) -> fl::json {
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
    mRemote->bind("getPins", [this](const fl::json& args) -> fl::json {
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
    mRemote->bind("setTxPin", [this](const fl::json& args) -> fl::json {
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

        FL_PRINT("[RPC] setTxPin(" << new_pin << ") - TX pin changed from " << old_pin << " to " << new_pin);

        response.set("success", true);
        response.set("txPin", static_cast<int64_t>(new_pin));
        response.set("previousTxPin", static_cast<int64_t>(old_pin));
        return response;
    });

    // Register "setRxPin" function - set RX pin (recreates RX channel)
    mRemote->bind("setRxPin", [this](const fl::json& args) -> fl::json {
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
            FL_PRINT("[RPC] setRxPin(" << new_pin << ") - Recreating RX channel...");

            // Destroy old RX channel
            mState->rx_channel.reset();

            // Create new RX channel on new pin using factory
            mState->rx_channel = mState->rx_factory(new_pin);

            if (mState->rx_channel) {
                FL_PRINT("[RPC] setRxPin - RX channel recreated on GPIO " << new_pin);
                rx_recreated = true;
            } else {
                FL_ERROR("[RPC] setRxPin - Failed to create RX channel on GPIO " << new_pin);
                response.set("error", "RxChannelCreationFailed");
                response.set("message", "Failed to create RX channel on new pin");
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
    mRemote->bind("setPins", [this](const fl::json& args) -> fl::json {
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

            FL_PRINT("[RPC] setPins - Recreating RX channel on GPIO " << new_rx_pin << "...");

            // Destroy old RX channel
            mState->rx_channel.reset();

            // Create new RX channel using factory
            mState->rx_channel = mState->rx_factory(new_rx_pin);

            if (mState->rx_channel) {
                FL_PRINT("[RPC] setPins - RX channel recreated successfully");
                rx_recreated = true;
            } else {
                FL_ERROR("[RPC] setPins - Failed to create RX channel on GPIO " << new_rx_pin);
                // Rollback both pins
                mState->pin_tx = old_tx_pin;
                mState->pin_rx = old_rx_pin;
                response.set("error", "RxChannelCreationFailed");
                response.set("message", "Failed to create RX channel - pins restored to previous values");
                return response;
            }
        } else {
            mState->pin_rx = new_rx_pin;
        }

        FL_PRINT("[RPC] setPins - TX: " << old_tx_pin << " → " << new_tx_pin
                << ", RX: " << old_rx_pin << " → " << new_rx_pin);

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
    mRemote->bind("findConnectedPins", [this](const fl::json& args) -> fl::json {
        return findConnectedPinsImpl(args);
    });

    // Register "help" function - list all RPC functions with descriptions
    mRemote->bind("help", [this](const fl::json& args) -> fl::json {
        fl::json functions = fl::json::array();

        // Phase 1: Basic Control
        fl::json start_fn = fl::json::object();
        start_fn.set("name", "start");
        start_fn.set("phase", "Phase 1: Basic Control");
        start_fn.set("args", "[]");
        start_fn.set("returns", "void");
        start_fn.set("description", "Trigger test matrix execution");
        functions.push_back(start_fn);

        fl::json status_fn = fl::json::object();
        status_fn.set("name", "status");
        status_fn.set("phase", "Phase 1: Basic Control");
        status_fn.set("args", "[]");
        status_fn.set("returns", "{startReceived, testComplete, frameCounter, state}");
        status_fn.set("description", "Query current test state");
        functions.push_back(status_fn);

        fl::json drivers_fn = fl::json::object();
        drivers_fn.set("name", "drivers");
        drivers_fn.set("phase", "Phase 1: Basic Control");
        drivers_fn.set("args", "[]");
        drivers_fn.set("returns", "[{name, priority, enabled}, ...]");
        drivers_fn.set("description", "List available drivers");
        functions.push_back(drivers_fn);

        // Phase 2: Configuration
        fl::json getConfig_fn = fl::json::object();
        getConfig_fn.set("name", "getConfig");
        getConfig_fn.set("phase", "Phase 2: Configuration");
        getConfig_fn.set("args", "[]");
        getConfig_fn.set("returns", "{drivers, laneRange, stripSizes, totalTestCases}");
        getConfig_fn.set("description", "Query current test matrix configuration");
        functions.push_back(getConfig_fn);

        fl::json setDrivers_fn = fl::json::object();
        setDrivers_fn.set("name", "setDrivers");
        setDrivers_fn.set("phase", "Phase 2: Configuration");
        setDrivers_fn.set("args", "[driver1, driver2, ...]");
        setDrivers_fn.set("returns", "{success, driversSet, testCases}");
        setDrivers_fn.set("description", "Configure enabled drivers");
        functions.push_back(setDrivers_fn);

        fl::json setLaneRange_fn = fl::json::object();
        setLaneRange_fn.set("name", "setLaneRange");
        setLaneRange_fn.set("phase", "Phase 2: Configuration");
        setLaneRange_fn.set("args", "[minLanes, maxLanes]");
        setLaneRange_fn.set("returns", "{success, minLanes, maxLanes, testCases}");
        setLaneRange_fn.set("description", "Configure lane range (1-8)");
        functions.push_back(setLaneRange_fn);

        fl::json setStripSizes_fn = fl::json::object();
        setStripSizes_fn.set("name", "setStripSizes");
        setStripSizes_fn.set("phase", "Phase 2: Configuration");
        setStripSizes_fn.set("args", "[size] or [shortSize, longSize]");
        setStripSizes_fn.set("returns", "{success, stripSizesSet, testCases}");
        setStripSizes_fn.set("description", "Configure strip sizes");
        functions.push_back(setStripSizes_fn);

        // Phase 3: Selective Execution
        fl::json runTestCase_fn = fl::json::object();
        runTestCase_fn.set("name", "runTestCase");
        runTestCase_fn.set("phase", "Phase 3: Selective Execution");
        runTestCase_fn.set("args", "[testCaseIndex]");
        runTestCase_fn.set("returns", "{success, testCaseIndex, result}");
        runTestCase_fn.set("description", "Run single test case by index");
        functions.push_back(runTestCase_fn);

        fl::json runDriver_fn = fl::json::object();
        runDriver_fn.set("name", "runDriver");
        runDriver_fn.set("phase", "Phase 3: Selective Execution");
        runDriver_fn.set("args", "[driverName]");
        runDriver_fn.set("returns", "{success, driver, testsRun, results}");
        runDriver_fn.set("description", "Run all tests for specific driver");
        functions.push_back(runDriver_fn);

        fl::json runAll_fn = fl::json::object();
        runAll_fn.set("name", "runAll");
        runAll_fn.set("phase", "Phase 3: Selective Execution");
        runAll_fn.set("args", "[]");
        runAll_fn.set("returns", "{success, totalCases, passedCases, skippedCases, results}");
        runAll_fn.set("description", "Run full test matrix with JSON results");
        functions.push_back(runAll_fn);

        fl::json getResults_fn = fl::json::object();
        getResults_fn.set("name", "getResults");
        getResults_fn.set("phase", "Phase 3: Selective Execution");
        getResults_fn.set("args", "[]");
        getResults_fn.set("returns", "[{driver, lanes, stripSize, ...}, ...]");
        getResults_fn.set("description", "Return all test results");
        functions.push_back(getResults_fn);

        fl::json getResult_fn = fl::json::object();
        getResult_fn.set("name", "getResult");
        getResult_fn.set("phase", "Phase 3: Selective Execution");
        getResult_fn.set("args", "[testCaseIndex]");
        getResult_fn.set("returns", "{driver, lanes, stripSize, ...}");
        getResult_fn.set("description", "Return specific test case result");
        functions.push_back(getResult_fn);

        // Phase 4: Utility and Control
        fl::json reset_fn = fl::json::object();
        reset_fn.set("name", "reset");
        reset_fn.set("phase", "Phase 4: Utility");
        reset_fn.set("args", "[]");
        reset_fn.set("returns", "{success, message, testCasesCleared}");
        reset_fn.set("description", "Reset test state without device reboot");
        functions.push_back(reset_fn);

        fl::json halt_fn = fl::json::object();
        halt_fn.set("name", "halt");
        halt_fn.set("phase", "Phase 4: Utility");
        halt_fn.set("args", "[]");
        halt_fn.set("returns", "{success, message}");
        halt_fn.set("description", "Trigger sketch halt");
        functions.push_back(halt_fn);

        fl::json ping_fn = fl::json::object();
        ping_fn.set("name", "ping");
        ping_fn.set("phase", "Phase 4: Utility");
        ping_fn.set("args", "[]");
        ping_fn.set("returns", "{success, message, timestamp, uptimeMs, frameCounter}");
        ping_fn.set("description", "Health check with timestamp");
        functions.push_back(ping_fn);

        // Phase 5: Pin Configuration
        fl::json getPins_fn = fl::json::object();
        getPins_fn.set("name", "getPins");
        getPins_fn.set("phase", "Phase 5: Pin Configuration");
        getPins_fn.set("args", "[]");
        getPins_fn.set("returns", "{txPin, rxPin, defaults: {txPin, rxPin}, platform}");
        getPins_fn.set("description", "Query current and default pin configuration");
        functions.push_back(getPins_fn);

        fl::json setTxPin_fn = fl::json::object();
        setTxPin_fn.set("name", "setTxPin");
        setTxPin_fn.set("phase", "Phase 5: Pin Configuration");
        setTxPin_fn.set("args", "[pin]");
        setTxPin_fn.set("returns", "{success, txPin, previousTxPin, testCases}");
        setTxPin_fn.set("description", "Set TX pin (regenerates test cases)");
        functions.push_back(setTxPin_fn);

        fl::json setRxPin_fn = fl::json::object();
        setRxPin_fn.set("name", "setRxPin");
        setRxPin_fn.set("phase", "Phase 5: Pin Configuration");
        setRxPin_fn.set("args", "[pin]");
        setRxPin_fn.set("returns", "{success, rxPin, previousRxPin, rxChannelRecreated}");
        setRxPin_fn.set("description", "Set RX pin (recreates RX channel)");
        functions.push_back(setRxPin_fn);

        fl::json setPins_fn = fl::json::object();
        setPins_fn.set("name", "setPins");
        setPins_fn.set("phase", "Phase 5: Pin Configuration");
        setPins_fn.set("args", "[{txPin, rxPin}] or [txPin, rxPin]");
        setPins_fn.set("returns", "{success, txPin, rxPin, rxChannelRecreated, testCases}");
        setPins_fn.set("description", "Set both TX and RX pins atomically");
        functions.push_back(setPins_fn);

        fl::json findConnectedPins_fn = fl::json::object();
        findConnectedPins_fn.set("name", "findConnectedPins");
        findConnectedPins_fn.set("phase", "Phase 5: Pin Configuration");
        findConnectedPins_fn.set("args", "[{startPin, endPin, autoApply}] (all optional)");
        findConnectedPins_fn.set("returns", "{success, found, txPin, rxPin, autoApplied, testedPairs}");
        findConnectedPins_fn.set("description", "Probe adjacent pin pairs to find jumper wire connection");
        functions.push_back(findConnectedPins_fn);

        fl::json help_fn = fl::json::object();
        help_fn.set("name", "help");
        help_fn.set("phase", "Phase 4: Utility");
        help_fn.set("args", "[]");
        help_fn.set("returns", "[{name, phase, args, returns, description}, ...]");
        help_fn.set("description", "List all RPC functions with descriptions");
        functions.push_back(help_fn);

        fl::json testSimd_fn = fl::json::object();
        testSimd_fn.set("name", "testSimd");
        testSimd_fn.set("phase", "Phase 4: Utility");
        testSimd_fn.set("args", "[]");
        testSimd_fn.set("returns", "{success, passed, totalTests, passedTests, failedTests, failures:[string]}");
        testSimd_fn.set("description", "Run comprehensive SIMD test suite (85 tests)");
        functions.push_back(testSimd_fn);

        fl::json testSimdBenchmark_fn = fl::json::object();
        testSimdBenchmark_fn.set("name", "testSimdBenchmark");
        testSimdBenchmark_fn.set("phase", "Phase 4: Utility");
        testSimdBenchmark_fn.set("args", "[{iterations}] (optional, default 10000)");
        testSimdBenchmark_fn.set("returns", "{success, iterations, float_us, s16x16_us, simd_us}");
        testSimdBenchmark_fn.set("description", "Benchmark multiply speed: float vs s16x16 vs s16x16x4 SIMD");
        functions.push_back(testSimdBenchmark_fn);

        fl::json response = fl::json::object();
        response.set("success", true);
        response.set("totalFunctions", static_cast<int64_t>(22));
        response.set("functions", functions);
        return response;
    });

    // Register "testSimd" function - run comprehensive SIMD test suite
    mRemote->bind("testSimd", [](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();

        // Run the full test suite and collect per-test results
        using validation::simd_check::SimdTestEntry;
        const SimdTestEntry* tests = nullptr;
        int num_tests = 0;
        validation::simd_check::getTests(&tests, &num_tests);

        int passed_count = 0;
        int failed_count = 0;
        fl::json failures = fl::json::array();

        for (int i = 0; i < num_tests; i++) {
            bool ok = tests[i].func();
            if (ok) {
                passed_count++;
            } else {
                failed_count++;
                failures.push_back(fl::string(tests[i].name));
            }
        }

        response.set("success", true);
        response.set("passed", failed_count == 0);
        response.set("totalTests", static_cast<int64_t>(num_tests));
        response.set("passedTests", static_cast<int64_t>(passed_count));
        response.set("failedTests", static_cast<int64_t>(failed_count));
        if (failed_count > 0) {
            response.set("failures", failures);
        }
        return response;
    });

    // Register "testSimdBenchmark" - multiply speed benchmark
    mRemote->bind("testSimdBenchmark", [](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();

        int iters = 10000;
        fl::json config;
        if (args.is_object()) {
            config = args;
        } else if (args.is_array() && args.size() >= 1 && args[0].is_object()) {
            config = args[0];
        }
        if (!config.is_null() && config.contains("iterations") && config["iterations"].is_int()) {
            iters = static_cast<int>(config["iterations"].as_int().value());
            if (iters < 1) iters = 1;
            if (iters > 1000000) iters = 1000000;
        }

        auto result = validation::simd_check::runMultiplyBenchmark(iters);

        response.set("success", true);
        response.set("iterations", result.iterations);

        fl::json add = fl::json::object();
        add.set("float_us", result.add_float_us);
        add.set("s8x8_us", result.add_s8x8_us);
        add.set("s16x16_us", result.add_s16x16_us);
        add.set("u16x16_us", result.add_u16x16_us);
        add.set("simd_us", result.add_simd_us);
        response.set("add", add);

        fl::json sub = fl::json::object();
        sub.set("float_us", result.sub_float_us);
        sub.set("s8x8_us", result.sub_s8x8_us);
        sub.set("s16x16_us", result.sub_s16x16_us);
        sub.set("u16x16_us", result.sub_u16x16_us);
        sub.set("simd_us", result.sub_simd_us);
        response.set("sub", sub);

        fl::json mul = fl::json::object();
        mul.set("float_us", result.mul_float_us);
        mul.set("s8x8_us", result.mul_s8x8_us);
        mul.set("s16x16_us", result.mul_s16x16_us);
        mul.set("u16x16_us", result.mul_u16x16_us);
        mul.set("simd_us", result.mul_simd_us);
        response.set("mul", mul);

        fl::json div = fl::json::object();
        div.set("float_us", result.div_float_us);
        div.set("s8x8_us", result.div_s8x8_us);
        div.set("s16x16_us", result.div_s16x16_us);
        div.set("u16x16_us", result.div_u16x16_us);
        response.set("div", div);

        return response;
    });

    // Register "testAsync" function - verify that show() returns before TX completes (async DMA)
    // This proves the SPI driver releases back to the main thread while draining.
    mRemote->bind("testAsync", [this](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();

        // Parse optional parameters from args object
        int num_leds = 300;
        fl::string requested_driver;
        fl::json config;
        if (args.is_object()) {
            config = args;
        } else if (args.is_array() && args.size() >= 1 && args[0].is_object()) {
            config = args[0];
        }
        if (!config.is_null()) {
            if (config.contains("numLeds") && config["numLeds"].is_int()) {
                num_leds = static_cast<int>(config["numLeds"].as_int().value());
            }
            if (config.contains("driver") && config["driver"].is_string()) {
                requested_driver = config["driver"].as_string().value();
            }
        }

        // Set exclusive driver if requested
        if (!requested_driver.empty()) {
            if (!FastLED.setExclusiveDriver(requested_driver.c_str())) {
                response.set("success", false);
                response.set("error", "DriverSetupFailed");
                fl::sstream msg;
                msg << "Failed to set '" << requested_driver.c_str() << "' as exclusive driver";
                response.set("message", msg.str().c_str());
                return response;
            }
        }

        if (num_leds < 10 || num_leds > 1000) {
            response.set("success", false);
            response.set("error", "InvalidNumLeds");
            response.set("message", "numLeds must be 10-1000");
            return response;
        }

        // Set up a channel with the specified number of LEDs
        fl::vector<CRGB> leds(num_leds);
        for (int i = 0; i < num_leds; i++) {
            leds[i] = CRGB(0xFF, 0x00, 0x80);  // Solid color pattern
        }

        fl::ChannelConfig channel_config(
            mState->pin_tx,
            fl::makeTimingConfig<fl::TIMING_WS2812B_V5>(),
            leds,
            RGB
        );

        auto channel = FastLED.add(channel_config);
        if (!channel) {
            response.set("success", false);
            response.set("error", "ChannelCreationFailed");
            response.set("message", "Failed to create channel");
            return response;
        }

        // === DRAW 1: May include one-time SPI hardware init overhead ===
        uint32_t t0 = micros();
        FastLED.show();
        uint32_t t1 = micros();
        FastLED.wait(5000);  // 5 second timeout
        uint32_t t2 = micros();

        uint32_t show1_us = t1 - t0;
        uint32_t wait1_us = t2 - t1;
        uint32_t total1_us = t2 - t0;

        // === DRAW 2: Should be fast (no init overhead) ===
        uint32_t t3 = micros();
        FastLED.show();
        uint32_t t4 = micros();
        FastLED.wait(5000);  // 5 second timeout
        uint32_t t5 = micros();

        uint32_t show2_us = t4 - t3;
        uint32_t wait2_us = t5 - t4;
        uint32_t total2_us = t5 - t3;

        // Clean up channel
        FastLED.clear(ClearFlags::CHANNELS);

        // Determine if async behavior is working on draw 2 (no init overhead):
        // If async: show_us << total_us (show returns quickly, wait blocks for remainder)
        // If blocking: show_us ≈ total_us (show blocks for entire TX, wait returns instantly)
        // Pass criterion: show_us < 50% of total_us (on draw 2)
        bool passed = (total2_us > 0) && (show2_us < total2_us / 2);

        // Determine driver name
        fl::string driver_name = "unknown";
        for (fl::size i = 0; i < mState->drivers_available.size(); i++) {
            if (mState->drivers_available[i].enabled) {
                driver_name = mState->drivers_available[i].name;
                break;
            }
        }

        response.set("success", true);
        response.set("passed", passed);
        // Draw 1 (with possible init overhead)
        response.set("show1_us", static_cast<int64_t>(show1_us));
        response.set("wait1_us", static_cast<int64_t>(wait1_us));
        response.set("total1_us", static_cast<int64_t>(total1_us));
        // Draw 2 (steady-state, no init)
        response.set("show2_us", static_cast<int64_t>(show2_us));
        response.set("wait2_us", static_cast<int64_t>(wait2_us));
        response.set("total2_us", static_cast<int64_t>(total2_us));
        response.set("num_leds", static_cast<int64_t>(num_leds));
        response.set("driver", driver_name.c_str());

        fl::sstream msg;
        if (passed) {
            msg << "Async OK: draw2 show()=" << show2_us
                << "us, wait()=" << wait2_us
                << "us, total=" << total2_us << "us"
                << " (draw1 show=" << show1_us << "us)";
        } else {
            msg << "Async FAIL: draw2 show()=" << show2_us
                << "us out of " << total2_us
                << "us total (expected <50%)"
                << " (draw1 show=" << show1_us << "us)";
        }
        response.set("message", msg.str().c_str());

        return response;
    });

    // ========================================================================
    // Network Validation RPC Functions
    // ========================================================================

    // Register "startNetServer" - Start WiFi AP + HTTP server for net-server validation
    mRemote->bind("startNetServer", [this](const fl::json& args) -> fl::json {
        mState->net_server_active = true;
        return startNetServer();
    });

    // Register "startNetClient" - Start WiFi AP only for net-client validation
    mRemote->bind("startNetClient", [this](const fl::json& args) -> fl::json {
        mState->net_client_active = true;
        return startNetClient();
    });

    // Register "runNetClientTest" - ESP32 fetches from host HTTP server
    // Args: {host_ip: string, port: int}
    mRemote->bind("runNetClientTest", [](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();

        // Parse arguments - args is the config object
        if (!args.is_object()) {
            response.set("success", false);
            response.set("error", "Expected object with host_ip and port");
            return response;
        }

        fl::json host_ip_val = args[fl::string("host_ip")];
        fl::json port_val = args[fl::string("port")];

        if (!host_ip_val.is_string() || !port_val.is_int()) {
            response.set("success", false);
            response.set("error", "Expected {host_ip: string, port: int}");
            return response;
        }

        fl::string host_ip = host_ip_val.as_string().value();
        uint16_t port = static_cast<uint16_t>(port_val.as_int().value());

        return runNetClientTest(host_ip.c_str(), port);
    });

    // Register "runNetLoopback" - Self-contained loopback test (no WiFi needed)
    // Starts HTTP server on localhost, client GETs 127.0.0.1 endpoints
    mRemote->bind("runNetLoopback", [](const fl::json& args) -> fl::json {
        return runNetLoopback();
    });

    // Register "stopNet" - Stop WiFi AP and HTTP server/client
    mRemote->bind("stopNet", [this](const fl::json& args) -> fl::json {
        mState->net_server_active = false;
        mState->net_client_active = false;
        return stopNet();
    });

    // ========================================================================
    // OTA Validation RPC Functions
    // ========================================================================

    // Register "startOta" - Start WiFi AP + OTA HTTP server for OTA validation
    mRemote->bind("startOta", [](const fl::json& args) -> fl::json {
        return startOta();
    });

    // Register "stopOta" - Stop OTA server and WiFi AP
    mRemote->bind("stopOta", [](const fl::json& args) -> fl::json {
        return stopOta();
    });

    // ========================================================================
    // BLE Validation RPC Functions
    // ========================================================================

    // Register "startBle" - Start BLE GATT server + create BLE Remote
    mRemote->bind("startBle", [this](const fl::json& args) -> fl::json {
        return this->startBleRemote();
    });

    // Register "stopBle" - Stop BLE GATT server + destroy BLE Remote
    mRemote->bind("stopBle", [this](const fl::json& args) -> fl::json {
        return this->stopBleRemote();
    });

    // Register "decodeFile" - Decode a media file and return first 16 pixels
    // Args: [base64_data_string, extension_string]  (base64 auto-decoded to fl::vector<fl::u8>)
    mRemote->bind("decodeFile", [](fl::vector<fl::u8> data, fl::string ext) -> fl::json {
        fl::json response = fl::json::object();

        if (ext != ".mp4") {
            response.set("success", false);
            response.set("error", "Only .mp4 supported for device decode");
            return response;
        }

        // Parse MP4 container metadata
        fl::string error;
        fl::H264Info info = fl::H264::parseH264Info(data, &error);
        if (!info.isValid) {
            response.set("success", false);
            response.set("error", error.c_str());
            return response;
        }
        response.set("width", static_cast<int64_t>(info.width));
        response.set("height", static_cast<int64_t>(info.height));

        if (!fl::H264::isSupported()) {
            response.set("success", false);
            response.set("error", "H264 decoder not supported on this platform");
            return response;
        }

        // Create decoder and decode first frame
        fl::string dec_error;
        auto decoder = fl::H264::createDecoder(fl::H264Config{}, &dec_error);
        if (!decoder) {
            response.set("success", false);
            response.set("error", dec_error.c_str());
            return response;
        }

        auto stream = fl::make_shared<fl::memorybuf>(data.size());
        stream->write(data);

        if (!decoder->begin(stream)) {
            fl::string msg;
            decoder->hasError(&msg);
            response.set("success", false);
            response.set("error", msg.empty() ? "Decoder begin() failed" : msg.c_str());
            return response;
        }

        auto result = decoder->decode();
        if (result != fl::DecodeResult::Success) {
            response.set("success", false);
            response.set("error", "Decode returned non-success");
            response.set("decode_result", static_cast<int64_t>(static_cast<int>(result)));
            decoder->end();
            return response;
        }

        fl::Frame frame = decoder->getCurrentFrame();
        decoder->end();

        if (!frame.isValid()) {
            response.set("success", false);
            response.set("error", "Decoded frame is invalid");
            return response;
        }

        response.set("success", true);
        response.set("frame_width", static_cast<int64_t>(frame.getWidth()));
        response.set("frame_height", static_cast<int64_t>(frame.getHeight()));

        // Return first 16 pixels as [[r,g,b], ...]
        auto pixels = frame.rgb();
        fl::json pixel_array = fl::json::array();
        int count = pixels.size() < 16 ? static_cast<int>(pixels.size()) : 16;
        for (int i = 0; i < count; i++) {
            fl::json px = fl::json::array();
            px.push_back(static_cast<int64_t>(pixels[i].r));
            px.push_back(static_cast<int64_t>(pixels[i].g));
            px.push_back(static_cast<int64_t>(pixels[i].b));
            pixel_array.push_back(px);
        }
        response.set("pixels", pixel_array);

        return response;
    });

    // Register "bleStatus" - Query BLE connection/subscription state
    mRemote->bind("bleStatus", [this](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();
#if FL_BLE_AVAILABLE
        response.set("ble_active", mState->ble_server_active);
        fl::BleStatusInfo info = fl::queryBleStatus(mBleState);
        response.set("connected", info.connected);
        response.set("connected_count", static_cast<int64_t>(info.connectedCount));
        response.set("tx_char_exists", info.txCharExists);
        response.set("tx_value_len", static_cast<int64_t>(info.txValueLen));
        response.set("ring_head", static_cast<int64_t>(info.ringHead));
        response.set("ring_tail", static_cast<int64_t>(info.ringTail));
#else
        response.set("ble_active", false);
        response.set("error", "BLE not supported on this platform");
#endif
        return response;
    });

    // ========================================================================
    // Coroutine Tests - fl::task::coroutine() and fl::await()
    // ========================================================================

    // Test: Basic coroutine creation and completion
    mRemote->bind("testCoroutineBasic", [](const fl::json& args) -> fl::json {
        (void)args;
        fl::json r = fl::json::object();

        fl::atomic<bool> task_ran(false);
        fl::atomic<bool> task_completed(false);

        fl::CoroutineConfig cfg;
        cfg.function = [&task_ran, &task_completed]() {
            task_ran.store(true);
            delay(50);
            task_completed.store(true);
        };
        cfg.name = "test_basic";
        auto t = fl::task::coroutine(cfg);

        uint32_t start = millis();
        while (t.isRunning() && (millis() - start) < 2000) {
            delay(10);
        }

        bool passed = task_ran.load() && task_completed.load() && !t.isRunning();
        r.set("success", passed);
        r.set("taskRan", task_ran.load());
        r.set("taskCompleted", task_completed.load());
        r.set("isRunning", t.isRunning());
        r.set("durationMs", static_cast<int64_t>(millis() - start));
        return r;
    });

    // Test: Task stop() while running
    mRemote->bind("testCoroutineStop", [](const fl::json& args) -> fl::json {
        (void)args;
        fl::json r = fl::json::object();

        fl::atomic<bool> task_started(false);

        fl::CoroutineConfig cfg;
        cfg.function = [&task_started]() {
            task_started.store(true);
            while (true) {
                delay(10);
            }
        };
        cfg.name = "test_stop";
        auto t = fl::task::coroutine(cfg);

        uint32_t start = millis();
        while (!task_started.load() && (millis() - start) < 2000) {
            delay(10);
        }

        bool was_running = t.isRunning();
        t.stop();
        bool stopped = !t.isRunning();

        bool passed = task_started.load() && was_running && stopped;
        r.set("success", passed);
        r.set("taskStarted", task_started.load());
        r.set("wasRunning", was_running);
        r.set("stopped", stopped);
        r.set("durationMs", static_cast<int64_t>(millis() - start));
        return r;
    });

    // Test: Multiple concurrent coroutines
    mRemote->bind("testCoroutineConcurrent", [](const fl::json& args) -> fl::json {
        (void)args;
        fl::json r = fl::json::object();

        const int NUM_TASKS = 3;
        fl::atomic<int> completed_count(0);
        fl::atomic<bool> task_flags[NUM_TASKS];
        for (int i = 0; i < NUM_TASKS; i++) {
            task_flags[i].store(false);
        }

        fl::task tasks[NUM_TASKS];
        for (int i = 0; i < NUM_TASKS; i++) {
            fl::CoroutineConfig cfg;
            cfg.function = [i, &task_flags, &completed_count]() {
                delay(20 + i * 20);
                task_flags[i].store(true);
                completed_count.fetch_add(1);
            };
            cfg.name = "test_concurrent";
            tasks[i] = fl::task::coroutine(cfg);
        }

        uint32_t start = millis();
        while (completed_count.load() < NUM_TASKS && (millis() - start) < 3000) {
            delay(10);
        }

        bool all_completed = true;
        bool all_stopped = true;
        for (int i = 0; i < NUM_TASKS; i++) {
            if (!task_flags[i].load()) all_completed = false;
            if (tasks[i].isRunning()) all_stopped = false;
        }

        bool passed = all_completed && all_stopped && (completed_count.load() == NUM_TASKS);
        r.set("success", passed);
        r.set("completedCount", static_cast<int64_t>(completed_count.load()));
        r.set("allCompleted", all_completed);
        r.set("allStopped", all_stopped);
        r.set("durationMs", static_cast<int64_t>(millis() - start));
        return r;
    });

    // Test: Consumer coroutine awaits promise, producer coroutine fulfills it.
    // Verifies fl::await() truly blocks until the producer resolves the promise.
    // Main thread does NOT touch the promise — only the producer coroutine does.
    mRemote->bind("testCoroutineAwait", [](const fl::json& args) -> fl::json {
        (void)args;
        fl::json r = fl::json::object();

        auto promise_ptr = fl::make_shared<fl::promise<int>>(fl::promise<int>::create());
        fl::atomic<bool> consumer_started(false);
        fl::atomic<bool> consumer_finished(false);
        fl::atomic<int> consumer_value(0);
        fl::atomic<bool> consumer_ok(false);
        fl::atomic<bool> producer_started(false);
        fl::atomic<bool> producer_finished(false);

        // Consumer coroutine: starts first, calls fl::await() which should block
        // until the producer coroutine resolves the promise
        fl::CoroutineConfig consumer_cfg;
        consumer_cfg.function = [promise_ptr, &consumer_started, &consumer_finished,
                                 &consumer_value, &consumer_ok]() {
            consumer_started.store(true);
            // This should block the coroutine until producer fulfills the promise
            auto result = fl::await(*promise_ptr);
            if (result.ok()) {
                consumer_ok.store(true);
                consumer_value.store(result.value());
            }
            consumer_finished.store(true);
        };
        consumer_cfg.name = "await_consumer";
        auto consumer = fl::task::coroutine(consumer_cfg);

        // Small delay so consumer enters fl::await() before producer starts
        delay(50);

        // Producer coroutine: simulates async work, then resolves the promise
        fl::CoroutineConfig producer_cfg;
        producer_cfg.function = [promise_ptr, &producer_started, &producer_finished]() {
            producer_started.store(true);
            delay(200);  // simulate real async work (e.g. sensor read, network I/O)
            promise_ptr->complete_with_value(42);
            producer_finished.store(true);
        };
        producer_cfg.name = "await_producer";
        auto producer = fl::task::coroutine(producer_cfg);

        // Main thread waits for both coroutines to finish (polling only)
        uint32_t start = millis();
        while ((!consumer_finished.load() || producer.isRunning()) && (millis() - start) < 5000) {
            delay(10);
        }

        // Verify: consumer was truly blocked — it should not finish before producer
        bool passed = consumer_started.load() && consumer_finished.load()
                    && consumer_ok.load() && (consumer_value.load() == 42)
                    && producer_started.load() && producer_finished.load();
        r.set("success", passed);
        r.set("consumerStarted", consumer_started.load());
        r.set("consumerFinished", consumer_finished.load());
        r.set("consumerOk", consumer_ok.load());
        r.set("consumerValue", static_cast<int64_t>(consumer_value.load()));
        r.set("producerStarted", producer_started.load());
        r.set("producerFinished", producer_finished.load());
        r.set("durationMs", static_cast<int64_t>(millis() - start));
        return r;
    });

    // Test: Consumer coroutine awaits promise, producer coroutine rejects it.
    // Verifies that fl::await() properly propagates errors from producer.
    mRemote->bind("testCoroutineAwaitError", [](const fl::json& args) -> fl::json {
        (void)args;
        fl::json r = fl::json::object();

        auto promise_ptr = fl::make_shared<fl::promise<int>>(fl::promise<int>::create());
        fl::atomic<bool> consumer_finished(false);
        fl::atomic<bool> got_error(false);
        fl::atomic<bool> producer_finished(false);

        // Consumer coroutine: awaits promise, expects error
        fl::CoroutineConfig consumer_cfg;
        consumer_cfg.function = [promise_ptr, &consumer_finished, &got_error]() {
            auto result = fl::await(*promise_ptr);
            if (!result.ok()) {
                got_error.store(true);
            }
            consumer_finished.store(true);
        };
        consumer_cfg.name = "await_err_consumer";
        auto consumer = fl::task::coroutine(consumer_cfg);

        delay(50);  // let consumer enter await

        // Producer coroutine: rejects the promise with error
        fl::CoroutineConfig producer_cfg;
        producer_cfg.function = [promise_ptr, &producer_finished]() {
            delay(100);
            promise_ptr->complete_with_error(fl::Error("test error"));
            producer_finished.store(true);
        };
        producer_cfg.name = "await_err_producer";
        auto producer = fl::task::coroutine(producer_cfg);

        uint32_t start = millis();
        while ((!consumer_finished.load() || producer.isRunning()) && (millis() - start) < 5000) {
            delay(10);
        }

        bool passed = consumer_finished.load() && got_error.load() && producer_finished.load();
        r.set("success", passed);
        r.set("consumerFinished", consumer_finished.load());
        r.set("gotError", got_error.load());
        r.set("producerFinished", producer_finished.load());
        r.set("durationMs", static_cast<int64_t>(millis() - start));
        return r;
    });

    // Test: Promise then/catch_ callbacks fire correctly when fulfilled by a coroutine.
    // The promise is created with .then() and .catch_() callbacks attached BEFORE
    // the producer coroutine fulfills it, verifying callback dispatch works.
    mRemote->bind("testCoroutinePromiseCallbacks", [](const fl::json& args) -> fl::json {
        (void)args;
        fl::json r = fl::json::object();

        auto promise_ptr = fl::make_shared<fl::promise<int>>(fl::promise<int>::create());
        fl::atomic<bool> then_called(false);
        fl::atomic<int> then_value(0);
        fl::atomic<bool> catch_called(false);
        fl::atomic<bool> producer_finished(false);

        // Attach callbacks to the promise BEFORE producer runs
        promise_ptr->then([&then_called, &then_value](const int& val) {
            then_called.store(true);
            then_value.store(val);
        });
        promise_ptr->catch_([&catch_called](const fl::Error&) {
            catch_called.store(true);
        });

        // Producer coroutine fulfills the promise
        fl::CoroutineConfig producer_cfg;
        producer_cfg.function = [promise_ptr, &producer_finished]() {
            delay(100);
            promise_ptr->complete_with_value(99);
            producer_finished.store(true);
        };
        producer_cfg.name = "cb_producer";
        auto producer = fl::task::coroutine(producer_cfg);

        uint32_t start = millis();
        while (producer.isRunning() && (millis() - start) < 5000) {
            delay(10);
            promise_ptr->update();  // pump callbacks
        }
        promise_ptr->update();  // final pump

        // then() should fire, catch_() should NOT
        bool passed = then_called.load() && (then_value.load() == 99)
                    && !catch_called.load() && producer_finished.load();
        r.set("success", passed);
        r.set("thenCalled", then_called.load());
        r.set("thenValue", static_cast<int64_t>(then_value.load()));
        r.set("catchCalled", catch_called.load());
        r.set("producerFinished", producer_finished.load());
        r.set("durationMs", static_cast<int64_t>(millis() - start));
        return r;
    });

    // Test: Promise catch_ callback fires on rejection by a coroutine.
    mRemote->bind("testCoroutinePromiseCatchCallback", [](const fl::json& args) -> fl::json {
        (void)args;
        fl::json r = fl::json::object();

        auto promise_ptr = fl::make_shared<fl::promise<int>>(fl::promise<int>::create());
        fl::atomic<bool> then_called(false);
        fl::atomic<bool> catch_called(false);
        fl::atomic<bool> producer_finished(false);

        promise_ptr->then([&then_called](const int&) {
            then_called.store(true);
        });
        promise_ptr->catch_([&catch_called](const fl::Error&) {
            catch_called.store(true);
        });

        // Producer coroutine rejects the promise
        fl::CoroutineConfig producer_cfg;
        producer_cfg.function = [promise_ptr, &producer_finished]() {
            delay(100);
            promise_ptr->complete_with_error(fl::Error("rejection test"));
            producer_finished.store(true);
        };
        producer_cfg.name = "catch_producer";
        auto producer = fl::task::coroutine(producer_cfg);

        uint32_t start = millis();
        while (producer.isRunning() && (millis() - start) < 5000) {
            delay(10);
            promise_ptr->update();
        }
        promise_ptr->update();

        // catch_() should fire, then() should NOT
        bool passed = !then_called.load() && catch_called.load() && producer_finished.load();
        r.set("success", passed);
        r.set("thenCalled", then_called.load());
        r.set("catchCalled", catch_called.load());
        r.set("producerFinished", producer_finished.load());
        r.set("durationMs", static_cast<int64_t>(millis() - start));
        return r;
    });

    // Test: Pipeline of 3 coroutines passing data through promises.
    // coroutine A produces value -> promise1 -> coroutine B transforms -> promise2 -> coroutine C consumes
    mRemote->bind("testCoroutineChainedAwait", [](const fl::json& args) -> fl::json {
        (void)args;
        fl::json r = fl::json::object();

        auto p1 = fl::make_shared<fl::promise<int>>(fl::promise<int>::create());
        auto p2 = fl::make_shared<fl::promise<int>>(fl::promise<int>::create());
        fl::atomic<int> final_value(0);
        fl::atomic<bool> chain_complete(false);
        fl::atomic<bool> a_done(false);
        fl::atomic<bool> b_done(false);
        fl::atomic<bool> c_done(false);

        // Coroutine C: end of chain, awaits p2
        fl::CoroutineConfig cfg_c;
        cfg_c.function = [p2, &final_value, &chain_complete, &c_done]() {
            auto result = fl::await(*p2);
            if (result.ok()) {
                final_value.store(result.value());
            }
            c_done.store(true);
            chain_complete.store(true);
        };
        cfg_c.name = "chain_c";
        auto tc = fl::task::coroutine(cfg_c);

        // Coroutine B: middle of chain, awaits p1, transforms, fulfills p2
        fl::CoroutineConfig cfg_b;
        cfg_b.function = [p1, p2, &b_done]() {
            auto result = fl::await(*p1);
            if (result.ok()) {
                // Transform: multiply by 10
                p2->complete_with_value(result.value() * 10);
            } else {
                p2->complete_with_error(result.error());
            }
            b_done.store(true);
        };
        cfg_b.name = "chain_b";
        auto tb = fl::task::coroutine(cfg_b);

        // Coroutine A: head of chain, produces the initial value into p1
        fl::CoroutineConfig cfg_a;
        cfg_a.function = [p1, &a_done]() {
            delay(100);  // simulate work
            p1->complete_with_value(7);
            a_done.store(true);
        };
        cfg_a.name = "chain_a";
        auto ta = fl::task::coroutine(cfg_a);

        uint32_t start = millis();
        while (!chain_complete.load() && (millis() - start) < 5000) {
            delay(10);
        }

        // 7 * 10 = 70
        bool passed = chain_complete.load() && (final_value.load() == 70)
                    && a_done.load() && b_done.load() && c_done.load();
        r.set("success", passed);
        r.set("finalValue", static_cast<int64_t>(final_value.load()));
        r.set("aDone", a_done.load());
        r.set("bDone", b_done.load());
        r.set("cDone", c_done.load());
        r.set("durationMs", static_cast<int64_t>(millis() - start));
        return r;
    });

    // Run all coroutine tests sequentially
    mRemote->bind("testCoroutineAll", [this](const fl::json& args) -> fl::json {
        (void)args;
        fl::json r = fl::json::object();

        const char* test_names[] = {
            "testCoroutineBasic",
            "testCoroutineStop",
            "testCoroutineConcurrent",
            "testCoroutineAwait",
            "testCoroutineAwaitError",
            "testCoroutinePromiseCallbacks",
            "testCoroutinePromiseCatchCallback",
            "testCoroutineChainedAwait"
        };
        const int num_tests = 8;

        int passed = 0;
        int failed = 0;
        fl::json results = fl::json::object();

        for (int i = 0; i < num_tests; i++) {
            fl::json empty_args = fl::json::object();
            auto bound = mRemote->get<fl::json(const fl::json&)>(test_names[i]);
            if (bound.ok()) {
                fl::json test_result = bound.value()(empty_args);
                bool success = false;
                if (test_result.contains("success")) {
                    auto val = test_result["success"].as_bool();
                    success = val.has_value() && val.value();
                }
                if (success) {
                    passed++;
                    FL_PRINT("[COROUTINE] PASS: " << test_names[i]);
                } else {
                    failed++;
                    FL_PRINT("[COROUTINE] FAIL: " << test_names[i]);
                }
                results.set(test_names[i], test_result);
            } else {
                failed++;
                fl::json err = fl::json::object();
                err.set("success", false);
                err.set("error", "Method not found");
                results.set(test_names[i], err);
                FL_PRINT("[COROUTINE] FAIL: " << test_names[i] << " (not found)");
            }
        }

        r.set("success", failed == 0);
        r.set("passed", static_cast<int64_t>(passed));
        r.set("failed", static_cast<int64_t>(failed));
        r.set("total", static_cast<int64_t>(num_tests));
        r.set("results", results);
        return r;
    });
}

void ValidationRemoteControl::tick(uint32_t current_millis) {
    if (mRemote) {
        // Remote::update() does pull + tick + push
        mRemote->update(current_millis);
    }
    if (mBleRemote) {
        mBleRemote->update(current_millis);
    }
    // Deferred BLE teardown: stopBle RPC sets this flag so the response
    // is sent (via push() above) before we call destroyBleTransport().
    if (mPendingBleStop) {
        mPendingBleStop = false;
#if FL_BLE_AVAILABLE
        mBleRemote.reset();  // destroy lambdas before freeing state they capture
        fl::destroyBleTransport(mBleState);
        mBleState = nullptr;
        mState->ble_server_active = false;
        getBleState().ble_server_active = false;
        FL_WARN("[BLE] Deferred teardown complete");
#endif
    }
}

void ValidationRemoteControl::registerAllMethods(fl::Remote* remote) {
    // Register the core methods that BLE remote needs.
    // This registers a subset of methods — enough for ping/pong PoC.

    // Register "ping" function - health check with timestamp
    remote->bind("ping", [this](const fl::json& args) -> fl::json {
        uint32_t now = millis();
        fl::json response = fl::json::object();
        response.set("success", true);
        response.set("message", "pong");
        response.set("timestamp", static_cast<int64_t>(now));
        response.set("uptimeMs", static_cast<int64_t>(now));
        response.set("transport", "ble");
        return response;
    });

    // Register "status" function - device readiness check
    remote->bind("status", [this](const fl::json& args) -> fl::json {
        fl::json status = fl::json::object();
        status.set("ready", true);
        status.set("pinTx", static_cast<int64_t>(mState->pin_tx));
        status.set("pinRx", static_cast<int64_t>(mState->pin_rx));
        status.set("transport", "ble");
        return status;
    });
}

fl::json ValidationRemoteControl::startBleRemote() {
#if FL_BLE_AVAILABLE
    if (mBleRemote) {
        fl::json response = fl::json::object();
        response.set("success", true);
        response.set("message", "BLE remote already active");
        response.set("device_name", VALIDATION_BLE_DEVICE_NAME);
        return response;
    }

    // Create BLE GATT server (heap-allocates transport state)
    mBleState = fl::createBleTransport(VALIDATION_BLE_DEVICE_NAME);

    // Get transport lambdas that capture mBleState
    auto [source, sink] = fl::getBleTransportCallbacks(mBleState);

    // Create BLE Remote instance with BLE transport
    mBleRemote = fl::make_unique<fl::Remote>(source, sink);

    // Register RPC methods on the BLE remote
    registerAllMethods(mBleRemote.get());

    mState->ble_server_active = true;
    getBleState().ble_server_active = true;

    fl::json response = fl::json::object();
    response.set("success", true);
    response.set("device_name", VALIDATION_BLE_DEVICE_NAME);
    response.set("service_uuid", FL_BLE_SERVICE_UUID);
    response.set("rx_uuid", FL_BLE_CHAR_RX_UUID);
    response.set("tx_uuid", FL_BLE_CHAR_TX_UUID);
    FL_WARN("[BLE] Remote created and advertising");
    return response;
#else
    fl::json response = fl::json::object();
    response.set("success", false);
    response.set("error", "BLE only supported on ESP32");
    return response;
#endif
}

fl::json ValidationRemoteControl::stopBleRemote() {
#if FL_BLE_AVAILABLE
    // Defer actual BLE teardown to tick() so the RPC response is sent first.
    // BLEDevice::deinit(true) blocks long enough to prevent the response
    // from being transmitted over serial before the device resets BLE state.
    mPendingBleStop = true;
#endif
    fl::json response = fl::json::object();
    response.set("success", true);
    return response;
}
