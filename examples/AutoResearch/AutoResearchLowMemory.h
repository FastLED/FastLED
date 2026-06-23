#pragma once
//
// examples/AutoResearch/AutoResearchLowMemory.h
//
// Low-memory AutoResearch bring-up surface for parts whose flash budget
// cannot fit the full ESP32 / Teensy LED-protocol matrix. Originally lived
// as the standalone `examples/AutoResearchLpc/AutoResearchLpc.ino` sketch;
// retired and folded in here per FastLED #3030 once the soft-FP cascade
// from #3022 phase 2 freed up the budget on LPC8xx.
//
// Included from the main `AutoResearch.ino` when
// `FASTLED_AUTORESEARCH_LOW_MEMORY` is defined (auto-defined when
// `FL_PLATFORM_HAS_LARGE_MEMORY == 0`, i.e. Low + Tiny tiers per the
// FastLED #3000 memory classification).
//
// Same JSON-RPC contract as the retired LPC sketch:
//   echo: [int] -> int
//   pinToggleRx: [tx_pin, rx_pin, freq_hz, duration_ms] -> CSV
//   ws2812SctTest: [test_case, tx_pin, rx_pin, capture_ms] -> CSV
// See FastLED #3021 for the SCT-RX bring-up flags.
//

// Gate FL_DBG to no-op. Without `RELEASE`, fl/log/log.h auto-sets
// FASTLED_FORCE_DBG=1, which expands every FL_DBG call site (inside
// fl::Remote etc.) through the fl::println formatting machinery and
// blows ~4 KB of flash budget. fbuild's nxplpc orchestrator does not
// yet propagate platformio.ini's `build_flags`, so it's defined here.
#ifndef RELEASE
#define RELEASE 1
#endif

// Opt in to the SCT input-capture RX backend (FastLED #3021). With this
// flag set, `LpcSctRxChannel::begin()` programs the SCT for hardware
// edge-capture; without it the driver is a no-op stub (host tests still
// work via `injectEdges()`).
#ifndef FASTLED_LPC_RX_SCT
#define FASTLED_LPC_RX_SCT 1
#endif

// Opt in to the SCT->DMA RX capture path. Required for WS2812 hardware
// loopback. Auto-enabled when WS2812 mode is on.
#if defined(FASTLED_LPC_RX_SCT_WS2812) && !defined(FASTLED_LPC_RX_SCT_DMA)
#define FASTLED_LPC_RX_SCT_DMA 1
#endif

#include <Arduino.h>
#include "fl/remote/remote.h"
#include "fl/remote/transport/serial.h"
#include "fl/wdt/watchdog.h"
#include "fl/log/log.h"
#include "fl/stl/cstdio.h"  // fl::serial_begin -- HWCDC-safe Serial.begin wrapper

// The host-stub example DLL build (`tests/shared/example_dll_wrapper_template.cpp`)
// compiles this sketch on Linux to surface ABI breaks. The RX SCT driver
// types are platform-gated behind `FL_IS_ARM_LPC` -- they only exist on
// the real LPC build. So the include + every RX usage below must be gated
// the same way.
#include "platforms/arm/lpc/is_lpc.h"  // ok platform headers - LPC RX driver gate
#if defined(FL_IS_ARM_LPC)
#include "fl/channels/rx_sct_capture.h"
#include "fl/stl/strstream.h"
#endif

#if defined(FASTLED_LPC_RX_SCT_WS2812)
#include "FastLED.h"
#include "fl/chipsets/timing_traits.h"
#endif

