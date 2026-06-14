// FL_AGENT_ALLOW_NEW_EXAMPLE
//
// @filter
// require:
//   - board: lpc845brk
// @end-filter
//
// Gate FL_DBG to no-op. Without `RELEASE`, fl/log/log.h auto-sets
// FASTLED_FORCE_DBG=1, which expands every FL_DBG call site (inside
// fl::Remote etc.) through the fl::println formatting machinery and
// blows ~4 KB of flash budget. fbuild's nxplpc orchestrator does not
// yet propagate platformio.ini's `build_flags`, so it's defined here.
#define RELEASE 1

// Opt in to the SCT input-capture RX backend (FastLED #3021). With this
// flag set, `LpcSctRxChannel::begin()` programs the SCT for hardware
// edge-capture; without it the driver is a no-op stub (host tests still
// work via `injectEdges()`). The Phase-1 acceptance for #3021 requires
// the lpc845brk fbuild target to compile with this flag set.
#define FASTLED_LPC_RX_SCT 1

// Opt in to the Phase-2 WS2812 byte-match loopback test (FastLED #3021).
// Pulls in `<FastLED.h>` and the WS2812 driver to enable the `ws2812SctTest`
// RPC. **Default off** because the LPC845 flash budget (64 KB) overflows
// when FastLED.h is included alongside fl::Remote until FastLED #3002
// (fl::json soft-FP elimination) lands. Once #3002 ships, this flag can
// be enabled by default for the lpc845brk fbuild target.
//
// #define FASTLED_LPC_RX_SCT_WS2812 1

//
// examples/AutoResearchLpc/AutoResearchLpc.ino
//
// AutoResearch bring-up harness for NXP LPC845-BRK (Cortex-M0+, 64 KB Flash,
// 16 KB SRAM). Mirrors the JSON-RPC contract of the main AutoResearch.ino but
// strips out the LED-protocol matrix (RMT/PARLIO/SPI/etc.) that the LPC8xx
// family doesn't have peripherals for.
//
// Purpose: validate that on a freshly-brought-up board the basic transport
// (Serial.println, FastLED log pipeline, JSON-RPC echo) all work end-to-end
// through `bash autoresearch lpc845brk --bring-up`, plus the SCT-RX
// pin-toggle loopback that closes the FastLED #3021 Phase-1 gate.
//
// RPC contract (over Serial @ 115200, JSON-RPC 2.0 framed by `\n`,
// responses prefixed with `REMOTE: `):
//
//   echo: [int] -> int
//     Returns the input integer unchanged. The bring-up harness sends
//     {"jsonrpc":"2.0","method":"echo","params":[N],"id":I} and verifies
//     the response is {"id":I,"result":N,"jsonrpc":"2.0"}.
//
//   pinToggleRx: [tx_pin, rx_pin, freq_hz, duration_ms] -> string
//     Phase-1 SCT-RX validation (FastLED #3021). Bit-bangs a square wave
//     on tx_pin and concurrently arms SCT input-capture on rx_pin (via
//     LpcSctRxChannel — the LPC8xx SCT input-capture driver).
//     Returns CSV: "success,edges,periods,mean_ns,sigma_ns,min_ns,max_ns"
//     where:
//       success    1 if at least one full period was captured, else 0
//       edges      total edges latched by SCT during the window
//       periods    number of complete (high+low) period pairs
//       mean_ns    mean of all period durations in ns
//       sigma_ns   standard deviation of period durations in ns
//       min_ns,max_ns   period extrema in ns
//     Wiring: jumper tx_pin to rx_pin externally on the LPC845-BRK header.
//
//   ws2812SctTest: [test_case, tx_pin, rx_pin, capture_ms] -> string
//     Phase-2 SCT-RX WS2812 byte-match loopback (FastLED #3021). Builds
//     a `CRGB[N]` pattern, kicks `FastLED.show()` through the bit-bang
//     `clockless_arm_lpc.h` driver on tx_pin, captures the wire on
//     rx_pin via SCT, decodes the 4-phase WS2812 timing, and reports
//     how many bytes round-tripped intact.
//     Returns CSV: "success,test_case,num_leds,exp_bytes,dec_bytes,matched,mismatched,edges"
//     ONLY available when the sketch is compiled with
//     `-DFASTLED_LPC_RX_SCT_WS2812=1` (default off — see flash budget
//     note at the top of the file).
//
// FL_DBG output from inside fl::Remote (e.g. "Stored request ID for echo")
// also reaches the host through this transport, demonstrating that the
// FastLED log pipeline is wired correctly. FL_WARN proper is gated behind
// SKETCH_HAS_LARGE_MEMORY which is off on LPC845 (flash budget); enabling
// it requires the fl::json fixed-point refactor tracked in FastLED #3002.

