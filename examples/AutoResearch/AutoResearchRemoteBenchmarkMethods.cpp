// Performance benchmark RPC bindings — the autoresearch perf framework
// itself: perfProbeMemcpy (SRAM bandwidth), perfProbeNop (probe overhead
// floor), perfProbeRepeat (probe variance), and wave2dPerf (Wave2D solver
// benchmark). Sanity probes gate trust in subsequent perf numbers.
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
#include "fl/stl/vector.h"
#include "fl/stl/singleton.h"
#include "fl/system/heap.h"
#include "fl/system/delay.h"
#include "fl/system/pin.h"
#include "fl/system/pins.h"
#include "platforms/cpu_frequency.h"
#include "Common.h"
#include "AutoResearchTest.h"
#include "AutoResearchHelpers.h"
#include "fl/stl/compiler_control.h"
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

namespace {

struct PerfProbeMemcpyState {
    fl::vector<uint8_t> source;
    fl::vector<uint8_t> destination;
};

PerfProbeMemcpyState& perfProbeMemcpyState() {
    return fl::Singleton<PerfProbeMemcpyState>::instance();
}

}  // namespace

void AutoResearchRemoteControl::bindBenchmarkMethods(fl::Remote& remote) {
    // ====== AutoResearch perf-instrumentation sanity probes ======
    // Task 2 of meta #3113. Three probes verify autoresearch timing
    // produces trustworthy data BEFORE we use it to gate optimizations.
    // Fail-loud: any probe failing → host reporter MUST stamp result
    // UNTRUSTED and refuse to feed wave2dPerf into comparison tables.

    // Probe 2a: memcpy throughput. Caller invokes with { bytes,
    // iterations }; returns MB/s on SRAM. Host compares vs per-platform
    // vendor floor (e.g. ESP32-S3 ~175 MB/s) before trusting wave2dPerf.
    remote.bind("perfProbeMemcpy", [](const fl::json& args) -> fl::json {
        int bytes = 4096;
        int iterations = 1000;
        if (args.is_array() && args.size() >= 1 && args[0].is_object()) {
            const fl::json &cfg = args[0];
            if (cfg.contains("bytes") && cfg["bytes"].is_int()) {
                bytes = static_cast<int>(cfg["bytes"].as_int().value());
            }
            if (cfg.contains("iterations") && cfg["iterations"].is_int()) {
                iterations = static_cast<int>(cfg["iterations"].as_int().value());
            }
        }
        fl::json response = fl::json::object();
        response.set("bytes", static_cast<int64_t>(bytes));
        response.set("iterations", static_cast<int64_t>(iterations));
        if (bytes < 64 || bytes > 8192 || iterations < 1 || iterations > 100000) {
            response.set("success", false);
            response.set("error", "out_of_range");
            return response;
        }
        // Heap-allocated on first use: 16 KB of static buffers
        // overflowed ESP32-S2's dram0 bss by 3.7 KB (FastLED#3576
        // Phase 4 compile matrix).
        PerfProbeMemcpyState& state = perfProbeMemcpyState();
        fl::vector<uint8_t>& src_vec = state.source;
        fl::vector<uint8_t>& dst_vec = state.destination;
        if (src_vec.size() < 8192) {
            src_vec.resize(8192);
            dst_vec.resize(8192);
        }
        uint8_t *src_buf = src_vec.data();
        uint8_t *dst_buf = dst_vec.data();
        for (int i = 0; i < bytes; ++i) {
            src_buf[i] = static_cast<uint8_t>(i & 0xFF);
        }
        const fl::u32 t0 = fl::micros();
        for (int i = 0; i < iterations; ++i) {
            FL_BUILTIN_MEMCPY(dst_buf, src_buf, bytes);
        }
        const fl::u32 t1 = fl::micros();
        const fl::u32 total_us = t1 - t0;
        const double total_bytes = static_cast<double>(bytes) * iterations;
        const double mb_per_s = (total_us > 0)
            ? (total_bytes / total_us) * (1e6 / (1024.0 * 1024.0))
            : 0.0;
        response.set("success", true);
        response.set("total_us", static_cast<int64_t>(total_us));
        response.set("mb_per_s", mb_per_s);
        return response;
    });

    // Probe 2b: nop-loop calibration. Volatile fence prevents the
    // compiler optimizing the loop away. Host computes
    // (total_us * cpu_mhz / iterations) and checks [1.0, 2.5]
    // cycles-per-nop. Out-of-band → timer broken or IRQ jitter.
    remote.bind("perfProbeNop", [](const fl::json& args) -> fl::json {
        int iterations = 100000;
        if (args.is_array() && args.size() >= 1 && args[0].is_object()) {
            const fl::json &cfg = args[0];
            if (cfg.contains("iterations") && cfg["iterations"].is_int()) {
                iterations = static_cast<int>(cfg["iterations"].as_int().value());
            }
        }
        fl::json response = fl::json::object();
        response.set("iterations", static_cast<int64_t>(iterations));
        if (iterations < 1000 || iterations > 10000000) {
            response.set("success", false);
            response.set("error", "out_of_range");
            return response;
        }
        volatile int counter = 0;
        const fl::u32 t0 = fl::micros();
        for (int i = 0; i < iterations; ++i) {
#if defined(__GNUC__) || defined(__clang__)
            __asm__ volatile("nop");
#else
            counter += 1;
#endif
        }
        const fl::u32 t1 = fl::micros();
        (void)counter;
        const fl::u32 total_us = t1 - t0;
        const double us_per_iter = (iterations > 0)
            ? (static_cast<double>(total_us) / iterations) : 0.0;
        response.set("success", true);
        response.set("total_us", static_cast<int64_t>(total_us));
        response.set("us_per_iter", us_per_iter);
        return response;
    });

    // Probe 2d: bit-bang cost attribution (FastLED#4203). The portable
    // BIT_BANG driver stretches every WS2812 bit by a constant ~2-3us, which
    // makes its output undecodable. The captures prove the cost is constant
    // per bit but cannot say which call carries it, so measure the candidates
    // against a common loop baseline instead of guessing:
    //   nop        - empty-loop floor for this board
    //   delayNs    - fl::delayNanoseconds(ns): runtime u64 divide, and on
    //                ESP32 an esp_clk_cpu_freq() query on every call
    //   delayNsHz  - same with the clock frequency hoisted out; the gap
    //                against delayNs is exactly the per-call clock query
    //   writeByte  - DigitalMultiWrite8::writeByte(): nibble LUT lookups plus
    //                two out-of-line applyNibble() calls
    // A bit costs 3x delay + 3x writeByte, so these numbers reconstruct the
    // measured per-bit overhead directly.
    remote.bind("perfProbeBitBangCost", [](const fl::json& args) -> fl::json {
        int iterations = 20000;
        int ns = 400;          // WS2812 T0H — the budget being blown
        int pin = -1;          // -1 keeps GPIO untouched (default, safe)
        if (args.is_array() && args.size() >= 1 && args[0].is_object()) {
            const fl::json &cfg = args[0];
            if (cfg.contains("iterations") && cfg["iterations"].is_int()) {
                iterations = static_cast<int>(cfg["iterations"].as_int().value());
            }
            if (cfg.contains("ns") && cfg["ns"].is_int()) {
                ns = static_cast<int>(cfg["ns"].as_int().value());
            }
            if (cfg.contains("pin") && cfg["pin"].is_int()) {
                pin = static_cast<int>(cfg["pin"].as_int().value());
            }
        }
        fl::json response = fl::json::object();
        response.set("iterations", static_cast<int64_t>(iterations));
        response.set("ns", static_cast<int64_t>(ns));
        response.set("pin", static_cast<int64_t>(pin));
        if (iterations < 1000 || iterations > 1000000 || ns < 0) {
            response.set("success", false);
            response.set("error", "out_of_range");
            return response;
        }

        const fl::u32 requested_ns = static_cast<fl::u32>(ns);
        const fl::u32 hz = static_cast<fl::u32>(FL_CPU_FREQUENCY());

        // Baseline: same loop shape, no payload.
        volatile int sink = 0;
        const fl::u32 nop_t0 = fl::micros();
        for (int i = 0; i < iterations; ++i) {
            sink += 1;
        }
        const fl::u32 nop_us = fl::micros() - nop_t0;

        const fl::u32 delay_t0 = fl::micros();
        for (int i = 0; i < iterations; ++i) {
            fl::delayNanoseconds(requested_ns);
        }
        const fl::u32 delay_us = fl::micros() - delay_t0;

        const fl::u32 delay_hz_t0 = fl::micros();
        for (int i = 0; i < iterations; ++i) {
            fl::delayNanoseconds(requested_ns, hz);
        }
        const fl::u32 delay_hz_us = fl::micros() - delay_hz_t0;

        // pin < 0 leaves every slot inactive, so applyNibble performs no GPIO
        // write at all — that isolates the LUT and call overhead from the
        // cost of the pin write itself.
        fl::Pins8 pins8;
        for (int i = 0; i < 8; ++i) {
            pins8.pins[i] = -1;
        }
        if (pin >= 0) {
            fl::pinMode(pin, fl::PinMode::Output);
            pins8.pins[0] = pin;
        }
        fl::DigitalMultiWrite8 writer;
        writer.init(pins8);
        const fl::u32 wb_t0 = fl::micros();
        for (int i = 0; i < iterations; ++i) {
            writer.writeByte(static_cast<fl::u8>(i & 0xFF));
        }
        const fl::u32 wb_us = fl::micros() - wb_t0;
        if (pin >= 0) {
            fl::digitalWrite(pin, fl::PinValue::Low);
        }
        (void)sink;

        const double denom = static_cast<double>(iterations);
        const double nop_per = static_cast<double>(nop_us) / denom;
        const double delay_per = static_cast<double>(delay_us) / denom;
        const double delay_hz_per = static_cast<double>(delay_hz_us) / denom;
        const double wb_per = static_cast<double>(wb_us) / denom;

        response.set("success", true);
        response.set("cpu_hz", static_cast<int64_t>(hz));
        response.set("nop_us_per_iter", nop_per);
        response.set("delay_us_per_iter", delay_per);
        response.set("delay_hz_us_per_iter", delay_hz_per);
        response.set("write_byte_us_per_iter", wb_per);
        // Cost above the loop floor, i.e. what the caller actually pays.
        response.set("delay_overhead_us", delay_per - nop_per);
        response.set("delay_hz_overhead_us", delay_hz_per - nop_per);
        response.set("write_byte_overhead_us", wb_per - nop_per);
        response.set("clock_query_us", delay_per - delay_hz_per);
        // One clockless bit issues three delays and three writeByte calls.
        response.set("predicted_bit_overhead_us",
                     3.0 * ((delay_per - nop_per) + (wb_per - nop_per)));
        return response;
    });

    // Probe 2c: repeat-stability check. Runs wave2dPerf's measurement
    // N times at a fixed config, reports mean + std-dev. Host checks
    // std_dev/mean < 5%; higher → IRQ noise or RPC framing jitter is
    // contaminating wave2dPerf, mark UNTRUSTED.
    remote.bind("perfProbeRepeat", [](const fl::json& args) -> fl::json {
        int W = 16;
        int H = 16;
        int iterations = 50;
        int repeats = 8;
        fl::string stencil_name = "FivePoint";
        if (args.is_array() && args.size() >= 1 && args[0].is_object()) {
            const fl::json &cfg = args[0];
            if (cfg.contains("W") && cfg["W"].is_int()) {
                W = static_cast<int>(cfg["W"].as_int().value());
            }
            if (cfg.contains("H") && cfg["H"].is_int()) {
                H = static_cast<int>(cfg["H"].as_int().value());
            }
            if (cfg.contains("iterations") && cfg["iterations"].is_int()) {
                iterations = static_cast<int>(cfg["iterations"].as_int().value());
            }
            if (cfg.contains("repeats") && cfg["repeats"].is_int()) {
                repeats = static_cast<int>(cfg["repeats"].as_int().value());
            }
            if (cfg.contains("stencil") && cfg["stencil"].is_string()) {
                stencil_name = cfg["stencil"].as_string().value();
            }
        }
        fl::json response = fl::json::object();
        response.set("W", static_cast<int64_t>(W));
        response.set("H", static_cast<int64_t>(H));
        response.set("iterations", static_cast<int64_t>(iterations));
        response.set("repeats", static_cast<int64_t>(repeats));
        if (W < 4 || H < 4 || W > 128 || H > 128 ||
            iterations < 1 || iterations > 1000 ||
            repeats < 2 || repeats > 64) {
            response.set("success", false);
            response.set("error", "out_of_range");
            return response;
        }
        const fl::LaplacianStencil stencil =
            (stencil_name == "NinePointIsotropic")
            ? fl::LaplacianStencil::NinePointIsotropic
            : fl::LaplacianStencil::FivePoint;
        // External-binder pattern: simulator-side code (in
        // src/fl/math/wave/wave_perf_bench.h) owns the wave PDE setup,
        // checkerboard seed, warm-up, timing and statistics. This RPC
        // is the thin JSON-RPC translation layer.
        const fl::wave_perf::WavePerfRepeatResult bench =
            fl::wave_perf::runWavePerfRepeat(
                static_cast<fl::u32>(W),
                static_cast<fl::u32>(H),
                static_cast<fl::u32>(iterations),
                static_cast<fl::u32>(repeats),
                stencil);
        response.set("success", bench.success);
        response.set("stencil", stencil_name.c_str());
        response.set("mean_us_per_update", bench.mean_us_per_update);
        response.set("std_dev_us_per_update", bench.std_dev_us_per_update);
        response.set("std_dev_pct", bench.std_dev_pct);
        return response;
    });

    // Wave2D perf benchmark — first task from meta #3113.
    //
    // Args: { W, H, iterations, stencil ("FivePoint" | "NinePointIsotropic"),
    //         loads_only (bool, optional) }
    //
    // Builds a WaveSimulation2D_Real, seeds it with a deterministic
    // checkerboard, optionally warms up, then times `iterations` calls to
    // update() using fl::micros(). Returns timing dict including us/update
    // and us/cell/update so we can compare scaling across platforms.
    //
    // `loads_only=true` swaps update() for a memory-bound baseline (just
    // sum every cell into a sink) so the gap between the two reveals
    // compute vs memory cost — critical for the ESP32-S3 PSRAM-vs-SRAM
    // analysis in sibling issue #3114.
    remote.bind("wave2dPerf", [](const fl::json& args) -> fl::json {
        // Defaults sized so the test runs in a sensible budget on small
        // platforms (32x32 / 100 iters ~= 100-500 ms on M4-class).
        int W = 32;
        int H = 32;
        int iterations = 100;
        fl::string stencil_name = "NinePointIsotropic";
        bool loads_only = false;
        if (args.is_array() && args.size() >= 1 && args[0].is_object()) {
            const fl::json &cfg = args[0];
            if (cfg.contains("W") && cfg["W"].is_int()) {
                W = static_cast<int>(cfg["W"].as_int().value());
            }
            if (cfg.contains("H") && cfg["H"].is_int()) {
                H = static_cast<int>(cfg["H"].as_int().value());
            }
            if (cfg.contains("iterations") && cfg["iterations"].is_int()) {
                iterations = static_cast<int>(cfg["iterations"].as_int().value());
            }
            if (cfg.contains("stencil") && cfg["stencil"].is_string()) {
                stencil_name = cfg["stencil"].as_string().value();
            }
            if (cfg.contains("loads_only") && cfg["loads_only"].is_bool()) {
                loads_only = cfg["loads_only"].as_bool().value();
            }
        }

        fl::json response = fl::json::object();
        response.set("W", static_cast<int64_t>(W));
        response.set("H", static_cast<int64_t>(H));
        response.set("iterations", static_cast<int64_t>(iterations));
        response.set("stencil", stencil_name.c_str());
        response.set("loads_only", loads_only);

        // Refuse silly inputs. 1024 cap protects us from accidentally
        // requesting a multi-MB grid via a bad RPC payload.
        if (W < 4 || H < 4 || W > 1024 || H > 1024 || iterations < 1 || iterations > 100000) {
            response.set("success", false);
            response.set("error", "out_of_range");
            return response;
        }

        const fl::LaplacianStencil stencil =
            (stencil_name == "FivePoint")
            ? fl::LaplacianStencil::FivePoint
            : fl::LaplacianStencil::NinePointIsotropic;

        // External-binder pattern: simulator-side code (in
        // src/fl/math/wave/wave_perf_bench.h) owns the wave PDE setup,
        // checkerboard seed, warm-up, and timing. This RPC is the thin
        // JSON-RPC translation layer.
        const fl::wave_perf::WavePerfResult bench = loads_only
            ? fl::wave_perf::runWavePerfLoadsOnly(
                  static_cast<fl::u32>(W), static_cast<fl::u32>(H),
                  static_cast<fl::u32>(iterations), stencil)
            : fl::wave_perf::runWavePerf(
                  static_cast<fl::u32>(W), static_cast<fl::u32>(H),
                  static_cast<fl::u32>(iterations), stencil);

        response.set("success", bench.success);
        response.set("total_us", static_cast<int64_t>(bench.total_us));
        response.set("us_per_update", bench.us_per_update);
        response.set("us_per_cell_per_update", bench.us_per_cell_per_update);
        response.set("fps_at_one_update_per_frame",
                     bench.fps_at_one_update_per_frame);
        return response;
    });
}

#endif // !(FASTLED_AUTORESEARCH_LOW_MEMORY)
