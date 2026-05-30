/// @file AutoResearchParlioStream.h
/// @brief Boot-time PARLIO streaming functional test (#2548 deep-dive follow-up).
///
/// Validates the engine's ISR-chunked streaming path on real hardware:
///   - Initializes PARLIO with **16 lanes** (forces the engine's 16-lane Wave8
///     branch, which dispatches to `wave8Transpose_16x4_bf1_pipe4` shipped in
///     #2559).
///   - Fills with a known pattern.
///   - Calls FastLED.show() — which goes through ParlioEngine's 3-buffer ring
///     + txDoneCallback ISR streaming + BF1 encode.
///   - Measures wall-clock time for show()+wait() to complete.
///
/// **Functional pass criterion**: show()+wait() completes < 100 ms. The
/// expected time is ~2 ms (BF1+pipe4 encode is 1.75 ms/frame, TX is 7.68 ms,
/// but the engine streams them concurrently so the bottleneck is the slower
/// of the two — TX). 100 ms gives ~10× margin and catches hangs / stalls.
///
/// Does NOT validate transmitted byte correctness — that needs RX loopback
/// with a jumper wire (handled by the existing autoresearch --parlio CLI).
/// Bit-exactness of BF1 is independently covered by the host test in
/// tests/fl/channels/wave8.cpp added in #2559.
///
/// Reports `BENCH_PARLIO_STREAM_VALIDATE PASS/FAIL` via `esp_rom_printf` to
/// the USB-Serial-JTAG console (COM25 on the P4 dev board), so it works
/// without the broken testSimd RPC routing (#2541).
///
/// Gated by `-DFL_PARLIO_STREAM_VALIDATE_AT_BOOT=1`. Runs once in setup().

#pragma once

#include "FastLED.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/chipsets/led_timing.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"

#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
#include <Arduino.h>      // micros()
#include <esp_rom_sys.h>  // esp_rom_printf
#endif

namespace autoresearch {
namespace parlio_stream {

/// Run at boot AFTER the RX channel is created (RX is unused — kept in the
/// signature for symmetry with future loopback variants).
///
/// @param base_tx_pin  First PARLIO TX pin; lanes occupy [base_tx_pin .. base_tx_pin+15]
inline void validateAtBoot(int base_tx_pin) {
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
    esp_rom_printf("\nBENCH_PARLIO_STREAM_VALIDATE_START\n");

    constexpr int kNumLanes = 16;
    constexpr int kNumLEDs = 256;   // matches the canonical bench workload
    constexpr uint32_t kTimeoutMs = 200;

    // 16 separate CRGB arrays, one per lane — forces the engine's 16-lane
    // Wave8 dispatch which uses wave8Transpose_16x4_bf1_pipe4.
    static CRGB leds[kNumLanes][kNumLEDs];
    for (int lane = 0; lane < kNumLanes; ++lane) {
        for (int i = 0; i < kNumLEDs; ++i) {
            // Pattern A: mixed bit positions per byte.
            leds[lane][i] = CRGB(0xF0, 0x0F, 0xAA);
        }
    }

    fl::ChipsetTimingConfig timing =
        fl::makeTimingConfig<fl::TIMING_WS2812B_V5>();

    // Build 16 PARLIO channels on consecutive pins.
    fl::ChannelOptions parlio_opts;
    parlio_opts.mBus = fl::Bus::PARLIO;

    fl::vector<fl::shared_ptr<fl::Channel>> channels;
    bool add_ok = true;
    for (int lane = 0; lane < kNumLanes; ++lane) {
        fl::ChannelConfig cfg(
            base_tx_pin + lane,
            timing,
            fl::span<CRGB>(leds[lane], kNumLEDs),
            RGB,
            parlio_opts);
        auto ch = FastLED.add(cfg);
        if (!ch) {
            esp_rom_printf("BENCH_PARLIO_STREAM_VALIDATE FAIL  "
                           "add_channel_failed_at_lane=%d\n", lane);
            add_ok = false;
            break;
        }
        channels.push_back(ch);
    }

    if (!add_ok) {
        FastLED.clear(ClearFlags::CHANNELS);
        esp_rom_printf("BENCH_PARLIO_STREAM_VALIDATE_END\n\n");
        return;
    }

    // Run several iterations to make sure streaming repeats cleanly across
    // back-to-back frames (catches single-frame-only bugs). First iter has
    // setup overhead; the steady-state cost is the average of iters 2..N.
    constexpr int kIterations = 5;
    uint32_t per_iter_us[8] = {0};
    bool ok = true;
    for (int iter = 0; iter < kIterations; ++iter) {
        const uint32_t t0 = micros();
        FastLED.show();
        FastLED.wait(kTimeoutMs);
        const uint32_t dt = micros() - t0;
        per_iter_us[iter] = dt;
        if (dt > kTimeoutMs * 1000u) {
            esp_rom_printf("BENCH_PARLIO_STREAM_VALIDATE FAIL  "
                           "iter=%d show_wait_us=%u (timeout=%ums)\n",
                           iter, static_cast<unsigned>(dt),
                           static_cast<unsigned>(kTimeoutMs));
            ok = false;
            break;
        }
    }

    FastLED.clear(ClearFlags::CHANNELS);

    if (ok) {
        // Steady-state average across iters 1..N-1 (skip iter 0 setup overhead).
        uint32_t steady_total = 0;
        for (int i = 1; i < kIterations; ++i) steady_total += per_iter_us[i];
        const uint32_t steady_avg = steady_total / (kIterations - 1);
        esp_rom_printf("BENCH_PARLIO_STREAM_VALIDATE PASS  lanes=%d leds=%d "
                       "iters=%d iter0=%u iter1=%u iter2=%u iter3=%u iter4=%u "
                       "steady_avg_us=%u (WS2812B 16-lane TX=7680us)\n",
                       kNumLanes, kNumLEDs, kIterations,
                       static_cast<unsigned>(per_iter_us[0]),
                       static_cast<unsigned>(per_iter_us[1]),
                       static_cast<unsigned>(per_iter_us[2]),
                       static_cast<unsigned>(per_iter_us[3]),
                       static_cast<unsigned>(per_iter_us[4]),
                       static_cast<unsigned>(steady_avg));
    }
    esp_rom_printf("BENCH_PARLIO_STREAM_VALIDATE_END\n\n");
#else
    (void)base_tx_pin;
#endif
}

}  // namespace parlio_stream
}  // namespace autoresearch