#include <Arduino.h>
#include "fl/remote/remote.h"
#include "fl/remote/transport/serial.h"
#include "fl/wdt/watchdog.h"
#include "fl/log/log.h"

// The host-stub example DLL build (`tests/shared/example_dll_wrapper_template.cpp`)
// compiles this sketch on Linux to surface ABI breaks. The RX SCT driver
// types (`fl::EdgeTime`, `fl::RxConfig`, `fl::LpcSctRxChannel`) are
// platform-gated behind `FL_IS_ARM_LPC` — they only exist on the real
// LPC build. So the include + every RX usage below must be gated the
// same way.
#include "platforms/arm/lpc/is_lpc.h"
#if defined(FL_IS_ARM_LPC)
#include "fl/channels/rx_sct_capture.h"
#include "fl/stl/strstream.h"
#endif

#if defined(FASTLED_LPC_RX_SCT_WS2812)
#include "FastLED.h"
#include "fl/chipsets/timing_traits.h"
#endif

namespace {
fl::Remote* g_remote = nullptr;

#if defined(FL_IS_ARM_LPC)
// ----------------------------------------------------------------------
// Period-stat helpers for `pinToggleRx`. Mirror the FlexIO RX benchmark
// logic in `examples/AutoResearch/AutoResearchRemote.cpp::flexioRxBenchmark`
// but inline so we don't pull in the AutoResearch ObjectFLED bus.
// ----------------------------------------------------------------------
struct PinTogglePeriodStats {
    fl::u32 periods;
    fl::u32 mean_ns;
    fl::u32 sigma_ns;
    fl::u32 min_ns;
    fl::u32 max_ns;
};

PinTogglePeriodStats computePeriodStats(const fl::EdgeTime* edges,
                                        fl::size edge_count) FL_NOEXCEPT {
    PinTogglePeriodStats s{0, 0, 0, 0, 0};
    fl::u64 sum    = 0;
    fl::u64 sum_sq = 0;
    fl::u32 min_ns = 0xFFFFFFFFu;
    fl::u32 max_ns = 0;
    fl::u32 count  = 0;
    for (fl::size i = 0; i + 1 < edge_count; i += 2) {
        const fl::u32 period_ns =
            static_cast<fl::u32>(edges[i].ns) +
            static_cast<fl::u32>(edges[i + 1].ns);
        if (period_ns == 0) continue;
        sum    += period_ns;
        sum_sq += static_cast<fl::u64>(period_ns) * static_cast<fl::u64>(period_ns);
        if (period_ns < min_ns) min_ns = period_ns;
        if (period_ns > max_ns) max_ns = period_ns;
        ++count;
    }
    if (count == 0) return s;
    const fl::u32 mean = static_cast<fl::u32>(sum / static_cast<fl::u64>(count));
    const fl::u64 mean64 = static_cast<fl::u64>(mean);
    const fl::u64 var = (sum_sq / static_cast<fl::u64>(count)) - (mean64 * mean64);
    // Integer sqrt for sigma — adequate at the σ < 500 ns scale.
    fl::u32 sig = 0;
    while (static_cast<fl::u64>(sig + 1) * static_cast<fl::u64>(sig + 1) <= var) ++sig;
    s.periods  = count;
    s.mean_ns  = mean;
    s.sigma_ns = sig;
    s.min_ns   = (min_ns == 0xFFFFFFFFu) ? 0u : min_ns;
    s.max_ns   = max_ns;
    return s;
}
#endif  // FL_IS_ARM_LPC

}  // namespace

