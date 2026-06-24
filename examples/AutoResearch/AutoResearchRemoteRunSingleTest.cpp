// runSingleTestImpl + anonymous-namespace helpers used by it (measureTightTiming etc).
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
#include "fl/stl/cstdio.h"
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

namespace {

uint32_t expectedClocklessWireUs(const fl::ChipsetTimingConfig& timing,
                                 uint32_t max_leds) {
    const uint64_t bit_period_ns = timing.total_period_ns();
    const uint64_t payload_ns =
        static_cast<uint64_t>(max_leds) * 24ULL * bit_period_ns;
    return static_cast<uint32_t>((payload_ns + 999ULL) / 1000ULL) +
           timing.reset_us;
}

uint32_t maxLaneLeds(const fl::vector<fl::ChannelConfig>& tx_configs) {
    uint32_t max_leds = 0;
    for (fl::size i = 0; i < tx_configs.size(); i++) {
        const uint32_t count =
            static_cast<uint32_t>(tx_configs[i].mLeds.size());
        if (count > max_leds) {
            max_leds = count;
        }
    }
    return max_leds;
}

class ScopedFastLedBrightness {
  public:
    explicit ScopedFastLedBrightness(uint8_t brightness) FL_NO_EXCEPT
        : mSavedBrightness(FastLED.getBrightness()) {
        FastLED.setBrightness(brightness);
    }

    ~ScopedFastLedBrightness() FL_NO_EXCEPT {
        FastLED.setBrightness(mSavedBrightness);
    }

