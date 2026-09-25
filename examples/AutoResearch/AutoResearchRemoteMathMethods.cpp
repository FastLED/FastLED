// Math / compute kernel RPC bindings: SIMD correctness + speed, Animartrix
// Perlin generator bench, wave8 expansion bench, PARLIO encoder/streaming
// validation. These exercise math/compute paths (not the perf probes which
// measure the probe machinery itself — those live in bindBenchmarkMethods).
// Extracted from AutoResearchRemote.cpp as part of #3132 / meta #3127.

#include "fl/system/sketch_macros.h"
#if FL_PLATFORM_HAS_LARGE_MEMORY

// Legacy debug macros (no-ops, kept for debugTest RPC function)
#define DEBUG_PRINT(x) do {} while(0)
#define DEBUG_PRINTLN(x) do {} while(0)

#include "AutoResearchRemote.h"
#include "AutoResearchBle.h"
#include "AutoResearchNet.h"
#include "AutoResearchOta.h"
#include "fl/remote/transport/serial.h"
#include "fl/system/heap.h"
#include "fl/audio/fft/fft.h"
#include "fl/audio/fft/fft_impl.h"
#include "fl/math/math.h"
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
#include "AutoResearchFlexIo.h"
#include "AutoResearchIeee754.h"
#include "AutoResearchMathExp.h"
#include "AutoResearchMp3.h"
#include "fl/chipsets/spi.h"
#include "fl/channels/config.h"
#include <Arduino.h>

#include "fl/net/ble.h"

#include "fl/codec/h264.h"
#include "fl/codec/mp4_parser.h"
#include "fl/stl/detail/memory_file_handle.h"
#include "fl/fx/frame.h"