void setup() {
    Serial.begin(115200);

    // Arm the LPC845 WWDT so a hung sketch unbricks itself on crash.
    // ~3 second window — long enough for any legitimate setup() work,
    // short enough that "device is dead" reboots within human attention
    // span during bring-up debugging. See FastLED #3002 follow-up.
    fl::Watchdog::instance().begin(3000);

    // Emit a literal-only warning so the autoresearch harness can verify
    // the FL_WARN log pipeline reaches the host. `FL_WARN_LIT` bypasses
    // the full sstream/log_emit chain (which is no-op'd on Low-memory
    // targets) and goes straight to `fl::println(const char*)`, so it
    // works on the LPC845 flash budget. See FastLED #3002.
    FL_WARN_LIT("FL_WARN: LPC845 bring-up OK");

    static fl::Remote remote(
        fl::createSerialRequestSource(),
        fl::createSerialResponseSink("REMOTE: "));
    g_remote = &remote;
    remote.bind("echo", [](int v) -> int {
        // Re-emit the FL_WARN marker on every echo so the autoresearch
        // harness can observe it even after the boot-banner drain step
        // (see ci/autoresearch/phases.py — `_run_bring_up_tests` resets
        // the input buffer twice before sending the first request).
        FL_WARN_LIT("FL_WARN: echo invoked");
        return v;
    });

#if defined(FL_IS_ARM_LPC)
    // ------------------------------------------------------------------
    // pinToggleRx (FastLED #3021 Phase 1)
    // ------------------------------------------------------------------
    // Drive tx_pin with a freq_hz square wave for duration_ms, while SCT
    // input-capture latches every edge on rx_pin. Returns CSV stats so the
    // Python orchestrator can assert mean and σ thresholds.
    //
    // Gated behind FL_IS_ARM_LPC so the example DLL host-stub build
    // (tests/shared/example_dll_wrapper_template.cpp) doesn't try to
    // compile the LPC-only RX driver path.
    //
    // CSV slot ordering (must match ci/autoresearch/test_lpc_pin_toggle_rx.py):
    //   success, edges, periods, mean_ns, sigma_ns, min_ns, max_ns
    remote.bind("pinToggleRx",
        [](int tx_pin, int rx_pin, int freq_hz, int duration_ms) -> fl::string {
            // Defensive arg clamp (M0+ has no exceptions; bad inputs just
            // truncate so the orchestrator sees `success=0`).
            if (tx_pin < 0 || rx_pin < 0 || freq_hz <= 0 || duration_ms <= 0) {
                return fl::string("0,0,0,0,0,0,0");
            }

            // Size the capture buffer for the expected edge count + 50%
            // headroom. Clamped to a ceiling that fits LPC845 SRAM
            // (16 KB total — 8 bytes/EdgeTime → 2048 edges = 16 KB hard
            // ceiling; in practice we keep well below to leave room for
            // the rest of the runtime).
            const fl::u32 expected_edges =
                2u * static_cast<fl::u32>(freq_hz) *
                     static_cast<fl::u32>(duration_ms) / 1000u;
            fl::u32 cap = expected_edges + (expected_edges / 2u);
            if (cap < 256u)  cap = 256u;
            if (cap > 2048u) cap = 2048u;

            auto rx = fl::LpcSctRxChannel::create(rx_pin);
            if (!rx) return fl::string("0,0,0,0,0,0,0");

            fl::RxConfig cfg;
            cfg.buffer_size = cap;
            cfg.start_low   = true;
            if (!rx->begin(cfg)) return fl::string("0,0,0,0,0,0,0");

            // Bit-bang TX *while* concurrently polling SCT input-capture.
            // The polling-mode RX driver can't autonomously fill its
            // EdgeTime ring (it requires someone to read CAP[0..1]
            // between edges, otherwise each new capture overwrites the
            // previous one in-register). To work around that on a
            // single-threaded M0+, the bit-bang loop calls rx->pollOnce()
            // between each digitalWrite, draining any pending edge.
            //
            // Phase 2 (#3021) upgrades the RX path to SCT→DMA, at which
            // point this interleave goes away — pollOnce() becomes a no-op
            // and wait() just blocks on the DMA-done flag.
            pinMode(tx_pin, OUTPUT);
            digitalWrite(tx_pin, LOW);

            const fl::u32 half_us = 500000u / static_cast<fl::u32>(freq_hz);
            const fl::u32 cycles  = static_cast<fl::u32>(freq_hz) *
                                     static_cast<fl::u32>(duration_ms) / 1000u;

            for (fl::u32 i = 0; i < cycles; ++i) {
                digitalWrite(tx_pin, HIGH);
                delayMicroseconds(half_us);
                rx->pollOnce();
                digitalWrite(tx_pin, LOW);
                delayMicroseconds(half_us);
                rx->pollOnce();
                if ((i & 0x7Fu) == 0u) {
                    fl::Watchdog::instance().feed();
                }
            }

            // Final drain — picks up any in-flight edge that the last
            // pollOnce() missed (the LOW→HIGH or HIGH→LOW transition
            // that races with the loop exit).
            rx->wait(1);

            // Pull the captured edges out.
            fl::EdgeTime edges_buf[2048];
            const fl::size n_read = rx->getRawEdgeTimes(
                fl::span<fl::EdgeTime>(edges_buf, sizeof(edges_buf) / sizeof(edges_buf[0])));

            PinTogglePeriodStats stats = computePeriodStats(edges_buf, n_read);

            // Format CSV. fl::sstream + manual concat is the lightest
            // option that avoids snprintf bloat on M0+.
            fl::sstream s;
            const bool success = (stats.periods > 0);
            s << (success ? 1 : 0) << ',' << static_cast<fl::u32>(n_read) << ','
              << stats.periods << ',' << stats.mean_ns << ',' << stats.sigma_ns << ','
              << stats.min_ns << ',' << stats.max_ns;
            return s.str();
        });

#if defined(FASTLED_LPC_RX_SCT_WS2812)
    // ------------------------------------------------------------------
    // ws2812SctTest (FastLED #3021 Phase 2)
    // ------------------------------------------------------------------
    // End-to-end WS2812 byte-match loopback. Mirrors the FlexIO
    // `flexioObjectFledTest` 5-case shape:
    //   0 — Red single LED            (1 LED, 0xFF0000 → GRB 00,FF,00)
    //   1 — RGB three-LED chain       (3 LEDs)
    //   2 — All zeros                 (1 LED — T0H/T0L fidelity)
    //   3 — All ones                  (1 LED — T1H/T1L fidelity)
    //   4 — 100-LED alternating R/G/B (long capture, watchdog-safety)
    //
    // **Hardware status:** the SCT capture path used here is the same
    // polling-mode `pollOnce()` as Phase 1's pinToggleRx. WS2812 runs
    // at 800 kHz with 0.4–0.85 µs sub-pulses, faster than the M0+
    // polling loop can keep up with. This handler ships the API +
    // pattern surface so the Python orchestrator can be exercised
    // and so the SCT→DMA RX upgrade has a callable RPC to target
    // in the follow-up. Until DMA lands, `mismatched` will be
    // non-zero in practice on real silicon — that's expected.
    //
    // CSV slot ordering (must match
    // ci/autoresearch/test_lpc_ws2812_loopback.py):
    //   success, test_case, num_leds, exp_bytes, dec_bytes,
    //   matched, mismatched, edges_captured
    remote.bind("ws2812SctTest",
        [](int test_case, int tx_pin, int rx_pin, int capture_ms) -> fl::string {
            if (test_case < 0 || test_case > 4 ||
                tx_pin < 0 || rx_pin < 0 || capture_ms <= 0) {
                return fl::string("0,0,0,0,0,0,0,0");
            }

            // Configure the test pattern + LED count for each case.
            int num_leds = 1;
            switch (test_case) {
                case 1: num_leds = 3; break;
                case 4: num_leds = 100; break;
                default: num_leds = 1; break;
            }

            // The bit-bang clockless driver writes GRB on the wire.
            // Build the in-memory CRGB buffer per case; the wire
            // bytes are CRGB.toGrb() per pixel.
            static CRGB leds_buf[100];
            for (int i = 0; i < num_leds; ++i) {
                switch (test_case) {
                    case 0: leds_buf[i] = CRGB(0xFF, 0x00, 0x00); break;  // red
                    case 1: {
                        if (i == 0)      leds_buf[i] = CRGB(0xFF, 0x00, 0x00);
                        else if (i == 1) leds_buf[i] = CRGB(0x00, 0xFF, 0x00);
                        else             leds_buf[i] = CRGB(0x00, 0x00, 0xFF);
                        break;
                    }
                    case 2: leds_buf[i] = CRGB(0x00, 0x00, 0x00); break;  // all zeros
                    case 3: leds_buf[i] = CRGB(0xFF, 0xFF, 0xFF); break;  // all ones
                    case 4: {
                        const uint8_t r = (i % 3 == 0) ? 0xFFu : 0u;
                        const uint8_t g = (i % 3 == 1) ? 0xFFu : 0u;
                        const uint8_t b = (i % 3 == 2) ? 0xFFu : 0u;
                        leds_buf[i] = CRGB(r, g, b);
                        break;
                    }
                }
            }

            // Build the expected wire bytes (GRB-ordered, MSB first
            // per WS2812). The decoder gives back the same wire bytes.
            const int expected_bytes = num_leds * 3;
            static uint8_t expected_buf[300];
            for (int i = 0; i < num_leds; ++i) {
                expected_buf[i * 3 + 0] = leds_buf[i].g;
                expected_buf[i * 3 + 1] = leds_buf[i].r;
                expected_buf[i * 3 + 2] = leds_buf[i].b;
            }

            // Arm SCT capture on rx_pin.
            auto rx = fl::LpcSctRxChannel::create(rx_pin);
            if (!rx) return fl::string("0,0,0,0,0,0,0,0");
            fl::RxConfig cfg;
            // 24 bits/LED × 2 edges/bit + 50% headroom, clamped.
            const fl::u32 expected_edges = (fl::u32)num_leds * 24u * 2u;
            fl::u32 cap = expected_edges + (expected_edges / 2u);
            if (cap < 256u)  cap = 256u;
            if (cap > 2048u) cap = 2048u;
            cfg.buffer_size = cap;
            cfg.start_low   = true;
            if (!rx->begin(cfg)) return fl::string("0,0,0,0,0,0,0,0");

            // Configure FastLED on tx_pin. Static so the controller
            // registry doesn't grow on repeated calls.
            //
            // NOTE: the bit-bang `clockless_arm_lpc.h` driver requires
            // a constexpr pin via the addLeds<CHIPSET, PIN> template;
            // we use a compile-time pin here matching the orchestrator
            // default (P0_10). Callers that pass a different tx_pin
            // get a "tx_pin not bound" error.
            constexpr int kFixedTxPin = 10;  // P0_10
            if (tx_pin != kFixedTxPin) {
                return fl::string("0,0,0,0,0,0,0,0");
            }
            static bool fastled_inited = false;
            if (!fastled_inited) {
                FastLED.addLeds<WS2812, kFixedTxPin, GRB>(leds_buf, 100);
                fastled_inited = true;
            }

            // Kick TX. While show() runs, the SCT is free-running and
            // CAP[0..1] is being clobbered by every edge. The
            // polling-mode pollOnce() inside wait() below cannot keep
            // up with WS2812's 800 kHz edge rate on a 30 MHz M0+,
            // hence the documented hardware caveat. The DMA upgrade
            // (#3021 follow-up) makes this section reliable.
            FastLED.show();
            rx->wait(capture_ms);

            // Decode the captured edges.
            static uint8_t decoded_buf[300];
            for (int i = 0; i < expected_bytes; ++i) decoded_buf[i] = 0;

            fl::ChipsetTiming4Phase timing =
                fl::make4PhaseTiming(fl::TimingTraits<WS2812Chipset>::T1,
                                     fl::TimingTraits<WS2812Chipset>::T2,
                                     fl::TimingTraits<WS2812Chipset>::T3);

            auto dec = rx->decode(timing, fl::span<u8>(decoded_buf, expected_bytes));
            const fl::u32 decoded_bytes = dec.ok() ? dec.value() : 0u;

            fl::u32 matched    = 0;
            fl::u32 mismatched = 0;
            for (fl::u32 i = 0; i < decoded_bytes && i < (fl::u32)expected_bytes; ++i) {
                if (decoded_buf[i] == expected_buf[i]) ++matched;
                else                                    ++mismatched;
            }
            const fl::u32 missing = (fl::u32)expected_bytes - decoded_bytes;
            mismatched += missing;

            // Edge count for diagnostics.
            fl::EdgeTime edges_drop[1];
            (void)rx->getRawEdgeTimes(fl::span<fl::EdgeTime>(edges_drop, 0));
            // The cleanest way to count edges is to read the ring's
            // length; the public API returns 0 when out.size() == 0
            // so use a larger window.
            fl::EdgeTime probe[1024];
            const fl::size edges_captured = rx->getRawEdgeTimes(
                fl::span<fl::EdgeTime>(probe, 1024));

            const bool success = (mismatched == 0 && decoded_bytes == (fl::u32)expected_bytes);

            fl::sstream s;
            s << (success ? 1 : 0) << ',' << test_case << ',' << num_leds << ','
              << expected_bytes << ',' << decoded_bytes << ','
              << matched << ',' << mismatched << ','
              << static_cast<fl::u32>(edges_captured);
            return s.str();
        });
#endif  // FASTLED_LPC_RX_SCT_WS2812
#endif  // FL_IS_ARM_LPC
}

void loop() {
    fl::Watchdog::instance().feed();
    if (g_remote) {
        g_remote->update(millis());
    }
}
