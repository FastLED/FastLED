// Performance benchmark RPC bindings — the autoresearch perf framework
// itself: perfProbeMemcpy (SRAM bandwidth), perfProbeNop (probe overhead
// floor), perfProbeRepeat (probe variance), and wave2dPerf (Wave2D solver
// benchmark). Sanity probes gate trust in subsequent perf numbers.
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
#include "fl/channels/color_managed_source.h"  // colorPipelinePerf (P9, #4043)
#include "fl/gfx/colorimetric_response.h"
#include "fl/gfx/pipeline.h"
#include "fl/channels/config.h"
#include "fl/channels/channel.h"
#include "fl/channels/driver.h"
#include "fl/channels/manager.h"
#include "fl/channels/data.h"
#include "fl/channels/power_prepass.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/stl/scope_exit.h"
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

#if FL_COLOR_PIPELINE_SHARED
class PowerBenchCaptureDriver : public fl::IChannelDriver {
  public:
    fl::ChannelDataPtr frames[3];
    int count = 0;
    bool canHandle(const fl::ChannelDataPtr&) const FL_NO_EXCEPT override { return true; }
    void enqueue(fl::ChannelDataPtr data) FL_NO_EXCEPT override {
        if (count < 3) frames[count++] = data;
    }
    void show() FL_NO_EXCEPT override {}
    DriverState poll() FL_NO_EXCEPT override { return DriverState::READY; }
    fl::string getName() const FL_NO_EXCEPT override { return "POWER_BENCH_CAPTURE"; }
    Capabilities getCapabilities() const FL_NO_EXCEPT override {
        return Capabilities(true, true);
    }
};