  private:
    uint8_t mSavedBrightness;
};

fl::json measureTightTiming(const fl::string& driver_name,
                            const fl::ChipsetTimingConfig& timing,
                            const fl::vector<fl::ChannelConfig>& tx_configs,
                            int iterations,
                            uint32_t max_allowed_overhead_us,
                            bool& out_passed) {
    fl::json metric = fl::json::object();
    out_passed = false;

    metric.set("requested", true);
    metric.set("supported", false);
    metric.set("driver", driver_name.c_str());

    if (iterations < 1) {
        metric.set("message", "iterations must be >= 1");
        return metric;
    }
    if (tx_configs.empty()) {
        metric.set("message", "no channel configs");
        return metric;
    }
    for (fl::size i = 0; i < tx_configs.size(); i++) {
        if (tx_configs[i].isSpi()) {
            metric.set("message", "clocked SPI timing metric is not supported");
            return metric;
        }
    }

    fl::vector<fl::ChannelConfig> sample_configs = tx_configs;
    fl::vector<fl::shared_ptr<fl::Channel>> channels;
    for (fl::size i = 0; i < sample_configs.size(); i++) {
        fl::ChannelConfig channel_config(
            sample_configs[i].getDataPin(),
            timing,
            sample_configs[i].mLeds,
            sample_configs[i].rgb_order);
        fl::shared_ptr<fl::Channel> channel = FastLED.add(channel_config);
        if (!channel) {
            FastLED.clear(ClearFlags::CHANNELS);
            metric.set("message", "failed to create channel");
            return metric;
        }
        channels.push_back(channel);
    }

    ScopedFastLedBrightness scoped_brightness(255);
    for (fl::size lane = 0; lane < sample_configs.size(); lane++) {
        fill_solid(sample_configs[lane].mLeds.data(),
                   sample_configs[lane].mLeds.size(),
                   CRGB::Black);
    }
    FastLED.show();
    if (!FastLED.wait(1000)) {
        FastLED.clear(ClearFlags::CHANNELS);
        metric.set("message", "warmup wait timeout");
        return metric;
    }
    delay(2);

    const uint32_t expected_wire_us =
        expectedClocklessWireUs(timing, maxLaneLeds(sample_configs));
    uint32_t min_show_us = 0xFFFFFFFFu;
    uint32_t max_show_us = 0;
    uint32_t min_wait_us = 0xFFFFFFFFu;
    uint32_t max_wait_us = 0;
    uint32_t min_total_us = 0xFFFFFFFFu;
    uint32_t max_total_us = 0;
    uint32_t min_overhead_us = 0xFFFFFFFFu;
    uint32_t max_overhead_us = 0;
    uint64_t sum_show_us = 0;
    uint64_t sum_wait_us = 0;
    uint64_t sum_total_us = 0;
    uint64_t sum_overhead_us = 0;
    int samples = 0;
    bool timed_out = false;

    for (int sample = 0; sample < iterations; sample++) {
        for (fl::size lane = 0; lane < sample_configs.size(); lane++) {
            const uint8_t r = static_cast<uint8_t>(31 + sample * 17 + lane * 11);
            const uint8_t g = static_cast<uint8_t>(67 + sample * 23 + lane * 7);
            const uint8_t b = static_cast<uint8_t>(103 + sample * 29 + lane * 5);
            fill_solid(sample_configs[lane].mLeds.data(),
                       sample_configs[lane].mLeds.size(),
                       CRGB(r, g, b));
        }

        const uint32_t t0 = micros();
        FastLED.show();
        const uint32_t t1 = micros();
        const bool ok = FastLED.wait(1000);
        const uint32_t t2 = micros();
        if (!ok) {
            timed_out = true;
            break;
        }

        const uint32_t show_us = t1 - t0;
        const uint32_t wait_us = t2 - t1;
        const uint32_t total_us = t2 - t0;
        const uint32_t overhead_us =
            total_us > expected_wire_us ? total_us - expected_wire_us : 0;

        if (show_us < min_show_us) min_show_us = show_us;
        if (show_us > max_show_us) max_show_us = show_us;
        if (wait_us < min_wait_us) min_wait_us = wait_us;
        if (wait_us > max_wait_us) max_wait_us = wait_us;
        if (total_us < min_total_us) min_total_us = total_us;
        if (total_us > max_total_us) max_total_us = total_us;
        if (overhead_us < min_overhead_us) min_overhead_us = overhead_us;
        if (overhead_us > max_overhead_us) max_overhead_us = overhead_us;

        sum_show_us += show_us;
        sum_wait_us += wait_us;
        sum_total_us += total_us;
        sum_overhead_us += overhead_us;
        samples++;
    }

    FastLED.clear(ClearFlags::CHANNELS);

    metric.set("supported", true);
    metric.set("samples", static_cast<int64_t>(samples));
    metric.set("iterations", static_cast<int64_t>(iterations));
    metric.set("expected_wire_us", static_cast<int64_t>(expected_wire_us));
    metric.set("max_allowed_overhead_us",
               static_cast<int64_t>(max_allowed_overhead_us));

    if (samples == 0) {
        metric.set("passed", false);
        metric.set("message", timed_out ? "timing wait timeout" : "no samples");
        return metric;
    }

    metric.set("min_show_us", static_cast<int64_t>(min_show_us));
    metric.set("max_show_us", static_cast<int64_t>(max_show_us));
    metric.set("avg_show_us",
               static_cast<int64_t>(sum_show_us / static_cast<uint64_t>(samples)));
    metric.set("min_wait_us", static_cast<int64_t>(min_wait_us));
    metric.set("max_wait_us", static_cast<int64_t>(max_wait_us));
    metric.set("avg_wait_us",
               static_cast<int64_t>(sum_wait_us / static_cast<uint64_t>(samples)));
    metric.set("min_total_us", static_cast<int64_t>(min_total_us));
    metric.set("max_total_us", static_cast<int64_t>(max_total_us));
    metric.set("avg_total_us",
               static_cast<int64_t>(sum_total_us / static_cast<uint64_t>(samples)));
    metric.set("min_overhead_us", static_cast<int64_t>(min_overhead_us));
    metric.set("max_overhead_us", static_cast<int64_t>(max_overhead_us));
    metric.set("avg_overhead_us",
               static_cast<int64_t>(sum_overhead_us / static_cast<uint64_t>(samples)));

    out_passed = !timed_out && max_overhead_us <= max_allowed_overhead_us;
    metric.set("passed", out_passed);
    if (timed_out) {
        metric.set("message", "timing wait timeout");
    } else {
        metric.set("message", out_passed ? "tight timing within budget"
                                         : "tight timing exceeded budget");
    }
    return metric;
}

} // namespace

// ============================================================================
// AutoResearchRemoteControl Private Helper Functions
// ============================================================================

