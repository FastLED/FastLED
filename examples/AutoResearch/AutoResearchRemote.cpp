// examples/AutoResearch/AutoResearchRemote.cpp
//
// Remote RPC control system implementation for AutoResearch sketch.
//
// ARCHITECTURE:
// - RPC responses use printJsonRaw()/printStreamRaw() which bypass fl::println
// - Test execution wrapped in fl::ScopedLogDisable to suppress debug noise
// - This provides clean, parseable JSON output without FL_DBG/FL_PRINT spam

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
#include "fl/stl/atomic.h"
#include "fl/task/promise.h"
#include "fl/math/simd.h"
#include "AutoResearchSimd.h"
#include "AutoResearchAnimartrixBench.h"  // animartrixPerlinBench RPC (#2628 follow-up)
#include "AutoResearchWave8Expand.h"  // #2526 wave8ExpandBenchmark RPC
#include "AutoResearchParlioEncode.h" // parlioEncodeBenchmark RPC (#2526 follow-up)
#include "AutoResearchParlioStream.h" // parlioStreamValidate RPC (#2548 follow-up)
#include "fl/system/heap.h"
#include "fl/chipsets/spi.h"
#include "fl/channels/config.h"
#include <Arduino.h>

#include "fl/net/ble.h"

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
    explicit ScopedFastLedBrightness(uint8_t brightness) FL_NOEXCEPT
        : mSavedBrightness(FastLED.getBrightness()) {
        FastLED.setBrightness(brightness);
    }

    ~ScopedFastLedBrightness() FL_NOEXCEPT {
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
        // Note: ScopedLogDisable removed to enable diagnostic output during test execution.
        // This allows capture/decode debug messages (e.g., RX timing, edge dumps) to appear
        // on serial, which is critical for diagnosing PARLIO failures at different LED counts.

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
                else if (n == "PARLIO")        opts.mBus = fl::Bus::PARLIO;
                else if (n == "SPI")           opts.mBus = fl::Bus::SPI;
                else if (n == "I2S")           opts.mBus = fl::Bus::I2S;
                else if (n == "I2S_SPI")       opts.mBus = fl::Bus::I2S_SPI;
                else if (n == "LCD_RGB")       opts.mBus = fl::Bus::LCD_RGB;
                else if (n == "LCD_SPI")       opts.mBus = fl::Bus::LCD_SPI;
                else if (n == "LCD_CLOCKLESS") opts.mBus = fl::Bus::LCD_CLOCKLESS;
                else if (n == "UART")          opts.mBus = fl::Bus::UART;
                else if (n == "FLEX_IO")       opts.mBus = fl::Bus::FLEX_IO;
                else if (n == "OBJECT_FLED")   opts.mBus = fl::Bus::OBJECT_FLED;
                else if (n == "BIT_BANG")      opts.mBus = fl::Bus::BIT_BANG;
                else if (n == "STUB")          opts.mBus = fl::Bus::STUB;
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

fl::json AutoResearchRemoteControl::findConnectedPinsImpl(const fl::json& args) {
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

AutoResearchRemoteControl::AutoResearchRemoteControl()
    : mRemote(fl::make_unique<fl::Remote>(
        fl::createSerialRequestSource(),
        fl::createSerialResponseSink("REMOTE: ")
    )) {
    // mState will be set by registerFunctions()
}

AutoResearchRemoteControl::~AutoResearchRemoteControl() = default;

void AutoResearchRemoteControl::registerFunctions(fl::shared_ptr<AutoResearchState> state) {
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

    // Register "deliberateHang" - force an infinite loop with interrupts
    // disabled to validate the unified Watchdog implementation (#2731).
    // The watchdog should fire within its configured timeout, reset the
    // device, and let the bootloader become reachable again.
    // Returns success BEFORE hanging so the caller sees the ACK; the hang
    // begins in the next iteration of the loop.
    mRemote->bind("deliberateHang", [this](const fl::json& args) -> fl::json {
        (void)args;
        FL_WARN("[deliberateHang] watchdog test: spinning forever in 200 ms");
        mState->deliberate_hang_requested = true;
        fl::json response = fl::json::object();
        response.set("success", true);
        response.set("message", "device will hang after RPC returns; watchdog should reset within configured timeout");
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

    // Register "flexioRxBenchmark" — square-wave validation for the new
    // FlexIO RX backend (Phase 2 of FastLED#2764).
    //
    // Drives `tx_pin` with a `frequency_hz` square wave via the Teensy core's
    // `analogWriteFrequency` PWM, configures an RxChannel on `rx_pin` using
    // `RxBackend::FLEXIO`, captures for `duration_ms`, and reports per-period
    // statistics: count, mean ns, σ ns, min ns, max ns.
    //
    // Args (positional, all optional with defaults):
    //   {
    //     "frequency_hz": int = 1000,
    //     "duration_ms":  int = 100,
    //     "tx_pin":       int = 3,   // matches GPIO 3↔4 jumper
    //     "rx_pin":       int = 4
    //   }
    //
    // Teensy-4-only because FLEXIO1 is iMXRT1062-specific. Other platforms
    // get a clean "not supported" response so the RPC harness can still
    // round-trip.
    mRemote->bind("flexioRxBenchmark", [this](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();
#if !defined(FL_IS_TEENSY_4X)
        (void)args;
        response.set("success", false);
        response.set("error", "PlatformNotSupported");
        response.set("message",
                     "flexioRxBenchmark is Teensy 4.x-only (FLEXIO1 capture).");
        return response;
#else
        // Parse args
        int frequency_hz = 1000;
        int duration_ms  = 100;
        int tx_pin       = 3;
        int rx_pin       = 4;
        if (args.is_array() && args.size() >= 1 && args[0].is_object()) {
            fl::json cfg = args[0];
            if (cfg.contains("frequency_hz") && cfg["frequency_hz"].is_int()) {
                frequency_hz = static_cast<int>(cfg["frequency_hz"].as_int().value());
            }
            if (cfg.contains("duration_ms") && cfg["duration_ms"].is_int()) {
                duration_ms = static_cast<int>(cfg["duration_ms"].as_int().value());
            }
            if (cfg.contains("tx_pin") && cfg["tx_pin"].is_int()) {
                tx_pin = static_cast<int>(cfg["tx_pin"].as_int().value());
            }
            if (cfg.contains("rx_pin") && cfg["rx_pin"].is_int()) {
                rx_pin = static_cast<int>(cfg["rx_pin"].as_int().value());
            }
        }
        if (frequency_hz < 1 || frequency_hz > 5000000 ||
            duration_ms < 1 || duration_ms > 5000) {
            response.set("success", false);
            response.set("error", "InvalidArgs");
            response.set("message",
                         "frequency_hz in [1, 5_000_000]; duration_ms in [1, 5000].");
            return response;
        }

        // 1. Drive the TX pin with a square wave.
        analogWriteFrequency(tx_pin, (float)frequency_hz);
        analogWrite(tx_pin, 128);  // 50% duty

        // 2. Create the FlexIO RX channel.
        // Width budget — size the edge buffer for the actual capture window:
        //   expected_edges = 2 * frequency_hz * duration_ms / 1000     (transitions)
        //   target = expected_edges * 1.5  (50% headroom for jitter/skew)
        // Clamped to [1024, 16384] so the DMA buffer stays reasonable and
        // matches the FlexIO RX driver's internal cap.
        fl::RxChannelConfig rx_cfg(rx_pin, fl::RxBackend::FLEXIO);
        const fl::u64 expected_edges =
            (fl::u64)2 * (fl::u64)frequency_hz * (fl::u64)duration_ms / 1000ULL;
        fl::u64 target = expected_edges + expected_edges / 2;  // +50%
        if (target < 1024ULL) target = 1024ULL;
        if (target > 16384ULL) target = 16384ULL;
        rx_cfg.edge_capacity = (size_t)target;
        rx_cfg.start_low = false;  // PWM output idles in either state, just track transitions
        auto rx_channel = fl::RxChannel::create(rx_cfg);
        if (!rx_channel) {
            analogWrite(tx_pin, 0);
            response.set("success", false);
            response.set("error", "RxCreateFailed");
            response.set("message",
                         "Failed to create FlexIO RX channel on pin (no FLEXIO1 mapping).");
            return response;
        }
        if (!rx_channel->begin(rx_cfg)) {
            analogWrite(tx_pin, 0);
            response.set("success", false);
            response.set("error", "RxBeginFailed");
            response.set("message",
                         "RxChannel::begin() returned false (see device WARN log).");
            return response;
        }

        // 3. Let it capture for the requested window.
        rx_channel->wait((u32)duration_ms);

        // 4. Stop the square wave so the line is quiet for stats computation.
        analogWrite(tx_pin, 0);

        // 5. Pull captured edges out.
        fl::vector<fl::EdgeTime> edges;
        edges.assign(rx_cfg.edge_capacity, fl::EdgeTime());
        size_t edges_captured =
            rx_channel->getRawEdgeTimes(fl::span<fl::EdgeTime>(edges), 0);
        edges.resize(edges_captured);

        // 6. Compute period statistics. A "period" = duration_high + duration_low
        //    for adjacent edges, so we iterate in pairs.
        fl::u64 sum_ns = 0;
        fl::u64 sum_sq_ns = 0;
        fl::u32 min_ns = 0xFFFFFFFFu;
        fl::u32 max_ns = 0;
        size_t periods = 0;
        for (size_t i = 0; i + 1 < edges_captured; i += 2) {
            const fl::u32 period_ns =
                (fl::u32)edges[i].ns + (fl::u32)edges[i + 1].ns;
            if (period_ns == 0) continue;
            sum_ns += period_ns;
            sum_sq_ns += (fl::u64)period_ns * (fl::u64)period_ns;
            if (period_ns < min_ns) min_ns = period_ns;
            if (period_ns > max_ns) max_ns = period_ns;
            ++periods;
        }
        fl::u32 mean_ns = 0;
        fl::u32 sigma_ns = 0;
        if (periods > 0) {
            mean_ns = (fl::u32)(sum_ns / (fl::u64)periods);
            const fl::u64 mean64 = (fl::u64)mean_ns;
            const fl::u64 var =
                periods > 0 ? (sum_sq_ns / (fl::u64)periods) - (mean64 * mean64)
                            : 0;
            // Integer sqrt for σ — adequate for the tolerance ranges we
            // assert in Phase 2 (σ < 100 ns at 100 kHz).
            fl::u32 s = 0;
            while ((fl::u64)(s + 1) * (fl::u64)(s + 1) <= var) ++s;
            sigma_ns = s;
        }
        if (min_ns == 0xFFFFFFFFu) min_ns = 0;

        response.set("success", true);
        response.set("frequency_hz", static_cast<int64_t>(frequency_hz));
        response.set("duration_ms", static_cast<int64_t>(duration_ms));
        response.set("tx_pin", static_cast<int64_t>(tx_pin));
        response.set("rx_pin", static_cast<int64_t>(rx_pin));
        response.set("edges_captured", static_cast<int64_t>(edges_captured));
        response.set("periods", static_cast<int64_t>(periods));
        response.set("period_mean_ns", static_cast<int64_t>(mean_ns));
        response.set("period_sigma_ns", static_cast<int64_t>(sigma_ns));
        response.set("period_min_ns", static_cast<int64_t>(min_ns));
        response.set("period_max_ns", static_cast<int64_t>(max_ns));
        return response;
#endif
    });

    // Register "flexioObjectFledTest" — end-to-end ObjectFLED TX → FlexIO RX
    // loopback verification (Phase 3 of FastLED#2764).
    //
    // Drives a small WS2812 pattern through `Bus::OBJECT_FLED` on `tx_pin`,
    // captures the wire signal back through the new `RxBackend::FLEXIO` on
    // `rx_pin`, decodes the bit stream against WS2812B-V5 timing, and reports
    // how many bytes matched the transmitted pattern.
    //
    // Test cases (parent issue Phase 3 table):
    //   0 — Red single LED            (1 LED, 0xFF0000 → wire-order GRB 00,FF,00)
    //   1 — RGB three-LED chain       (3 LEDs)
    //   2 — All zeros                 (1 LED, 0x000000 — T0H/T0L fidelity)
    //   3 — All ones                  (1 LED, 0xFFFFFF — T1H/T1L fidelity)
    //   4 — 100-LED alternating R/G/B (long capture, watchdog-safety)
    //
    // Args (positional, all optional):
    //   { "test_case": int = 0,
    //     "tx_pin":    int = 3,   // matches GPIO 3↔4 jumper
    //     "rx_pin":    int = 4,
    //     "capture_ms": int = 50 }
    //
    // Returns:
    //   { success, test_case, num_leds, expected_bytes, decoded_bytes,
    //     matched, mismatched, edges_captured }
    //
    // Teensy-4-only — FLEXIO1 is iMXRT1062-specific. ObjectFLED also relies
    // on Teensy 4-core APIs, so non-Teensy builds return `PlatformNotSupported`.
    mRemote->bind("flexioObjectFledTest", [this](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();
#if !defined(FL_IS_TEENSY_4X)
        (void)args;
        response.set("success", false);
        response.set("error", "PlatformNotSupported");
        response.set("message",
                     "flexioObjectFledTest is Teensy 4.x-only (FLEXIO1 RX + "
                     "Teensy-core ObjectFLED driver).");
        return response;
#else
        // Parse args
        int test_case = 0;
        int tx_pin    = 3;
        int rx_pin    = 4;
        int capture_ms = 50;
        if (args.is_array() && args.size() >= 1 && args[0].is_object()) {
            fl::json cfg = args[0];
            if (cfg.contains("test_case") && cfg["test_case"].is_int()) {
                test_case = static_cast<int>(cfg["test_case"].as_int().value());
            }
            if (cfg.contains("tx_pin") && cfg["tx_pin"].is_int()) {
                tx_pin = static_cast<int>(cfg["tx_pin"].as_int().value());
            }
            if (cfg.contains("rx_pin") && cfg["rx_pin"].is_int()) {
                rx_pin = static_cast<int>(cfg["rx_pin"].as_int().value());
            }
            if (cfg.contains("capture_ms") && cfg["capture_ms"].is_int()) {
                capture_ms = static_cast<int>(cfg["capture_ms"].as_int().value());
            }
        }
        if (test_case < 0 || test_case > 4 ||
            capture_ms < 1 || capture_ms > 5000) {
            response.set("success", false);
            response.set("error", "InvalidArgs");
            response.set("message",
                         "test_case in [0,4]; capture_ms in [1, 5000].");
            return response;
        }

        // 1. Build the test pattern. Fixed upper bound (100 LEDs) covers
        //    case 4 — the larger cases reuse the same static buffer.
        static CRGB leds_buf[100];
        int num_leds = 0;
        switch (test_case) {
            case 0:
                num_leds = 1;
                leds_buf[0] = CRGB::Red;
                break;
            case 1:
                num_leds = 3;
                leds_buf[0] = CRGB::Red;
                leds_buf[1] = CRGB::Green;
                leds_buf[2] = CRGB::Blue;
                break;
            case 2:
                num_leds = 1;
                leds_buf[0] = CRGB::Black;
                break;
            case 3:
                num_leds = 1;
                leds_buf[0] = CRGB(0xFF, 0xFF, 0xFF);
                break;
            case 4:
                num_leds = 100;
                for (int i = 0; i < 100; ++i) {
                    leds_buf[i] = (i % 3 == 0) ? CRGB::Red
                                : (i % 3 == 1) ? CRGB::Green
                                               : CRGB::Blue;
                }
                break;
        }

        // 2. Build the expected wire-order byte stream (WS2812 is GRB).
        fl::vector<u8> expected;
        expected.reserve((size_t)num_leds * 3);
        for (int i = 0; i < num_leds; ++i) {
            expected.push_back(leds_buf[i].g);
            expected.push_back(leds_buf[i].r);
            expected.push_back(leds_buf[i].b);
        }

        // 3. Set up FlexIO RX. Size the edge buffer for 24 bits per LED ×
        //    2 transitions per bit, with 25% headroom.
        const size_t expected_edges = (size_t)num_leds * 24u * 2u;
        size_t edge_capacity = expected_edges + expected_edges / 4u + 64u;
        if (edge_capacity > 16384u) edge_capacity = 16384u;

        fl::RxChannelConfig rx_cfg(rx_pin, fl::RxBackend::FLEXIO);
        rx_cfg.edge_capacity = edge_capacity;
        rx_cfg.start_low = true;
        auto rx_channel = fl::RxChannel::create(rx_cfg);
        if (!rx_channel || !rx_channel->begin(rx_cfg)) {
            response.set("success", false);
            response.set("error", "RxBeginFailed");
            response.set("message",
                         "Failed to bring up FlexIO RX on the requested pin "
                         "(check kFlexIo1Pins[] mapping).");
            return response;
        }

        // 4. Configure ObjectFLED TX via FastLED.add().
        fl::ChannelOptions opts;
        opts.mBus = fl::Bus::OBJECT_FLED;
        auto resolved_timing  = fl::makeTimingConfig<fl::TIMING_WS2812B_V5>();
        auto resolved_encoder = fl::encoder_for<fl::TIMING_WS2812B_V5>();
        fl::NamedTimingConfig timing_cfg(resolved_timing, "WS2812B-V5",
                                         resolved_encoder);
        fl::ChannelConfig tx_cfg(
            tx_pin,
            timing_cfg.timing,
            fl::span<CRGB>(leds_buf, (size_t)num_leds),
            RGB,
            opts);
        auto tx_channel = FastLED.add(tx_cfg);
        if (!tx_channel) {
            response.set("success", false);
            response.set("error", "TxAddFailed");
            response.set("message",
                         "FastLED.add() rejected the ObjectFLED ChannelConfig.");
            return response;
        }

        // 5. Trigger TX. FastLED.show() schedules the DMA and returns once
        //    the frame has been queued. The RX wait must be long enough to
        //    cover BOTH the TX transmission time AND the capture-buffer
        //    fill — otherwise teardown happens mid-frame and the hardware
        //    can be left in a bad state. Compute a minimum from the actual
        //    WS2812 timing (1.25 µs per bit, 24 bits per LED, plus reset
        //    gap) and use the larger of the caller's `capture_ms` and that
        //    minimum.
        FastLED.show();
        const u32 ws2812_tx_us =
            (u32)num_leds * 24u * 13u / 10u + 100u;  // 1.25 µs/bit + reset
        const u32 min_capture_ms = (ws2812_tx_us + 999u) / 1000u + 10u;
        const u32 effective_capture_ms = ((u32)capture_ms > min_capture_ms)
                                              ? (u32)capture_ms
                                              : min_capture_ms;
        rx_channel->wait(effective_capture_ms);

        // 6. Decode the captured edge stream against WS2812 4-phase timing.
        const fl::ChipsetTiming ws2812_timing =
            fl::to_runtime_timing<fl::TIMING_WS2812B_V5>();
        fl::ChipsetTiming4Phase rx_timing =
            fl::make4PhaseTiming(ws2812_timing);
        fl::vector<u8> decoded;
        decoded.assign(expected.size() + 16u, 0u);
        auto decode_result =
            rx_channel->decode(rx_timing, fl::span<u8>(decoded));

        // 7. Compare decoded bytes against expected.
        u32 decoded_bytes = 0u;
        if (decode_result) {
            decoded_bytes = decode_result.value();
        }
        const size_t cmp_n = (decoded_bytes < expected.size())
                                 ? (size_t)decoded_bytes
                                 : expected.size();
        size_t matched = 0;
        size_t mismatched = 0;
        for (size_t i = 0; i < cmp_n; ++i) {
            if (decoded[i] == expected[i]) ++matched; else ++mismatched;
        }
        if (decoded_bytes < expected.size()) {
            mismatched += expected.size() - decoded_bytes;
        }

        // 8. Read raw edge count for diagnostics.
        fl::vector<fl::EdgeTime> edges;
        edges.assign(edge_capacity, fl::EdgeTime());
        const size_t edges_captured =
            rx_channel->getRawEdgeTimes(fl::span<fl::EdgeTime>(edges), 0);

        // 9. Tear the TX channel back down so subsequent tests can use the
        //    same pin (FastLED.clear(ClearFlags::CHANNELS) matches the
        //    pattern used by runSingleTestImpl in this file).
        FastLED.clear(ClearFlags::CHANNELS);

        response.set("success", mismatched == 0 && decoded_bytes == expected.size());
        response.set("test_case", static_cast<int64_t>(test_case));
        response.set("tx_pin", static_cast<int64_t>(tx_pin));
        response.set("rx_pin", static_cast<int64_t>(rx_pin));
        response.set("num_leds", static_cast<int64_t>(num_leds));
        response.set("expected_bytes", static_cast<int64_t>(expected.size()));
        response.set("decoded_bytes", static_cast<int64_t>(decoded_bytes));
        response.set("matched", static_cast<int64_t>(matched));
        response.set("mismatched", static_cast<int64_t>(mismatched));
        response.set("edges_captured", static_cast<int64_t>(edges_captured));
        return response;
#endif
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
        setLaneRange_fn.set("description", "Configure lane range (1-16)");
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

        fl::json animartrixPerlinBench_fn = fl::json::object();
        animartrixPerlinBench_fn.set("name", "animartrixPerlinBench");
        animartrixPerlinBench_fn.set("phase", "Phase 4: Utility");
        animartrixPerlinBench_fn.set("args", "[{iterations}] (optional, default 100, max 10000)");
        animartrixPerlinBench_fn.set("returns", "{success, iterations, pnoise_calls_per_iter, pnoise_float_us, pnoise_i16_us, speedup_x1000}");
        animartrixPerlinBench_fn.set("description", "Animartrix-representative Perlin noise bench: scalar float pnoise vs s16x16 fixed-point pnoise2d (16x16 grid per iter, mirrors a real frame). Answers: how much does fixed-point beat float for Animartrix's hot path on this hardware?");
        functions.push_back(animartrixPerlinBench_fn);

        fl::json wave8ExpandBenchmark_fn = fl::json::object();
        wave8ExpandBenchmark_fn.set("name", "wave8ExpandBenchmark");
        wave8ExpandBenchmark_fn.set("phase", "Phase 4: Utility");
        wave8ExpandBenchmark_fn.set("args", "[{iterations}] (optional, default 30000, max 200000)");
        wave8ExpandBenchmark_fn.set("returns", "{success, iterations, expand_nibble_us, expand_byte_us, expand_batched_us, transpose16_nibble_us, transpose16_byte_us, sink}");
        wave8ExpandBenchmark_fn.set("description", "Bench PARLIO Wave8 expansion (#2526): nibble vs byte vs batched LUT, plus full per-byte-position cost (expansion + 16-lane transpose)");
        functions.push_back(wave8ExpandBenchmark_fn);

        fl::json parlioEncodeBenchmark_fn = fl::json::object();
        parlioEncodeBenchmark_fn.set("name", "parlioEncodeBenchmark");
        parlioEncodeBenchmark_fn.set("phase", "Phase 4: Utility");
        parlioEncodeBenchmark_fn.set("args", "[{iterations}] (optional, default 12000, max 200000)");
        parlioEncodeBenchmark_fn.set("returns", "{success, iters, lanes, leds_per_lane, scratchPsramOk, outputPsramOk, perpos_ss_us, perpos_sp_us, perpos_ps_us, perpos_pp_us, frame_ss_us, frame_sp_us, frame_ps_us, frame_pp_us, sink}");
        parlioEncodeBenchmark_fn.set("description", "Bench full PARLIO encode hot loop (16-lane gather + BF1 pipe4 direct encode) with SRAM and optional PSRAM placements; answers PSRAM hypothesis + ISR-streaming feasibility");
        functions.push_back(parlioEncodeBenchmark_fn);

        fl::json parlioStreamValidate_fn = fl::json::object();
        parlioStreamValidate_fn.set("name", "parlioStreamValidate");
        parlioStreamValidate_fn.set("phase", "Phase 4: Utility");
        parlioStreamValidate_fn.set("args", "[{baseTxPin, txPins, numLanes, numLeds, iterations, timeoutMs}] (all optional; txPins overrides contiguous baseTxPin)");
        parlioStreamValidate_fn.set("returns", "{success, completed, baseTxPin, txPins, lanes, leds_per_lane, iterations, perIterUs:[...], steadyAvgUs, failedIter, underrunCount, txDoneCount, workerIsrCount, ringError, hardwareIdle}");
        parlioStreamValidate_fn.set("description", "Functional test of the PARLIO ISR-chunked streaming engine (#2548). Drives N back-to-back FastLED.show() calls through the production engine (which uses BF1+pipe4 on 16-lane Wave8 since #2559) and verifies all complete within timeout. Catches hangs/stalls.");
        functions.push_back(parlioStreamValidate_fn);

        fl::json flexioRxBenchmark_fn = fl::json::object();
        flexioRxBenchmark_fn.set("name", "flexioRxBenchmark");
        flexioRxBenchmark_fn.set("phase", "Phase 4: Utility");
        flexioRxBenchmark_fn.set("args", "[{frequency_hz=1000, duration_ms=100, tx_pin=3, rx_pin=4}] (all optional)");
        flexioRxBenchmark_fn.set("returns", "{success, frequency_hz, duration_ms, tx_pin, rx_pin, edges_captured, periods, period_mean_ns, period_sigma_ns, period_min_ns, period_max_ns}");
        flexioRxBenchmark_fn.set("description", "Square-wave validation for the FlexIO RX backend (Teensy 4.x only, FastLED#2764 Phase 2). Drives tx_pin via analogWriteFrequency at 50%% duty, captures via RxBackend::FLEXIO on rx_pin, reports per-period statistics.");
        functions.push_back(flexioRxBenchmark_fn);

        fl::json flexioObjectFledTest_fn = fl::json::object();
        flexioObjectFledTest_fn.set("name", "flexioObjectFledTest");
        flexioObjectFledTest_fn.set("phase", "Phase 4: Utility");
        flexioObjectFledTest_fn.set("args", "[{test_case=0..4, tx_pin=3, rx_pin=4, capture_ms=50}] (all optional)");
        flexioObjectFledTest_fn.set("returns", "{success, test_case, tx_pin, rx_pin, num_leds, expected_bytes, decoded_bytes, matched, mismatched, edges_captured}");
        flexioObjectFledTest_fn.set("description", "End-to-end ObjectFLED TX -> FlexIO RX loopback verification (Teensy 4.x only, FastLED#2764 Phase 3). Drives WS2812 patterns through Bus::OBJECT_FLED, captures via RxBackend::FLEXIO, decodes the bit stream, and reports byte-level match counts. Five fixed test patterns: 0=red, 1=RGB triple, 2=all zeros, 3=all ones, 4=100-LED alternating.");
        functions.push_back(flexioObjectFledTest_fn);

        fl::json response = fl::json::object();
        response.set("success", true);
        response.set("totalFunctions", static_cast<int64_t>(28));
        response.set("functions", functions);
        return response;
    });

    // Register "testSimd" function - run comprehensive SIMD test suite
    mRemote->bind("testSimd", [](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();

        // Run the full test suite and collect per-test results
        using autoresearch::simd_check::SimdTestEntry;
        const SimdTestEntry* tests = nullptr;
        int num_tests = 0;
        autoresearch::simd_check::getTests(&tests, &num_tests);

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

        auto result = autoresearch::simd_check::runMultiplyBenchmark(iters);

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

    // Register "animartrixPerlinBench" - Animartrix-representative Perlin
    // noise bench: scalar float (fl::pnoise) vs s16x16 fixed-point
    // (fl::perlin_i16_optimized::pnoise2d). Same workload that drives every
    // Animartrix frame — one Perlin lookup per output pixel, 16x16 grid
    // per iteration. Args: {iterations} (optional, default 100).
    mRemote->bind("animartrixPerlinBench", [](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();

        int iters = 100;
        fl::json config;
        if (args.is_object()) {
            config = args;
        } else if (args.is_array() && args.size() >= 1 && args[0].is_object()) {
            config = args[0];
        }
        if (!config.is_null() && config.contains("iterations") && config["iterations"].is_int()) {
            iters = static_cast<int>(config["iterations"].as_int().value());
            if (iters < 1) iters = 1;
            if (iters > 10000) iters = 10000;
        }

        auto result = autoresearch::animartrix_check::runPerlinBenchmark(iters);

        response.set("success", true);
        response.set("iterations", result.iterations);
        // Workload-per-iter is 16*16 = 256 pnoise calls. Surface this so
        // the client can compute per-call timings without hardcoding.
        response.set("pnoise_calls_per_iter", static_cast<int64_t>(256));
        response.set("pnoise_float_us", result.pnoise_float_us);
        response.set("pnoise_i16_us", result.pnoise_i16_us);
        // Speedup expressed as float / i16 (>1 means i16 wins). Computed
        // host-side too for cross-check; this is just convenience.
        if (result.pnoise_i16_us > 0) {
            double speedup = static_cast<double>(result.pnoise_float_us) /
                             static_cast<double>(result.pnoise_i16_us);
            // Encode as basis-points (1000ths) to avoid float in the json wire fmt
            response.set("speedup_x1000", static_cast<int64_t>(speedup * 1000.0));
        }
        return response;
    });

    // Register "wave8ExpandBenchmark" - PARLIO Wave8 expansion bench (#2526).
    // Compares nibble-LUT vs byte-LUT vs batched byte-LUT, and times the full
    // per-byte-position cost (expansion + 16-lane transpose) for both LUTs.
    // Args: {iterations} (optional, default 30000, max 200000)
    mRemote->bind("wave8ExpandBenchmark", [](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();

        int iters = 30000;
        fl::json config;
        if (args.is_object()) {
            config = args;
        } else if (args.is_array() && args.size() >= 1 && args[0].is_object()) {
            config = args[0];
        }
        if (!config.is_null() && config.contains("iterations") && config["iterations"].is_int()) {
            iters = static_cast<int>(config["iterations"].as_int().value());
        }

        auto r = autoresearch::wave8_bench::measureWave8Expand(iters);

        response.set("success", true);
        response.set("iterations", static_cast<int64_t>(r.iters));
        response.set("expand_nibble_us", static_cast<int64_t>(r.expand_nibble_us));
        response.set("expand_byte_us", static_cast<int64_t>(r.expand_byte_us));
        response.set("expand_batched_us", static_cast<int64_t>(r.expand_batched_us));
        response.set("transpose16_nibble_us", static_cast<int64_t>(r.transpose16_nibble_us));
        response.set("transpose16_byte_us", static_cast<int64_t>(r.transpose16_byte_us));
        response.set("sink", static_cast<int64_t>(r.sink));
        return response;
    });

    // Register "parlioStreamValidate" - functional test of the production
    // PARLIO ISR-chunked streaming engine (#2548). Exercises the 16-lane Wave8
    // path which dispatches to wave8Transpose_16x4_bf1_pipe4 (BF1, #2559).
    // Drives N back-to-back FastLED.show() cycles and verifies each completes
    // within timeout. Returns per-iter timing so the host can diagnose stalls.
    mRemote->bind("parlioStreamValidate", [this](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();
        auto invalidArgs = [](const char* message) -> fl::json {
            fl::json error = fl::json::object();
            error.set("success", false);
            error.set("error", "InvalidArgs");
            error.set("message", message);
            return error;
        };

        int num_lanes = 16;
        int num_leds = 256;
        int iterations = 5;
        int timeout_ms = 200;
        int base_tx_pin = mState->pin_tx;
        int tx_pins[autoresearch::parlio_stream::kMaxLanes];
        bool has_tx_pins = false;
        bool num_lanes_provided = false;
        for (int i = 0; i < autoresearch::parlio_stream::kMaxLanes; ++i) {
            tx_pins[i] = -1;
        }

        fl::json config;
        if (args.is_object()) {
            config = args;
        } else if (args.is_array() && args.size() >= 1 && args[0].is_object()) {
            config = args[0];
        }
        if (!config.is_null()) {
            if (config.contains("baseTxPin") && config["baseTxPin"].is_int())
                base_tx_pin = static_cast<int>(config["baseTxPin"].as_int().value());
            if (config.contains("numLanes")) {
                if (!config["numLanes"].is_int()) {
                    return invalidArgs("numLanes must be an integer");
                }
                num_lanes = static_cast<int>(config["numLanes"].as_int().value());
                num_lanes_provided = true;
            }
            if (config.contains("numLeds") && config["numLeds"].is_int())
                num_leds = static_cast<int>(config["numLeds"].as_int().value());
            if (config.contains("iterations") && config["iterations"].is_int())
                iterations = static_cast<int>(config["iterations"].as_int().value());
            if (config.contains("timeoutMs") && config["timeoutMs"].is_int())
                timeout_ms = static_cast<int>(config["timeoutMs"].as_int().value());
            if (config.contains("txPins")) {
                if (!config["txPins"].is_array()) {
                    return invalidArgs("txPins must be an integer array");
                }
                if (!num_lanes_provided) {
                    return invalidArgs("txPins requires numLanes");
                }
                if (num_lanes < 1 ||
                    num_lanes > autoresearch::parlio_stream::kMaxLanes) {
                    return invalidArgs("numLanes out of range for txPins");
                }

                const fl::json pins = config["txPins"];
                if (pins.size() != static_cast<size_t>(num_lanes)) {
                    return invalidArgs("txPins length must match numLanes");
                }

                for (int i = 0; i < num_lanes; ++i) {
                    if (!pins[i].is_int()) {
                        return invalidArgs("txPins entries must be integers");
                    }
                    int64_t pin_value = pins[i].as_int().value();
                    if (pin_value < 0 || pin_value >= 64) {
                        return invalidArgs("txPins entries must be in range 0..63");
                    }
                    tx_pins[i] = static_cast<int>(pin_value);
                }
                has_tx_pins = true;
                base_tx_pin = tx_pins[0];
            }
        }

        // Clamp inputs to safe ranges.
        if (base_tx_pin < 0) base_tx_pin = 0;
        if (num_lanes < 1) num_lanes = 1;
        if (num_lanes > 16) num_lanes = 16;
        if (num_leds < 1) num_leds = 1;
        if (num_leds > 256) num_leds = 256;
        if (iterations < 1) iterations = 1;
        if (iterations > autoresearch::parlio_stream::kMaxIterations) {
            iterations = autoresearch::parlio_stream::kMaxIterations;
        }
        if (timeout_ms < 1) timeout_ms = 1;
        if (timeout_ms > 5000) timeout_ms = 5000;

        auto r = autoresearch::parlio_stream::validateParlioStreaming(
            base_tx_pin, num_lanes, num_leds, iterations,
            static_cast<uint32_t>(timeout_ms),
            has_tx_pins ? tx_pins : nullptr);

        response.set("success", true);
        response.set("channelsOk", r.channels_ok);
        response.set("completed", r.completed);
        response.set("baseTxPin", static_cast<int64_t>(r.base_tx_pin));
        int last_tx_pin = r.base_tx_pin + r.lanes - 1;
        if (r.explicit_tx_pins) {
            for (int i = r.lanes - 1; i >= 0; --i) {
                if (r.tx_pins[i] >= 0) {
                    last_tx_pin = r.tx_pins[i];
                    break;
                }
            }
        }
        response.set("lastTxPin", static_cast<int64_t>(last_tx_pin));
        response.set("explicitTxPins", r.explicit_tx_pins);
        response.set("lanes", static_cast<int64_t>(r.lanes));
        response.set("ledsPerLane", static_cast<int64_t>(r.leds_per_lane));
        response.set("iterations", static_cast<int64_t>(r.iterations));
        response.set("steadyAvgUs", static_cast<int64_t>(r.steady_avg_us));
        response.set("steadyAvgShowUs", static_cast<int64_t>(r.steady_avg_show_us));
        response.set("steadyAvgWaitUs", static_cast<int64_t>(r.steady_avg_wait_us));
        response.set("failedIter", static_cast<int64_t>(r.failed_iter));
        response.set("timeoutMs", static_cast<int64_t>(r.timeout_ms));
        response.set("txDoneCount", static_cast<int64_t>(r.tx_done_count));
        response.set("workerIsrCount", static_cast<int64_t>(r.worker_isr_count));
        response.set("underrunCount", static_cast<int64_t>(r.underrun_count));
        response.set("ringCount", static_cast<int64_t>(r.ring_count));
        response.set("bytesTotal", static_cast<int64_t>(r.bytes_total));
        response.set("bytesTransmitted", static_cast<int64_t>(r.bytes_transmitted));
        response.set("ringError", r.ring_error);
        response.set("hardwareIdle", r.hardware_idle);
        fl::json response_tx_pins = fl::json::array();
        for (int i = 0; i < r.lanes; ++i) {
            response_tx_pins.push_back(static_cast<int64_t>(r.tx_pins[i]));
        }
        response.set("txPins", response_tx_pins);
        fl::json per_iter = fl::json::array();
        fl::json per_iter_show = fl::json::array();
        fl::json per_iter_wait = fl::json::array();
        for (int i = 0; i < r.iterations; ++i) {
            per_iter.push_back(static_cast<int64_t>(r.per_iter_us[i]));
            per_iter_show.push_back(static_cast<int64_t>(r.per_iter_show_us[i]));
            per_iter_wait.push_back(static_cast<int64_t>(r.per_iter_wait_us[i]));
        }
        response.set("perIterUs", per_iter);
        response.set("perIterShowUs", per_iter_show);
        response.set("perIterWaitUs", per_iter_wait);
        return response;
    });

    // Register "parlioEncodeBenchmark" - full PARLIO encode hot-loop bench with
    // {scratch, output} in SRAM/PSRAM (4 combinations). Answers the PSRAM
    // hypothesis + ISR-streaming feasibility on the byte-LUT path (#2526
    // follow-up).
    mRemote->bind("parlioEncodeBenchmark", [](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();

        int iters = 12000;
        fl::json config;
        if (args.is_object()) {
            config = args;
        } else if (args.is_array() && args.size() >= 1 && args[0].is_object()) {
            config = args[0];
        }
        if (!config.is_null() && config.contains("iterations") && config["iterations"].is_int()) {
            iters = static_cast<int>(config["iterations"].as_int().value());
        }

        auto r = autoresearch::parlio_bench::measureParlioEncode(iters);

        response.set("success", r.iters > 0);
        response.set("iters", static_cast<int64_t>(r.iters));
        response.set("lanes", static_cast<int64_t>(r.lanes));
        response.set("leds_per_lane", static_cast<int64_t>(r.leds_per_lane));
        response.set("scratchPsramOk", r.scratch_psram_ok);
        response.set("outputPsramOk", r.output_psram_ok);
        response.set("perpos_ss_us", static_cast<int64_t>(r.perpos_ss_us));
        response.set("perpos_sp_us", static_cast<int64_t>(r.perpos_sp_us));
        response.set("perpos_ps_us", static_cast<int64_t>(r.perpos_ps_us));
        response.set("perpos_pp_us", static_cast<int64_t>(r.perpos_pp_us));
        if (r.iters > 0) {
            constexpr fl::u32 kFrameBytePositions = 256 * 3;
            response.set("frame_ss_us", static_cast<int64_t>(
                static_cast<fl::u64>(r.perpos_ss_us) * kFrameBytePositions / r.iters));
            response.set("frame_sp_us", static_cast<int64_t>(
                static_cast<fl::u64>(r.perpos_sp_us) * kFrameBytePositions / r.iters));
            response.set("frame_ps_us", static_cast<int64_t>(
                static_cast<fl::u64>(r.perpos_ps_us) * kFrameBytePositions / r.iters));
            response.set("frame_pp_us", static_cast<int64_t>(
                static_cast<fl::u64>(r.perpos_pp_us) * kFrameBytePositions / r.iters));
        }
        response.set("sink", static_cast<int64_t>(r.sink));
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

        // Set exclusive driver if requested (by-name path: requested_driver from RPC)
        if (!requested_driver.empty()) {
            if (!autoResearchSetExclusiveDriverByName(requested_driver.c_str())) {
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
        response.set("ble_active", mState->ble_server_active);
        fl::net::ble::StatusInfo info = fl::net::ble::queryStatus(mBleState);
        response.set("connected", info.connected);
        response.set("connected_count", static_cast<int64_t>(info.connectedCount));
        response.set("tx_char_exists", info.txCharExists);
        response.set("tx_value_len", static_cast<int64_t>(info.txValueLen));
        response.set("ring_head", static_cast<int64_t>(info.ringHead));
        response.set("ring_tail", static_cast<int64_t>(info.ringTail));
        return response;
    });

    // ========================================================================
    // Coroutine Tests - fl::task::coroutine() and fl::task::await()
    // ========================================================================

    // Test: Basic coroutine creation and completion
    mRemote->bind("testCoroutineBasic", [](const fl::json& args) -> fl::json {
        (void)args;
        fl::json r = fl::json::object();

        fl::atomic<bool> task_ran(false);
        fl::atomic<bool> task_completed(false);

        fl::task::CoroutineConfig cfg;
        cfg.func = [&task_ran, &task_completed]() {
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

        fl::task::CoroutineConfig cfg;
        cfg.func = [&task_started]() {
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

        fl::task::Handle tasks[NUM_TASKS];
        for (int i = 0; i < NUM_TASKS; i++) {
            fl::task::CoroutineConfig cfg;
            cfg.func = [i, &task_flags, &completed_count]() {
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
    // Verifies fl::task::await() truly blocks until the producer resolves the promise.
    // Main thread does NOT touch the promise — only the producer coroutine does.
    mRemote->bind("testCoroutineAwait", [](const fl::json& args) -> fl::json {
        (void)args;
        fl::json r = fl::json::object();

        auto promise_ptr = fl::make_shared<fl::task::Promise<int>>(fl::task::Promise<int>::create());
        fl::atomic<bool> consumer_started(false);
        fl::atomic<bool> consumer_finished(false);
        fl::atomic<int> consumer_value(0);
        fl::atomic<bool> consumer_ok(false);
        fl::atomic<bool> producer_started(false);
        fl::atomic<bool> producer_finished(false);

        // Consumer coroutine: starts first, calls fl::task::await() which should block
        // until the producer coroutine resolves the promise
        fl::task::CoroutineConfig consumer_cfg;
        consumer_cfg.func = [promise_ptr, &consumer_started, &consumer_finished,
                                 &consumer_value, &consumer_ok]() {
            consumer_started.store(true);
            // This should block the coroutine until producer fulfills the promise
            auto result = fl::task::await(*promise_ptr);
            if (result.ok()) {
                consumer_ok.store(true);
                consumer_value.store(result.value());
            }
            consumer_finished.store(true);
        };
        consumer_cfg.name = "await_consumer";
        auto consumer = fl::task::coroutine(consumer_cfg);

        // Small delay so consumer enters fl::task::await() before producer starts
        delay(50);

        // Producer coroutine: simulates async work, then resolves the promise
        fl::task::CoroutineConfig producer_cfg;
        producer_cfg.func = [promise_ptr, &producer_started, &producer_finished]() {
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
    // Verifies that fl::task::await() properly propagates errors from producer.
    mRemote->bind("testCoroutineAwaitError", [](const fl::json& args) -> fl::json {
        (void)args;
        fl::json r = fl::json::object();

        auto promise_ptr = fl::make_shared<fl::task::Promise<int>>(fl::task::Promise<int>::create());
        fl::atomic<bool> consumer_finished(false);
        fl::atomic<bool> got_error(false);
        fl::atomic<bool> producer_finished(false);

        // Consumer coroutine: awaits promise, expects error
        fl::task::CoroutineConfig consumer_cfg;
        consumer_cfg.func = [promise_ptr, &consumer_finished, &got_error]() {
            auto result = fl::task::await(*promise_ptr);
            if (!result.ok()) {
                got_error.store(true);
            }
            consumer_finished.store(true);
        };
        consumer_cfg.name = "await_err_consumer";
        auto consumer = fl::task::coroutine(consumer_cfg);

        delay(50);  // let consumer enter await

        // Producer coroutine: rejects the promise with error
        fl::task::CoroutineConfig producer_cfg;
        producer_cfg.func = [promise_ptr, &producer_finished]() {
            delay(100);
            promise_ptr->complete_with_error(fl::task::Error("test error"));
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

        auto promise_ptr = fl::make_shared<fl::task::Promise<int>>(fl::task::Promise<int>::create());
        fl::atomic<bool> then_called(false);
        fl::atomic<int> then_value(0);
        fl::atomic<bool> catch_called(false);
        fl::atomic<bool> producer_finished(false);

        // Attach callbacks to the promise BEFORE producer runs
        promise_ptr->then([&then_called, &then_value](const int& val) {
            then_called.store(true);
            then_value.store(val);
        });
        promise_ptr->catch_([&catch_called](const fl::task::Error&) {
            catch_called.store(true);
        });

        // Producer coroutine fulfills the promise
        fl::task::CoroutineConfig producer_cfg;
        producer_cfg.func = [promise_ptr, &producer_finished]() {
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

        auto promise_ptr = fl::make_shared<fl::task::Promise<int>>(fl::task::Promise<int>::create());
        fl::atomic<bool> then_called(false);
        fl::atomic<bool> catch_called(false);
        fl::atomic<bool> producer_finished(false);

        promise_ptr->then([&then_called](const int&) {
            then_called.store(true);
        });
        promise_ptr->catch_([&catch_called](const fl::task::Error&) {
            catch_called.store(true);
        });

        // Producer coroutine rejects the promise
        fl::task::CoroutineConfig producer_cfg;
        producer_cfg.func = [promise_ptr, &producer_finished]() {
            delay(100);
            promise_ptr->complete_with_error(fl::task::Error("rejection test"));
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

        auto p1 = fl::make_shared<fl::task::Promise<int>>(fl::task::Promise<int>::create());
        auto p2 = fl::make_shared<fl::task::Promise<int>>(fl::task::Promise<int>::create());
        fl::atomic<int> final_value(0);
        fl::atomic<bool> chain_complete(false);
        fl::atomic<bool> a_done(false);
        fl::atomic<bool> b_done(false);
        fl::atomic<bool> c_done(false);

        // Coroutine C: end of chain, awaits p2
        fl::task::CoroutineConfig cfg_c;
        cfg_c.func = [p2, &final_value, &chain_complete, &c_done]() {
            auto result = fl::task::await(*p2);
            if (result.ok()) {
                final_value.store(result.value());
            }
            c_done.store(true);
            chain_complete.store(true);
        };
        cfg_c.name = "chain_c";
        auto tc = fl::task::coroutine(cfg_c);

        // Coroutine B: middle of chain, awaits p1, transforms, fulfills p2
        fl::task::CoroutineConfig cfg_b;
        cfg_b.func = [p1, p2, &b_done]() {
            auto result = fl::task::await(*p1);
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
        fl::task::CoroutineConfig cfg_a;
        cfg_a.func = [p1, &a_done]() {
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

void AutoResearchRemoteControl::tick(uint32_t current_millis) {
    if (mRemote) {
        // Remote::update() does pull + tick + push
        mRemote->update(current_millis);
    }
    if (mBleRemote) {
        mBleRemote->update(current_millis);
    }
    // Deferred BLE teardown: stopBle RPC sets this flag so the response
    // is sent (via push() above) before we call ble::destroyTransport().
    if (mPendingBleStop) {
        mPendingBleStop = false;
        mBleRemote.reset();  // destroy lambdas before freeing state they capture
        fl::net::ble::destroyTransport(mBleState);
        mBleState = nullptr;
        mState->ble_server_active = false;
        getBleState().ble_server_active = false;
        FL_WARN("[BLE] Deferred teardown complete");
    }
}

void AutoResearchRemoteControl::registerAllMethods(fl::Remote* remote) {
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

fl::json AutoResearchRemoteControl::startBleRemote() {
    if (mBleRemote) {
        fl::json response = fl::json::object();
        response.set("success", true);
        response.set("message", "BLE remote already active");
        response.set("device_name", AUTORESEARCH_BLE_DEVICE_NAME);
        return response;
    }

    // Create BLE GATT server (heap-allocates transport state)
    // On stub platforms, createTransport returns nullptr and logs FL_ERROR.
    mBleState = fl::net::ble::createTransport(AUTORESEARCH_BLE_DEVICE_NAME);
    if (!mBleState) {
        fl::json response = fl::json::object();
        response.set("success", false);
        response.set("error", "BLE not available on this platform");
        return response;
    }

    // Get transport lambdas that capture mBleState
    auto callbacks = fl::net::ble::getTransportCallbacks(mBleState);

    // Create BLE Remote instance with BLE transport
    mBleRemote = fl::make_unique<fl::Remote>(callbacks.first, callbacks.second);

    // Register RPC methods on the BLE remote
    registerAllMethods(mBleRemote.get());

    mState->ble_server_active = true;
    getBleState().ble_server_active = true;

    fl::json response = fl::json::object();
    response.set("success", true);
    response.set("device_name", AUTORESEARCH_BLE_DEVICE_NAME);
    response.set("service_uuid", FL_BLE_SERVICE_UUID);
    response.set("rx_uuid", FL_BLE_CHAR_RX_UUID);
    response.set("tx_uuid", FL_BLE_CHAR_TX_UUID);
    FL_WARN("[BLE] Remote created and advertising");
    return response;
}

fl::json AutoResearchRemoteControl::stopBleRemote() {
    // Defer actual BLE teardown to tick() so the RPC response is sent first.
    // BLEDevice::deinit(true) blocks long enough to prevent the response
    // from being transmitted over serial before the device resets BLE state.
    mPendingBleStop = true;
    fl::json response = fl::json::object();
    response.set("success", true);
    return response;
}