fl::colorimetric_response::EmitterProfile powerBenchProfile(
    fl::colorimetric_response::EmitterTopology topology) {
    fl::colorimetric_response::EmitterProfile profile = fl::profiles::WS2812B;
    profile.topology = topology;
    profile.xy_white1[0] = 0.39f;
    profile.xy_white1[1] = 0.38f;
    profile.xy_white2[0] = 0.28f;
    profile.xy_white2[1] = 0.31f;
    profile.lum_white1 = 0.8f;
    profile.lum_white2 = 0.8f;
    return profile;
}
#endif

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
        // Six timed loops each run `iterations` times, so the wall time is
        // roughly 6 * iterations * ns. AutoResearch's loop watchdog is 5 s;
        // cap the estimate well under it so a large ns cannot turn this probe
        // into a watchdog reset on an unattended bench.
        constexpr fl::u64 kMaxProbeNs = 2000000000ULL;  // 2 s
        const fl::u64 estimated_ns =
            static_cast<fl::u64>(iterations) * static_cast<fl::u64>(ns) * 6ULL;
        if (estimated_ns > kMaxProbeNs) {
            response.set("success", false);
            response.set("error", "duration_budget_exceeded");
            response.set("estimated_ns", static_cast<int64_t>(estimated_ns));
            response.set("budget_ns", static_cast<int64_t>(kMaxProbeNs));
            return response;
        }

        const fl::u32 requested_ns = static_cast<fl::u32>(ns);
        const fl::u32 hz = static_cast<fl::u32>(FL_CPU_FREQUENCY());

        // Baseline: same loop shape, no payload. An empty memory barrier
        // keeps the loop alive without adding work of its own -- the previous
        // `volatile int` read-modify-write costs a load, add and store to
        // memory every iteration, which inflates nop_per and therefore
        // under-reports every overhead computed by subtracting it.
        const fl::u32 nop_t0 = fl::micros();
        for (int i = 0; i < iterations; ++i) {
            __asm__ __volatile__("" ::: "memory");
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

        // Compile-time NS: cycles_from_ns_*() folds to a constant, so the
        // runtime u64 divide disappears while the platform busy-wait still
        // runs. If this is fast, the divide carried the cost; if it is still
        // slow, the cost is inside the busy-wait. Fixed at 400ns (WS2812 T0H)
        // because a template argument cannot come from the RPC payload -- the
        // `ns` parameter only steers the runtime variants above.
        // Runtime form pinned to 400 ns. ns_conversion_us subtracts the
        // compile-time 400 ns loop, and the compile-time form cannot take its
        // duration from the RPC payload -- so comparing it against the
        // caller-supplied `ns` above would difference two different delays
        // and misattribute the gap to conversion cost.
        const fl::u32 delay_hz400_t0 = fl::micros();
        for (int i = 0; i < iterations; ++i) {
            fl::delayNanoseconds(400u, hz);
        }
        const fl::u32 delay_hz400_us = fl::micros() - delay_hz400_t0;

        const fl::u32 delay_ct_t0 = fl::micros();
        for (int i = 0; i < iterations; ++i) {
            fl::delayNanoseconds<400>();
        }
        const fl::u32 delay_ct_us = fl::micros() - delay_ct_t0;

        // Pure cycle-counted loop: no ns->cycles conversion and no platform
        // busy-wait wrapper. This is the floor a hand-rolled bit loop could
        // reach. The board reports cpu_hz=125000000, so 400ns is 50 cycles;
        // 60 is kept as a small deliberate overshoot. delaycycles<> is specialized
        // only up to 50 with no generic fallback, so 60 is composed from two
        // specializations rather than written as delaycycles<60>(), which is
        // an undefined reference at link time.
        const fl::u32 delay_cyc_t0 = fl::micros();
        for (int i = 0; i < iterations; ++i) {
            fl::delaycycles<50>();
            fl::delaycycles<10>();
        }
        const fl::u32 delay_cyc_us = fl::micros() - delay_cyc_t0;

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
        const double delay_ct_per = static_cast<double>(delay_ct_us) / denom;
        const double delay_cyc_per = static_cast<double>(delay_cyc_us) / denom;
        response.set("delay_ct_us_per_iter", delay_ct_per);
        response.set("delay_cycles_us_per_iter", delay_cyc_per);
        response.set("delay_ct_overhead_us", delay_ct_per - nop_per);
        response.set("delay_cycles_overhead_us", delay_cyc_per - nop_per);
        // Cost attributable to the runtime ns->cycles conversion: the gap
        // between the runtime and compile-time forms, both at 400 ns.
        const double delay_hz400_per =
            static_cast<double>(delay_hz400_us) / iterations;
        response.set("delay_hz400_us_per_iter", delay_hz400_per);
        response.set("ns_conversion_us", delay_hz400_per - delay_ct_per);
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

#if FL_COLOR_PIPELINE_SHARED
    // colorPipelinePerf below stops at the pixel source. This measures the
    // complete show -> power plan -> channel encode path on three channels.
    // A capture driver consumes encoded frames without requiring LED wiring;
    // physical transmission time is deliberately outside this measurement.
    remote.bind("mixedPowerShowPerf", [](const fl::json& args) -> fl::json {
        int pixels = 64;
        int frames = 4;
        int budget_mw = 2500;
        if (args.is_array() && args.size() && args[0].is_object()) {
            const fl::json& cfg = args[0];
            if (cfg.contains("pixels") && cfg["pixels"].is_int())
                pixels = static_cast<int>(cfg["pixels"].as_int().value());
            if (cfg.contains("frames") && cfg["frames"].is_int())
                frames = static_cast<int>(cfg["frames"].as_int().value());
            if (cfg.contains("budget_mw") && cfg["budget_mw"].is_int())
                budget_mw = static_cast<int>(cfg["budget_mw"].as_int().value());
        }
        fl::json response = fl::json::object();
        response.set("pixels_per_channel", pixels);
        response.set("frames_per_sample", frames);
        response.set("budget_mw", budget_mw);
        if (pixels < 1 || pixels > 128 || frames < 1 || frames > 8 ||
            budget_mw < 625 || budget_mw > 50000) {
            response.set("success", false);
            response.set("error", "out_of_range");
            return response;
        }
        if (FastLED.count() != 0) {
            response.set("success", false);
            response.set("error", "existing_channels");
            return response;
        }

        fl::vector<CRGB> leds(static_cast<fl::size>(pixels * 3));
        for (int i = 0; i < pixels * 3; ++i)
            leds[static_cast<fl::size>(i)] = CRGB(170 + i * 7, 220 + i * 3, 190 + i * 11);
        fl::ChannelOptions legacy_options;
        fl::ChannelOptions rgbw_options;
        fl::ChannelOptions rgbww_options;
        legacy_options.mDitherMode = DISABLE_DITHER;
        rgbw_options.mDitherMode = DISABLE_DITHER;
        rgbww_options.mDitherMode = DISABLE_DITHER;
        rgbw_options.mWhiteCfg = fl::Rgbw(6000, fl::RGBW_MODE::kRGBWNullWhitePixel,
                                           fl::EOrderW::W0);
        rgbww_options.mWhiteCfg = fl::Rgbww(2700, 6500,
            fl::RGBWW_MODE::kRGBWWColorimetric, fl::EOrderWW::WwWcStart);
        if (!rgbw_options.setColorProfile(powerBenchProfile(
                fl::colorimetric_response::EmitterTopology::RGBW),
                fl::SourceProfile::linearSrgb()) ||
            !rgbww_options.setColorProfile(powerBenchProfile(
                fl::colorimetric_response::EmitterTopology::RGBWW),
                fl::SourceProfile::linearSrgb())) {
            response.set("success", false);
            response.set("error", "profile_build_failed");
            return response;
        }
        const auto timing = fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>();
        auto legacy = fl::Channel::create(fl::ChannelConfig(101, timing,
            fl::span<CRGB>(leds.data(), pixels), RGB, legacy_options));
        auto rgbw = fl::Channel::create(fl::ChannelConfig(102, timing,
            fl::span<CRGB>(leds.data() + pixels, pixels), RGB, rgbw_options));
        auto rgbww = fl::Channel::create(fl::ChannelConfig(103, timing,
            fl::span<CRGB>(leds.data() + pixels * 2, pixels), RGB, rgbww_options));
        if (!legacy || !rgbw || !rgbww || !rgbw->isColorManaged() ||
            !rgbww->isColorManaged()) {
            response.set("success", false);
            response.set("error", "channel_create_failed");
            return response;
        }
        auto capture = fl::make_shared<PowerBenchCaptureDriver>();
        const fl::u8 prior_brightness = FastLED.getBrightness();
        fl::ChannelManager::instance().addDriver(1000000, capture);
        auto cleanup = fl::make_scope_exit([&]() {
            FastLED.remove(legacy);
            FastLED.remove(rgbw);
            FastLED.remove(rgbww);
            FastLED.clear(ClearFlags::POWER_SETTINGS);
            FastLED.setBrightness(prior_brightness);
            fl::ChannelManager::instance().removeDriver(capture);
        });
        FastLED.add(legacy);
        FastLED.add(rgbw);
        FastLED.add(rgbww);
        FastLED.setBrightness(255);
        const fl::FramePowerPlan on_plan =
            fl::calculateFramePowerPlan(255, static_cast<fl::u32>(budget_mw));
        const fl::FramePowerPlan off_plan =
            fl::calculateFramePowerPlan(255, 0xffffffffu);

        bool captured_all = true;
        auto show_sample = [&](bool limited, fl::u32& hash) -> fl::u32 {
            if (limited) FastLED.setMaxPowerInMilliWatts(budget_mw);
            else FastLED.clear(ClearFlags::POWER_SETTINGS);
            capture->count = 0;
            FastLED.show(); // untimed warm-up after each setting change
            const fl::u32 start = fl::micros();
            for (int f = 0; f < frames; ++f) {
                capture->count = 0;
                FastLED.show();
            }
            const fl::u32 elapsed = fl::micros() - start;
            captured_all = captured_all && capture->count == 3;
            if (capture->count == 3) {
                for (int c = 0; c < 3; ++c) {
                    const auto& bytes = capture->frames[c]->getData();
                    for (fl::size i = 0; i < bytes.size(); ++i)
                        hash = (hash ^ bytes[i]) * 16777619u;
                    capture->frames[c].reset();
                }
            }
            return elapsed;
        };
        fl::u32 on_hash = 2166136261u, off_hash = 2166136261u;
        fl::u32 on_us[4], off_us[4];
        // ABBA / BAAB: each condition occupies every position once.
        const bool order[8] = {true, false, false, true,
                               false, true, true, false};
        int on_count = 0, off_count = 0;
        for (int i = 0; i < 8; ++i) {
            if (order[i]) on_us[on_count++] = show_sample(true, on_hash);
            else off_us[off_count++] = show_sample(false, off_hash);
        }
        if (!captured_all) {
            response.set("success", false);
            response.set("error", "capture_failed");
            return response;
        }
        auto median4 = [](fl::u32* values) -> fl::u32 {
            for (int i = 1; i < 4; ++i)
                for (int j = i; j > 0 && values[j] < values[j - 1]; --j) {
                    const fl::u32 tmp = values[j]; values[j] = values[j - 1]; values[j - 1] = tmp;
                }
            return (values[1] + values[2]) / 2;
        };
        response.set("success", true);
        response.set("on_median_us", static_cast<int64_t>(median4(on_us)));
        response.set("off_median_us", static_cast<int64_t>(median4(off_us)));
        response.set("on_plan_modeled_mw", static_cast<int64_t>(on_plan.modeled_mW));
        response.set("off_plan_modeled_mw", static_cast<int64_t>(off_plan.modeled_mW));
        response.set("on_plan_flux_q16", static_cast<int64_t>(on_plan.flux_q16));
        response.set("off_plan_flux_q16", static_cast<int64_t>(off_plan.flux_q16));
        response.set("on_fnv1a", static_cast<int64_t>(on_hash));
        response.set("off_fnv1a", static_cast<int64_t>(off_hash));
        return response;
    });
#endif

#if FL_COLOR_PROFILE_RUNTIME
    // P9 (#4043): throughput of the colour pipeline on the real per-pixel
    // path. Times `ColorManagedPixelSource::loadAndScaleRGB` -- decode, gamut
    // map, device solve, flux and the final quantize, exactly what an encoder
    // pulls per pixel on a colour-managed channel -- against the legacy
    // `loadAndScale0/1/2` over the same buffer, so the ratio is the pipeline's
    // cost over what an unmanaged channel pays. Args: { pixels, frames,
    // dither, response_curve }. `dither` selects temporal dither (C5);
    // `response_curve` selects a synthetic three-sample response (#4497).
    remote.bind("colorPipelinePerf", [](const fl::json& args) -> fl::json {
        int pixels = 256;
        int frames = 20;
        bool dither = false;
        bool response_curve = false;
        if (args.is_array() && args.size() >= 1 && args[0].is_object()) {
            const fl::json& cfg = args[0];
            if (cfg.contains("pixels") && cfg["pixels"].is_int()) {
                pixels = static_cast<int>(cfg["pixels"].as_int().value());
            }
            if (cfg.contains("frames") && cfg["frames"].is_int()) {
                frames = static_cast<int>(cfg["frames"].as_int().value());
            }
            if (cfg.contains("dither") && cfg["dither"].is_bool()) {
                dither = cfg["dither"].as_bool().value();
            }
            if (cfg.contains("response_curve") && cfg["response_curve"].is_bool()) {
                response_curve = cfg["response_curve"].as_bool().value();
            }
        }
        fl::json response = fl::json::object();
        response.set("pixels", static_cast<int64_t>(pixels));
        response.set("frames", static_cast<int64_t>(frames));
        response.set("dither", dither);
        response.set("response_curve", response_curve);
        // Each path is measured in both complementary orders. Bound all eight
        // intervals so the synchronous RPC returns before the watchdog window.
        if (pixels < 1 || pixels > 4096 || frames < 1 || frames > 10000 ||
            static_cast<long>(pixels) * frames > 50000L) {
            response.set("success", false);
            response.set("error", "out_of_range");
            return response;
        }
        fl::StreamingPipelineQ16 pipeline;
        fl::colorimetric_response::EmitterProfile device = fl::profiles::WS2812B;
        const fl::u16 linear_response[] = {0, 32768, 65535};
        const fl::u16 nonlinear_response[] = {0, 16384, 65535};
        if (response_curve) {
            device.response_lut_r = linear_response;
            device.response_lut_g = nonlinear_response;
            device.response_lut_b = linear_response;
            device.response_lut_size = 3;
        }
        if (!fl::buildStreamingPipelineQ16(fl::SourceProfile::linearSrgb(),
                                           device,
                                           fl::GamutPolicy::ChromaCompress,
                                           &pipeline)) {
            response.set("success", false);
            response.set("error", "pipeline_build_failed");
            return response;
        }
        fl::vector<CRGB> buffer(static_cast<fl::size>(pixels));
        for (int i = 0; i < pixels; ++i) {
            buffer[static_cast<fl::size>(i)] = CRGB(
                static_cast<uint8_t>(i * 7), static_cast<uint8_t>(i * 13 + 40),
                static_cast<uint8_t>(255 - i * 3));
        }
        const EDitherMode mode = dither ? BINARY_DITHER : DISABLE_DITHER;
        ColorAdjustment adjustment = ColorAdjustment::noAdjustment();
        // One order-sensitive FNV-1a per path over every emitted byte, so a
        // checksum match between boards means the same byte sequence.
        uint32_t managed_hash = 2166136261u;
        uint32_t legacy_hash = 2166136261u;
        uint32_t managed_second_hash = 2166136261u;
        uint32_t legacy_second_hash = 2166136261u;
        uint32_t baab_managed_hash = 2166136261u;
        uint32_t baab_legacy_hash = 2166136261u;

        auto measure_managed = [&](uint32_t& hash) -> uint32_t {
            const uint32_t start = fl::micros();
            for (int f = 0; f < frames; ++f) {
                PixelController<RGB> controller(buffer.data(), pixels, adjustment, mode);
                fl::ColorManagedPixelSource source(controller, GRB, pipeline);
                while (source.has(1)) {
                    uint8_t b0, b1, b2;
                    source.loadAndScaleRGB(&b0, &b1, &b2);
                    hash = (hash ^ b0) * 16777619u;
                    hash = (hash ^ b1) * 16777619u;
                    hash = (hash ^ b2) * 16777619u;
                    source.advanceData();
                }
            }
            return fl::micros() - start;
        };

        auto measure_legacy = [&](uint32_t& hash) -> uint32_t {
            const uint32_t start = fl::micros();
            for (int f = 0; f < frames; ++f) {
                PixelController<GRB> controller(buffer.data(), pixels, adjustment, mode);
                while (controller.has(1)) {
                    hash = (hash ^ controller.loadAndScale0()) * 16777619u;
                    hash = (hash ^ controller.loadAndScale1()) * 16777619u;
                    hash = (hash ^ controller.loadAndScale2()) * 16777619u;
                    controller.stepDithering();
                    controller.advanceData();
                }
            }
            return fl::micros() - start;
        };

        // ABBA and BAAB place each implementation in every position across
        // the pair, reducing both linear drift and order-specific warm-up bias.
        const uint32_t abba_m1_us = measure_managed(managed_hash);
        const uint32_t abba_l2_us = measure_legacy(legacy_hash);
        const uint32_t abba_l3_us = measure_legacy(legacy_second_hash);
        const uint32_t abba_m4_us = measure_managed(managed_second_hash);
        const uint32_t baab_l1_us = measure_legacy(baab_legacy_hash);
        const uint32_t baab_m2_us = measure_managed(baab_managed_hash);
        const uint32_t baab_m3_us = measure_managed(baab_managed_hash);
        const uint32_t baab_l4_us = measure_legacy(baab_legacy_hash);
        const uint32_t managed_us = abba_m1_us + abba_m4_us + baab_m2_us + baab_m3_us;
        const uint32_t legacy_us = abba_l2_us + abba_l3_us + baab_l1_us + baab_l4_us;

        const double n = 4.0 * static_cast<double>(pixels) *
                         static_cast<double>(frames);
        const double managed_per = static_cast<double>(managed_us) / n;
        const double legacy_per = static_cast<double>(legacy_us) / n;
        response.set("success", true);
        response.set("managed_us", static_cast<int64_t>(managed_us));
        response.set("legacy_us", static_cast<int64_t>(legacy_us));
        response.set("abba_m1_us", static_cast<int64_t>(abba_m1_us));
        response.set("abba_l2_us", static_cast<int64_t>(abba_l2_us));
        response.set("abba_l3_us", static_cast<int64_t>(abba_l3_us));
        response.set("abba_m4_us", static_cast<int64_t>(abba_m4_us));
        response.set("baab_l1_us", static_cast<int64_t>(baab_l1_us));
        response.set("baab_m2_us", static_cast<int64_t>(baab_m2_us));
        response.set("baab_m3_us", static_cast<int64_t>(baab_m3_us));
        response.set("baab_l4_us", static_cast<int64_t>(baab_l4_us));
        response.set("managed_us_per_pixel", managed_per);
        response.set("legacy_us_per_pixel", legacy_per);
        response.set("managed_pixels_per_second",
                     managed_per > 0.0 ? 1.0e6 / managed_per : 0.0);
        response.set("managed_fnv1a", static_cast<int64_t>(managed_hash));
        response.set("legacy_fnv1a", static_cast<int64_t>(legacy_hash));
        response.set("managed_second_fnv1a", static_cast<int64_t>(managed_second_hash));
        response.set("legacy_second_fnv1a", static_cast<int64_t>(legacy_second_hash));
        response.set("baab_managed_fnv1a", static_cast<int64_t>(baab_managed_hash));
        response.set("baab_legacy_fnv1a", static_cast<int64_t>(baab_legacy_hash));
        return response;
    });
#endif
}

#endif  // FL_PLATFORM_HAS_LARGE_MEMORY