fl::json AutoResearchRemoteControl::runSingleTestImpl(const fl::json& args) {
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
    if (lane_sizes_json.size() == 0 || lane_sizes_json.size() > 16) {
        response.set("success", false);
        response.set("error", "InvalidLaneCount");
        response.set("message", "laneSizes must have 1-16 elements");
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

    // 4b. Extract frameCount (optional, default: 1)
    // Number of back-to-back captures of the SAME LED buffer within a single
    // pattern. frameCount >= 2 exposes second-frame degradation bugs where a
    // driver corrupts or zeroes its DMA buffer after the first show() — see
    // issues #2254 and #2288 (ESP32-S3 SPI \"black frame between displays\").
    int frame_count = 1;
    if (config.contains("frameCount") && config["frameCount"].is_int()) {
        frame_count = static_cast<int>(config["frameCount"].as_int().value());
        if (frame_count < 1 || frame_count > 16) {
            response.set("success", false);
            response.set("error", "InvalidFrameCount");
            response.set("message", "frameCount must be in [1, 16]");
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

    // 9. Extract tightTiming (optional, default: false)
    bool measure_tight_timing = false;
    if (config.contains("tightTiming") && config["tightTiming"].is_bool()) {
        measure_tight_timing = config["tightTiming"].as_bool().value();
    }

    int tight_timing_iterations = 8;
    if (config.contains("tightTimingIterations") &&
        config["tightTimingIterations"].is_int()) {
        tight_timing_iterations =
            static_cast<int>(config["tightTimingIterations"].as_int().value());
        if (tight_timing_iterations < 1 || tight_timing_iterations > 64) {
            response.set("success", false);
            response.set("error", "InvalidTightTimingIterations");
            response.set("message", "tightTimingIterations must be in [1, 64]");
            return response;
        }
    }

    uint32_t tight_timing_max_overhead_us = 2000;
    if (config.contains("tightTimingMaxOverheadUs") &&
        config["tightTimingMaxOverheadUs"].is_int()) {
        const int max_overhead =
            static_cast<int>(config["tightTimingMaxOverheadUs"].as_int().value());
        if (max_overhead < 1) {
            response.set("success", false);
            response.set("error", "InvalidTightTimingMaxOverheadUs");
            response.set("message", "tightTimingMaxOverheadUs must be >= 1");
            return response;
        }
        tight_timing_max_overhead_us = static_cast<uint32_t>(max_overhead);
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

    // Set driver as exclusive (by-name path: driver_name comes from RPC)
    if (!autoResearchSetExclusiveDriverByName(driver_name.c_str())) {
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
    fl::ClocklessEncoder resolved_encoder = fl::ClocklessEncoder::CLOCKLESS_ENCODER_WS2812;
    if (use_legacy_api) {
        resolved_timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
        resolved_encoder = fl::encoder_for<fl::TIMING_WS2812_800KHZ>();
        timing_name = "WS2812-800KHZ";
    } else if (timing_name == "UCS7604-800KHZ") {
        resolved_timing = fl::makeTimingConfig<fl::TIMING_UCS7604_800KHZ>();
        resolved_encoder = fl::encoder_for<fl::TIMING_UCS7604_800KHZ>();
    } else {
        resolved_timing = fl::makeTimingConfig<fl::TIMING_WS2812B_V5>();
        resolved_encoder = fl::encoder_for<fl::TIMING_WS2812B_V5>();
    }
    fl::NamedTimingConfig timing_config(resolved_timing, timing_name.c_str(), resolved_encoder);

    // Dynamically allocate LED arrays for each lane
    fl::vector<fl::unique_ptr<fl::vector<CRGB>>> led_arrays;
    fl::vector<fl::ChannelConfig> tx_configs;

    // SPI chipset drivers (LCD_SPI, I2S_SPI) use APA102 protocol with data+clock pins
    // Clockless drivers use WS2812B timing on a single data pin
    bool is_spi_chipset_driver = (driver_name == "LCD_SPI" || driver_name == "I2S_SPI");

    for (fl::size i = 0; i < lane_sizes.size(); i++) {
        auto leds = fl::make_unique<fl::vector<CRGB>>(lane_sizes[i]);
        if (is_spi_chipset_driver) {
            // SPI chipset: APA102 with data pin = pin_tx (lane 0 only)
            // Multi-lane SPI validation is not currently supported because
            // the I80 bus transposes all lanes onto a single bus — only
            // lane 0 (D0) can be captured via the TX→RX jumper wire.
            // Clock pin must avoid TX, RX, and DC (21) pins.
            int data_pin = pin_tx;
            int clock_pin = pin_rx + 1;
            // Use 2.4MHz SPI clock (matches I2S LCD_CAM default, ~417ns/bit)
            // Lower frequencies (e.g., 800kHz) fail with ESP_ERR_NOT_SUPPORTED
            // on the I80 panel IO.
            fl::SpiChipsetConfig spi_cfg(data_pin, clock_pin, fl::SpiEncoder::apa102(2400000));
            tx_configs.push_back(fl::ChannelConfig(
                spi_cfg,
                fl::span<CRGB>(leds->data(), leds->size()),
                RGB
            ));
        } else {
            tx_configs.push_back(fl::ChannelConfig(
                pin_tx + i,  // Consecutive pins for multi-lane
                timing_config.timing,
                fl::span<CRGB>(leds->data(), leds->size()),
                RGB  // Default color order
            ));
        }
        led_arrays.push_back(fl::move(leds));
    }

    // Create temporary RX channel if pinRx differs from default
    fl::shared_ptr<fl::RxChannel> rx_channel_to_use = mState->rx_channel;

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
    fl::AutoResearchConfig autoresearch_config(
        timing_config.timing,
        timing_config.name,
        tx_configs,
        driver_name.c_str(),
        rx_channel_to_use,
        mState->rx_buffer,
        lane_sizes[0],  // base_strip_size (used for logging)
        fl::RxDeviceType::RMT,  // Default RX device type
        timing_config.encoder
    );

    // Run test with debug output suppressed
    int total_tests = 0;
    int passed_tests = 0;
    bool passed = false;
    uint32_t show_duration_ms = 0;
    fl::vector<fl::RunResult> run_results;
    fl::json tight_timing_response = fl::json::object();
    bool tight_timing_passed = true;

    {
        // Keep timing-sensitive hardware validation quiet. On Teensy 4,
        // Serial-backed FL_WARN output can block long enough to starve
        // ObjectFLED's DMA refill path, which is exactly what #3343 is
        // trying to prove or eliminate. JSON-RPC responses bypass this guard.
        fl::ScopedLogDisable quiet_logs;

        if (use_legacy_api) {
            // Legacy API path: WS2812B<PIN> template instantiation
            for (int iter = 0; iter < iterations; iter++) {
                int iter_total = 0, iter_passed = 0;
                autoResearchChipsetTimingLegacy(autoresearch_config, iter_total, iter_passed, show_duration_ms, &run_results, frame_count);
                total_tests += iter_total;
                passed_tests += iter_passed;
            }
        } else {
            // Channel API path: FastLED.add(channel_config)
            for (int iter = 0; iter < iterations; iter++) {
                int iter_total = 0, iter_passed = 0;
                autoResearchChipsetTiming(autoresearch_config, iter_total, iter_passed, show_duration_ms, &run_results, frame_count);
                total_tests += iter_total;
                passed_tests += iter_passed;
            }
        }

        passed = (total_tests > 0) && (passed_tests == total_tests);
    }

    if (measure_tight_timing) {
        if (use_legacy_api) {
            tight_timing_passed = false;
            tight_timing_response.set("requested", true);
            tight_timing_response.set("supported", false);
            tight_timing_response.set("passed", false);
            tight_timing_response.set("driver", driver_name.c_str());
            tight_timing_response.set("message", "legacy API timing metric is not supported");
        } else {
            tight_timing_response = measureTightTiming(
                driver_name,
                timing_config.timing,
                tx_configs,
                tight_timing_iterations,
                tight_timing_max_overhead_us,
                tight_timing_passed);
        }
        bool tight_timing_supported = false;
        if (tight_timing_response.contains("supported") &&
            tight_timing_response["supported"].is_bool()) {
            tight_timing_supported =
                tight_timing_response["supported"].as_bool().value();
        }
        if (tight_timing_supported) {
            passed = passed && tight_timing_passed;
        }
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
    response.set("frameCount", static_cast<int64_t>(frame_count));
    if (measure_tight_timing) {
        response.set("tightTiming", tight_timing_response);
    }

    // Free run_results before building response to reclaim heap
    // Only serialize pattern details when tests FAIL (saves heap on passing tests)
    if (!passed && !run_results.empty()) {
        fl::json patterns = fl::json::array();
        for (fl::size ri = 0; ri < run_results.size(); ri++) {
            const auto& rr = run_results[ri];
            if (rr.passed) continue;  // Skip passing patterns
            fl::json pat = fl::json::object();
            pat.set("runNumber", static_cast<int64_t>(rr.run_number));
            pat.set("totalLeds", static_cast<int64_t>(rr.total_leds));
            pat.set("mismatchedLeds", static_cast<int64_t>(rr.mismatches));
            pat.set("capturedBytes", static_cast<int64_t>(rr.capturedBytes));
            pat.set("captureWaitResult", static_cast<int64_t>(rr.captureWaitResult));
            pat.set("rawEdgesAfterWait", static_cast<int64_t>(rr.rawEdgesAfterWait));
            pat.set("captureFailed", rr.captureFailed);
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

#endif // !(FASTLED_AUTORESEARCH_LOW_MEMORY)
