// FlexIO RX RPC bindings: flexioRxBenchmark, flexioObjectFledTest, flexioRxLoopbackPing.
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


void AutoResearchRemoteControl::bindFlexioMethods(fl::Remote& remote) {
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
    remote.bind("flexioRxBenchmark", [this](const fl::json& args) -> fl::json {
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
    remote.bind("flexioObjectFledTest", [this](const fl::json& args) -> fl::json {
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

    // Register "flexioRxLoopbackPing" — FastLED#3066 phase 1.7 diagnostic.
    //
    // Bypasses every clockless driver entirely and tests whether the
    // FlexIO RX backend can capture HAND-DRIVEN GPIO transitions. Drives
    // `tx_pin` as a plain `digitalWrite` HIGH/LOW square wave while
    // `RxBackend::FLEXIO` is armed on `rx_pin`. If the captured buffer
    // shows any non-zero data, IOMUX ALT4 + PINSEL routing is correct
    // and the WS2812-loopback failure mode is downstream (timer/shifter
    // bandwidth, decoder thresholds, etc.). If the buffer stays all
    // zero with a 4-edge ground-truth pin toggle, the routing itself is
    // broken — debug that BEFORE more timer/shifter config experiments.
    //
    // Wiring: same TX↔RX jumper as flexioObjectFledTest (defaults 3↔4).
    //
    // Args:
    //   { tx_pin?:    3,    // any digital pin
    //     rx_pin?:    4,    // must be in kFlexIo1Pins[]
    //     toggle_us?: 50,   // half-period of the manual square wave
    //     edges?:     8 }   // number of digitalWrite transitions
    // Returns:
    //   { success, tx_pin, rx_pin, toggle_us, edges, edges_captured,
    //     capture_buffer_first8_hex: [...], notes }
    //
    // Teensy-4-only.
    remote.bind("flexioRxLoopbackPing", [this](const fl::json& args) -> fl::json {
        fl::json response = fl::json::object();
#if !defined(FL_IS_TEENSY_4X)
        (void)args;
        response.set("success", false);
        response.set("error", "PlatformNotSupported");
        response.set("message",
                     "flexioRxLoopbackPing is Teensy 4.x-only (FLEXIO1 RX).");
        return response;
#else
        int tx_pin = 3;
        int rx_pin = 4;
        int toggle_us = 50;
        int edges = 8;
        if (args.is_array() && args.size() >= 1 && args[0].is_object()) {
            const fl::json &cfg = args[0];
            if (cfg.contains("tx_pin") && cfg["tx_pin"].is_int()) {
                tx_pin = static_cast<int>(cfg["tx_pin"].as_int().value());
            }
            if (cfg.contains("rx_pin") && cfg["rx_pin"].is_int()) {
                rx_pin = static_cast<int>(cfg["rx_pin"].as_int().value());
            }
            if (cfg.contains("toggle_us") && cfg["toggle_us"].is_int()) {
                toggle_us = static_cast<int>(cfg["toggle_us"].as_int().value());
            }
            if (cfg.contains("edges") && cfg["edges"].is_int()) {
                edges = static_cast<int>(cfg["edges"].as_int().value());
            }
        }
        if (edges < 2 || edges > 64) {
            response.set("success", false);
            response.set("error", "InvalidEdges");
            response.set("message", "edges must be in [2, 64]");
            return response;
        }

        // 1. Arm FlexIO RX on rx_pin.
        fl::RxChannelConfig rx_cfg(rx_pin, fl::RxBackend::FLEXIO);
        rx_cfg.edge_capacity = 1024;   // plenty for ~10 edges
        rx_cfg.start_low = true;
        auto rx_channel = fl::RxChannel::create(rx_cfg);
        if (!rx_channel || !rx_channel->begin(rx_cfg)) {
            response.set("success", false);
            response.set("error", "RxBeginFailed");
            response.set("message",
                         "FlexIO RX begin() failed (kFlexIo1Pins[] mapping?).");
            return response;
        }

        // 2. Drive tx_pin as a plain GPIO output, toggle a known number of
        //    transitions. Start LOW so the first digitalWrite(HIGH) is a
        //    rising edge.
        pinMode(tx_pin, OUTPUT);
        digitalWrite(tx_pin, LOW);
        delayMicroseconds(100);  // let the line settle + RX arm fully

        bool level = true;
        for (int i = 0; i < edges; ++i) {
            digitalWrite(tx_pin, level ? HIGH : LOW);
            delayMicroseconds(toggle_us);
            level = !level;
        }

        // FastLED#3066 iter 7 diagnostic: also probe SHIFTBUF[0] +
        // SHIFTBUFBIS (bit-swapped) + SHIFTBUFBYS (byte-swapped) so we
        // can see whether the captured bit lands in a bit position
        // different from what the canonical SHIFTBUF[0] read produces.
        {
            constexpr uintptr_t kFLEXIO1_BASE_DIAG = 0x401AC000u;
            volatile uint32_t *pin   = (volatile uint32_t *)(kFLEXIO1_BASE_DIAG + 0x00C);
            volatile uint32_t *shftstat = (volatile uint32_t *)(kFLEXIO1_BASE_DIAG + 0x010);
            volatile uint32_t *timstat = (volatile uint32_t *)(kFLEXIO1_BASE_DIAG + 0x018);
            volatile uint32_t *shftbuf  = (volatile uint32_t *)(kFLEXIO1_BASE_DIAG + 0x200);
            volatile uint32_t *shftbufbis = (volatile uint32_t *)(kFLEXIO1_BASE_DIAG + 0x280);
            volatile uint32_t *shftbufbys = (volatile uint32_t *)(kFLEXIO1_BASE_DIAG + 0x300);
            FL_WARN("[ping diag] post-toggle FLEXIO1: PIN=0x"
                    << fl::hex << *pin
                    << " SHIFTSTAT=0x" << *shftstat
                    << " TIMSTAT=0x" << *timstat << fl::dec);
            FL_WARN("[ping diag] SHIFTBUF[0]=0x" << fl::hex << *shftbuf
                    << " SHIFTBUFBIS[0]=0x" << *shftbufbis
                    << " SHIFTBUFBYS[0]=0x" << *shftbufbys << fl::dec);
        }

        // 3. Wait briefly for FlexIO RX to flush any pending DMA. The DMA
        //    is configured for completion-on-buffer-full; a partial fill
        //    won't trigger the ISR, so `wait()` will TIMEOUT — which is
        //    fine, we read the captured edges directly.
        rx_channel->wait(50);

        // 4. Read the captured edges via the public API.
        fl::vector<fl::EdgeTime> captured_edges;
        captured_edges.assign(64, fl::EdgeTime());
        size_t edge_count =
            rx_channel->getRawEdgeTimes(fl::span<fl::EdgeTime>(captured_edges), 0);

        // 5. Restore tx_pin so subsequent tests can use it.
        pinMode(tx_pin, INPUT);

        response.set("success", true);
        response.set("tx_pin", static_cast<int64_t>(tx_pin));
        response.set("rx_pin", static_cast<int64_t>(rx_pin));
        response.set("toggle_us", static_cast<int64_t>(toggle_us));
        response.set("edges", static_cast<int64_t>(edges));
        response.set("edges_captured", static_cast<int64_t>(edge_count));

        // Dump the first 8 captured edge durations (ns) so the host can
        // see whether they roughly match the requested toggle_us *
        // 1000 ns expectation.
        fl::json edges_arr = fl::json::array();
        const size_t to_dump = (edge_count < 8u) ? edge_count : 8u;
        for (size_t i = 0; i < to_dump; ++i) {
            fl::json e = fl::json::object();
            e.set("high", captured_edges[i].high);
            e.set("ns", static_cast<int64_t>(captured_edges[i].ns));
            edges_arr.push_back(e);
        }
        response.set("first_edges", edges_arr);
        return response;
#endif
    });
}

#endif // !(FASTLED_AUTORESEARCH_LOW_MEMORY)
