// runParallelTestImpl. Extracted from AutoResearchRemote.cpp as part of #3132 / meta #3127.

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
#include "fl/stl/detail/memory_file_handle.h"
#include "fl/fx/frame.h"

fl::json AutoResearchRemoteControl::runParallelTestImpl(const fl::json& args) {
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
    fl::ClocklessEncoder resolved_encoder = fl::ClocklessEncoder::CLOCKLESS_ENCODER_WS2812;
    if (timing_name == "UCS7604-800KHZ") {
        resolved_timing = fl::makeTimingConfig<fl::TIMING_UCS7604_800KHZ>();
        resolved_encoder = fl::encoder_for<fl::TIMING_UCS7604_800KHZ>();
    } else {
        resolved_timing = fl::makeTimingConfig<fl::TIMING_WS2812B_V5>();
        resolved_encoder = fl::encoder_for<fl::TIMING_WS2812B_V5>();
    }
    fl::NamedTimingConfig timing_config(resolved_timing, timing_name.c_str(), resolved_encoder);

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

            // Set up channel with typed driver selection (#2459).
            // The pre-#2459 string `mAffinity` field is gone — translate the
            // discovered driver name back to a `fl::Bus` enum value.
            fl::ChannelOptions opts;
            {
                const fl::string& n = de.name;
                if      (n == "RMT")           opts.mBus = fl::Bus::RMT;
                else if (n == "PARLIO")        opts.mBus = fl::Bus::FLEX_IO;
                else if (n == "SPI")           opts.mBus = fl::Bus::SPI;
                else if (n == "I2S")           opts.mBus = fl::Bus::FLEX_IO;
                else if (n == "I2S_SPI")       opts.mBus = fl::Bus::FLEX_IO;
                else if (n == "LCD_RGB")       { opts.mBus = fl::Bus::FLEX_IO; opts.mBusWhich = 1; }
                else if (n == "LCD_SPI")       opts.mBus = fl::Bus::FLEX_IO;
                else if (n == "LCD_CLOCKLESS") opts.mBus = fl::Bus::FLEX_IO;
                else if (n == "UART")          opts.mBus = fl::Bus::UART;
                else if (n == "FLEX_IO")       { opts.mBus = fl::Bus::FLEX_IO; opts.mBusWhich = 1; }
                else if (n == "OBJECT_FLED")   opts.mBus = fl::Bus::FLEX_IO;
                else if (n == "LPUART")        opts.mBus = fl::Bus::UART;
                else if (n == "BIT_BANG")      opts.mBus = fl::Bus::BIT_BANG;
                else if (n == "STUB")          opts.mBus = fl::Bus::BIT_BANG;
                // else: leave Bus::AUTO; priority dispatch will pick.
            }

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
    fl::vector<fl::RunResult> run_results;

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

            fl::AutoResearchConfig autoresearch_config(
                timing_config.timing,
                timing_config.name,
                tx_configs,
                primary_driver.name.c_str(),
                mState->rx_channel,
                mState->rx_buffer,
                primary_driver.lane_sizes[0],
                fl::RxDeviceType::RMT,
                timing_config.encoder
            );

            int total_tests = 0;
            int passed_tests = 0;
            uint32_t val_show_duration_ms = 0;

            autoResearchChipsetTiming(autoresearch_config, total_tests, passed_tests,
                                      val_show_duration_ms, &run_results);

            rx_validation_passed = (total_tests > 0) && (passed_tests == total_tests);
        }
    }

    // Step 6: Clean up channels
    FastLED.clear(ClearFlags::CHANNELS);

    uint32_t duration_ms = millis() - start_ms;
    int total_tests = 1;
    int passed_tests = show_success ? 1 : 0;
    const int primary_tx_pin = driver_entries.empty()
                                   ? mState->pin_tx
                                   : driver_entries[0].pin_tx;
    int capture_evidence_bytes = 0;
    int capture_evidence_raw_edges = 0;
    for (fl::size ri = 0; ri < run_results.size(); ri++) {
        const auto& rr = run_results[ri];
        if (rr.capturedBytes > 0) {
            capture_evidence_bytes += rr.capturedBytes;
        }
        if (rr.rawEdgesAfterWait > capture_evidence_raw_edges) {
            capture_evidence_raw_edges = rr.rawEdgesAfterWait;
        }
    }
    if (rx_validation_attempted) {
        total_tests++;
        if (rx_validation_passed) {
            passed_tests++;
        }
    }

    // ========== RESPONSE ==========
    response.set("success", true);
    response.set("passed", show_success && rx_validation_passed);
    response.set("totalTests", static_cast<int64_t>(total_tests));
    response.set("passedTests", static_cast<int64_t>(passed_tests));
    response.set("failedTests", static_cast<int64_t>(total_tests - passed_tests));
    response.set("requestedTxPin", static_cast<int64_t>(primary_tx_pin));
    response.set("requestedRxPin", static_cast<int64_t>(mState->pin_rx));
    response.set("actualTxPin", static_cast<int64_t>(primary_tx_pin));
    response.set("actualRxPin", static_cast<int64_t>(mState->pin_rx));
    fl::string capture_backend = mState->rx_channel
                                     ? mState->rx_channel->getEngineName()
                                     : fl::string("none");
    response.set("captureBackend", capture_backend.c_str());
    response.set("captureEvidenceExpected", rx_validation_attempted);
    response.set("captureEvidenceBytes",
                 static_cast<int64_t>(capture_evidence_bytes));
    response.set("captureEvidenceRawEdges",
                 static_cast<int64_t>(capture_evidence_raw_edges));
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

#endif // !(FASTLED_AUTORESEARCH_LOW_MEMORY)