namespace {
fl::Remote* g_low_memory_remote = nullptr;

#if defined(FL_IS_ARM_LPC)
// Period-stat helpers for `pinToggleRx`. Mirrors the FlexIO RX benchmark
// logic in `examples/AutoResearch/AutoResearchRemote.cpp::flexioRxBenchmark`
// but inline so we don't pull in the AutoResearch ObjectFLED bus.
struct LowMemPinTogglePeriodStats {
    fl::u32 periods;
    fl::u32 mean_ns;
    fl::u32 sigma_ns;
    fl::u32 min_ns;
    fl::u32 max_ns;
};

inline LowMemPinTogglePeriodStats computeLowMemPeriodStats(
    const fl::EdgeTime* edges, fl::size edge_count) FL_NOEXCEPT {
    LowMemPinTogglePeriodStats s{0, 0, 0, 0, 0};
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

inline void autoResearchLowMemorySetup() {
    fl::serial_begin(115200);

    // Arm the watchdog so a hung sketch unbricks itself on crash.
    fl::Watchdog::instance().begin(3000);

    // Emit a literal-only warning so the autoresearch harness can verify
    // the FL_WARN log pipeline reaches the host.
    FL_WARN_LIT("FL_WARN: low-memory bring-up OK");

    // The C++ inline-ODR rule guarantees a single shared instance across
    // every translation unit that calls this inline function, which is
    // exactly what we want (one Remote bound to the singleton Serial
    // transport). The included-once-per-sketch model of `.ino` builds
    // also reduces to a single TU in practice.
    static fl::Remote remote(  // okay static in header
        fl::createSerialRequestSource(),
        fl::createSerialResponseSink("REMOTE: "));
    g_low_memory_remote = &remote;
    remote.bind("echo", [](int v) -> int {
        FL_WARN_LIT("FL_WARN: echo invoked");
        return v;
    });

#if defined(FL_IS_ARM_LPC)
    // pinToggleRx (FastLED #3021 Phase 1) — bit-bang square wave on tx_pin
    // and capture SCT edges on rx_pin. CSV stats out.
    remote.bind("pinToggleRx",
        [](int tx_pin, int rx_pin, int freq_hz, int duration_ms) -> fl::string {
            if (tx_pin < 0 || rx_pin < 0 || freq_hz <= 0 || duration_ms <= 0) {
                return fl::string("0,0,0,0,0,0,0");
            }
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
            rx->wait(1);

            fl::EdgeTime edges_buf[2048];
            const fl::size n_read = rx->getRawEdgeTimes(
                fl::span<fl::EdgeTime>(edges_buf, sizeof(edges_buf) / sizeof(edges_buf[0])));

            LowMemPinTogglePeriodStats stats = computeLowMemPeriodStats(edges_buf, n_read);

            fl::sstream s;
            const bool success = (stats.periods > 0);
            s << (success ? 1 : 0) << ',' << static_cast<fl::u32>(n_read) << ','
              << stats.periods << ',' << stats.mean_ns << ',' << stats.sigma_ns << ','
              << stats.min_ns << ',' << stats.max_ns;
            return s.str();
        });

#if defined(FASTLED_LPC_RX_SCT_WS2812)
    // ws2812SctTest (FastLED #3021 Phase 2) -- WS2812 byte-match loopback.
    remote.bind("ws2812SctTest",
        [](int test_case, int tx_pin, int rx_pin, int capture_ms) -> fl::string {
            if (test_case < 0 || test_case > 4 ||
                tx_pin < 0 || rx_pin < 0 || capture_ms <= 0) {
                return fl::string("0,0,0,0,0,0,0,0");
            }
            int num_leds = 1;
            switch (test_case) {
                case 1: num_leds = 3; break;
                case 4: num_leds = 100; break;
                default: num_leds = 1; break;
            }
            static CRGB leds_buf[100];
            for (int i = 0; i < num_leds; ++i) {
                switch (test_case) {
                    case 0: leds_buf[i] = CRGB(0xFF, 0x00, 0x00); break;
                    case 1: {
                        if (i == 0)      leds_buf[i] = CRGB(0xFF, 0x00, 0x00);
                        else if (i == 1) leds_buf[i] = CRGB(0x00, 0xFF, 0x00);
                        else             leds_buf[i] = CRGB(0x00, 0x00, 0xFF);
                        break;
                    }
                    case 2: leds_buf[i] = CRGB(0x00, 0x00, 0x00); break;
                    case 3: leds_buf[i] = CRGB(0xFF, 0xFF, 0xFF); break;
                    case 4: {
                        const uint8_t r = (i % 3 == 0) ? 0xFFu : 0u;
                        const uint8_t g = (i % 3 == 1) ? 0xFFu : 0u;
                        const uint8_t b = (i % 3 == 2) ? 0xFFu : 0u;
                        leds_buf[i] = CRGB(r, g, b);
                        break;
                    }
                }
            }
            const int expected_bytes = num_leds * 3;
            static uint8_t expected_buf[300];
            for (int i = 0; i < num_leds; ++i) {
                expected_buf[i * 3 + 0] = leds_buf[i].g;
                expected_buf[i * 3 + 1] = leds_buf[i].r;
                expected_buf[i * 3 + 2] = leds_buf[i].b;
            }
            auto rx = fl::LpcSctRxChannel::create(rx_pin);
            if (!rx) return fl::string("0,0,0,0,0,0,0,0");
            fl::RxConfig cfg;
            const fl::u32 expected_edges = (fl::u32)num_leds * 24u * 2u;
            fl::u32 cap = expected_edges + (expected_edges / 2u);
            if (cap < 256u)  cap = 256u;
            if (cap > 2048u) cap = 2048u;
            cfg.buffer_size = cap;
            cfg.start_low   = true;
            if (!rx->begin(cfg)) return fl::string("0,0,0,0,0,0,0,0");

            constexpr int kFixedTxPin = 10;  // P0_10
            if (tx_pin != kFixedTxPin) {
                return fl::string("0,0,0,0,0,0,0,0");
            }
            static bool fastled_inited = false;
            if (!fastled_inited) {
                FastLED.addLeds<WS2812, kFixedTxPin, GRB>(leds_buf, 100);
                fastled_inited = true;
            }
            FastLED.show();
            rx->wait(capture_ms);

            static uint8_t decoded_buf[300];
            for (int i = 0; i < expected_bytes; ++i) decoded_buf[i] = 0;

            fl::ChipsetTiming4Phase timing =
                fl::make4PhaseTiming(
                    fl::to_runtime_timing<fl::WS2812ChipsetTiming>(),
                    /*reset_us=*/0u);

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

inline void autoResearchLowMemoryLoop() {
    fl::Watchdog::instance().feed();
    if (g_low_memory_remote) {
        g_low_memory_remote->update(millis());
    }
}