void AutoResearchRemoteControl::bindMathMethods(fl::Remote& remote) {
    // Register "testSimd" function - run comprehensive SIMD test suite
    remote.bind("testSimd", [](const fl::json& args) -> fl::json {
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
    remote.bind("testSimdBenchmark", [](const fl::json& args) -> fl::json {
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
    remote.bind("animartrixPerlinBench", [](const fl::json& args) -> fl::json {
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
    remote.bind("wave8ExpandBenchmark", [](const fl::json& args) -> fl::json {
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

    // Register "ieee754CodecTest" - on-device integer IEEE 754 decimal codec
    // verification for #3039. No strtof/libm reference is used by the test.
    remote.bind("ieee754CodecTest", [](const fl::json& args) -> fl::json {
        (void)args;
        const auto r = autoresearch::ieee754_check::run();
        fl::json response = fl::json::object();
        response.set("success", r.success);
        response.set("tests_run", static_cast<int64_t>(r.tests_run));
        response.set("tests_failed", static_cast<int64_t>(r.tests_failed));
        response.set("first_failure", r.first_failure ? r.first_failure : "");
        response.set("expected_bits", static_cast<int64_t>(r.expected_bits));
        response.set("actual_bits", static_cast<int64_t>(r.actual_bits));
        return response;
    });

    // Register "mathExpBenchmark" - fl::exp accuracy (ulp vs libm) and speed
    // (fl::expf/exp vs the toolchain's expf/exp) on this core. #4288.
    remote.bind("mathExpBenchmark", [](const fl::json& args) -> fl::json {
        fl::u32 iters = 20000;
        const fl::json config = args.is_array() && args.size() > 0 ? args[0] : args;
        if (!config.is_null() && config.contains("iterations") && config["iterations"].is_int()) {
            iters = static_cast<fl::u32>(config["iterations"].as_int().value());
        }
        const auto r = autoresearch::math_exp::run(iters);
        fl::json response = fl::json::object();
        response.set("success", r.success);
        response.set("iterations", static_cast<int64_t>(r.iterations));
        response.set("accuracy_points", static_cast<int64_t>(r.accuracy_points));
        response.set("worst_ulp_float", static_cast<double>(r.worst_ulp_float));
        response.set("worst_x_float", static_cast<double>(r.worst_x_float));
        response.set("worst_ulp_double", r.worst_ulp_double);
        response.set("worst_x_double", r.worst_x_double);
        response.set("fl_expf_us", static_cast<int64_t>(r.fl_expf_us));
        response.set("libm_expf_us", static_cast<int64_t>(r.libm_expf_us));
        response.set("fl_exp_us", static_cast<int64_t>(r.fl_exp_us));
        response.set("libm_exp_us", static_cast<int64_t>(r.libm_exp_us));
        response.set("large_memory", static_cast<int64_t>(r.large_memory));
        response.set("sink", static_cast<double>(r.sink));
        return response;
    });

    // Register "fftCqOctaveBench" - CQ_OCTAVE per-frame cost on device
    // (#4540). Times the first frame (cold scratch) and the average warm
    // frame, and reports free heap before construction, after the first
    // frame, and after all warm frames, so per-frame heap growth shows up as
    // a difference between the last two. The input is a fixed two-tone
    // signal so results are comparable across builds.
    remote.bind("fftCqOctaveBench", [](const fl::json& args) -> fl::json {
        fl::i32 samples = 512;
        fl::i32 bands = 16;
        fl::i32 frames = 50;
        const fl::json config = args.is_array() && args.size() > 0 ? args[0] : args;
        if (!config.is_null() && config.is_object()) {
            if (config.contains("samples") && config["samples"].is_int())
                samples = static_cast<fl::i32>(config["samples"].as_int().value());
            if (config.contains("bands") && config["bands"].is_int())
                bands = static_cast<fl::i32>(config["bands"].as_int().value());
            if (config.contains("frames") && config["frames"].is_int())
                frames = static_cast<fl::i32>(config["frames"].as_int().value());
        }
        fl::json response = fl::json::object();
        response.set("samples", static_cast<int64_t>(samples));
        response.set("bands", static_cast<int64_t>(bands));
        response.set("frames", static_cast<int64_t>(frames));
        if (samples < 64 || samples > 4096 || bands < 2 || bands > 512 ||
            frames < 1 || frames > 10000) {
            response.set("success", false);
            response.set("error", "out_of_range");
            return response;
        }
        const fl::i32 kSampleRate = 44100;
        fl::vector<fl::i16> signal(static_cast<fl::size>(samples));
        for (fl::i32 i = 0; i < samples; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(kSampleRate);
            const float v = 0.5f * fl::sinf(2.0f * static_cast<float>(FL_M_PI) * 220.0f * t) +
                            0.3f * fl::sinf(2.0f * static_cast<float>(FL_M_PI) * 1500.0f * t);
            signal[static_cast<fl::size>(i)] = static_cast<fl::i16>(v * 20000.0f);
        }
        const fl::u32 heapBefore = static_cast<fl::u32>(fl::getFreeHeap().free_sram);
        fl::audio::fft::Args fftArgs(samples, bands, 174.6f, 4698.3f, kSampleRate,
                                     fl::audio::fft::Mode::CQ_OCTAVE);
        fl::audio::fft::Impl fft(fftArgs);
        fl::audio::fft::Bins out(static_cast<fl::size>(bands));
        const fl::u32 t0 = fl::micros();
        fft.run(signal, &out);
        const fl::u32 firstUs = fl::micros() - t0;
        const fl::u32 heapAfterFirst = static_cast<fl::u32>(fl::getFreeHeap().free_sram);
        const fl::u32 t1 = fl::micros();
        for (fl::i32 f = 0; f < frames; ++f) {
            fft.run(signal, &out);
        }
        const fl::u32 warmTotalUs = fl::micros() - t1;
        const fl::u32 heapAfterWarm = static_cast<fl::u32>(fl::getFreeHeap().free_sram);
        float checksum = 0.0f;
        for (float b : out.raw()) checksum += b;
        response.set("success", true);
        response.set("first_frame_us", static_cast<int64_t>(firstUs));
        response.set("warm_frame_us", static_cast<double>(warmTotalUs) / frames);
        response.set("heap_before", static_cast<int64_t>(heapBefore));
        response.set("heap_after_first", static_cast<int64_t>(heapAfterFirst));
        response.set("heap_after_warm", static_cast<int64_t>(heapAfterWarm));
        response.set("checksum", static_cast<double>(checksum));
        return response;
    });

    // Register "mp3CodecTest" - on-device verification of the fixed-point MP3
    // decoder. FastLED ships minimp3 in fixed-point mode on every embedded
    // target, and that pipeline is where 32-bit-specific defects live: host
    // testing on x86-64 cannot see an int64 add that costs two instructions
    // plus carry here, signed overflow the optimiser assumed away, or a
    // different alignment of the scratch arena. The check is therefore
    // bit-exact -- an FNV-1a over the PCM against the value the same
    // mp3dec_decode_frame_r produced on the host -- because the fixed-point
    // decoder is deterministic and has no tolerance to spend.
    remote.bind("mp3CodecTest", [](const fl::json& args) -> fl::json {
        (void)args;
        const auto r = autoresearch::mp3_check::run();
        fl::json response = fl::json::object();
        response.set("success", r.success);
        response.set("streams_run", static_cast<int64_t>(r.streams_run));
        response.set("streams_failed", static_cast<int64_t>(r.streams_failed));
        response.set("first_failure", r.first_failure ? r.first_failure : "");
        response.set("expected_fnv1a", static_cast<int64_t>(r.expected_fnv1a));
        response.set("actual_fnv1a", static_cast<int64_t>(r.actual_fnv1a));
        response.set("expected_samples",
                     static_cast<int64_t>(r.expected_samples));
        response.set("actual_samples", static_cast<int64_t>(r.actual_samples));
        response.set("decode_micros", static_cast<int64_t>(r.decode_micros));
        response.set("scratch_bytes", static_cast<int64_t>(r.scratch_bytes));
        response.set("shared_decoder_matched", r.shared_decoder_matched);
        response.set("samples_decoded",
                     static_cast<int64_t>(r.samples_decoded));
        response.set("frames_decoded",
                     static_cast<int64_t>(r.frames_decoded));
        response.set("combined_fnv1a",
                     static_cast<int64_t>(r.combined_fnv1a));
        response.set("verify_micros",
                     static_cast<int64_t>(r.verify_micros));
        response.set("audio_micros",
                     static_cast<int64_t>(r.audio_micros));
        return response;
    });

    // Register "parlioStreamValidate" - functional test of the production
    // PARLIO ISR-chunked streaming engine (#2548). Exercises the 16-lane Wave8
    // path which dispatches to wave8Transpose_16x4_bf1_pipe4 (BF1, #2559).
    // Drives N back-to-back FastLED.show() cycles and verifies each completes
    // within timeout. Returns per-iter timing so the host can diagnose stalls.
    remote.bind("parlioStreamValidate", [this](const fl::json& args) -> fl::json {
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
    remote.bind("parlioEncodeBenchmark", [](const fl::json& args) -> fl::json {
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

    // Register "flexIoDeviceInfo" — introspect which concrete driver
    // is bound to the `Bus::FLEX_IO` slot at runtime and return a
    // device-info JSON structure the host can inspect. Peripheral-
    // agnostic per the "Runtime driver selection is Bus::FLEX_IO"
    // agent-doc rule (FastLED#3515 Phase B). Optional args:
    // `{ "instance": 0 | 1 }` — currently only affects the
    // `instance` echo in the response since the manager's public
    // API doesn't yet expose per-(bus,instance) lookup. When
    // IChannelDriver grows getBus/getInstance() methods, this handler
    // will resolve to the specific driver on that slot.
    remote.bind("flexIoDeviceInfo", [](const fl::json& args) -> fl::json {
        fl::u8 instance = 0;
        fl::json config;
        if (args.is_object()) {
            config = args;
        } else if (args.is_array() && args.size() >= 1 && args[0].is_object()) {
            config = args[0];
        }
        if (!config.is_null() && config.contains("instance") &&
            config["instance"].is_int()) {
            int64_t iv = config["instance"].as_int().value();
            if (iv >= 0 && iv <= 255) {
                instance = static_cast<fl::u8>(iv);
            }
        }
        fl::json response = autoresearch::flex_io::buildDeviceInfo(instance);
        response.set("success", true);
        return response;
    });
}

#endif  // FL_PLATFORM_HAS_LARGE_MEMORY
