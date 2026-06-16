// Benchmark RPC bindings: testSimd, testSimdBenchmark, animartrixPerlinBench, wave8ExpandBenchmark, parlioStreamValidate, parlioEncodeBenchmark.
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
#include "AutoResearchParlioStream.h"
#include "fl/chipsets/spi.h"
#include "fl/channels/config.h"
#include <Arduino.h>

#include "fl/net/ble.h"

#include "fl/codec/h264.h"
#include "fl/codec/mp4_parser.h"
#include "fl/stl/detail/memory_file_handle.h"
#include "fl/fx/frame.h"


void AutoResearchRemoteControl::bindBenchmarkMethods(fl::Remote* remote) {
    // Register "testSimd" function - run comprehensive SIMD test suite
    remote->bind("testSimd", [](const fl::json& args) -> fl::json {
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
    remote->bind("testSimdBenchmark", [](const fl::json& args) -> fl::json {
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
    remote->bind("animartrixPerlinBench", [](const fl::json& args) -> fl::json {
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
    remote->bind("wave8ExpandBenchmark", [](const fl::json& args) -> fl::json {
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
    remote->bind("parlioStreamValidate", [this](const fl::json& args) -> fl::json {
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
    remote->bind("parlioEncodeBenchmark", [](const fl::json& args) -> fl::json {
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
}

#endif // !(FASTLED_AUTORESEARCH_LOW_MEMORY)
