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
#include "fl/channels/rx_sct_capture.h"
#include "fl/stl/strstream.h"

namespace {
fl::Remote* g_remote = nullptr;

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

    // ------------------------------------------------------------------
    // pinToggleRx (FastLED #3021 Phase 1)
    // ------------------------------------------------------------------
    // Drive tx_pin with a freq_hz square wave for duration_ms, while SCT
    // input-capture latches every edge on rx_pin. Returns CSV stats so the
    // Python orchestrator can assert mean and σ thresholds.
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

            // Format CSV. fl::strstream + manual concat is the lightest
            // option that avoids snprintf bloat on M0+.
            fl::strstream s;
            const bool success = (stats.periods > 0);
            s << (success ? 1 : 0) << ',' << static_cast<fl::u32>(n_read) << ','
              << stats.periods << ',' << stats.mean_ns << ',' << stats.sigma_ns << ','
              << stats.min_ns << ',' << stats.max_ns;
            return s.str();
        });
}

void loop() {
    fl::Watchdog::instance().feed();
    if (g_remote) {
        g_remote->update(millis());
    }
}
