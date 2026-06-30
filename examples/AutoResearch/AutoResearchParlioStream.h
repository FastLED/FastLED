/// @file AutoResearchParlioStream.h
/// @brief PARLIO ISR-streaming functional validation (#2548 deep-dive follow-up).
///
/// Validates the engine's ISR-chunked streaming path on real hardware:
///   - Initializes PARLIO with **16 lanes** (forces the engine's 16-lane Wave8
///     branch, which dispatches to `wave8Transpose_16x4_bf1_pipe4` shipped in
///     #2559).
///   - Fills with a known pattern.
///   - Calls FastLED.show() — which goes through ParlioEngine's 3-buffer ring
///     + txDoneCallback ISR streaming + BF1 encode.
///   - Measures wall-clock time for show()+wait() to complete across N back-
///     to-back frames.
///
/// **Pass criterion**: every show()+wait() completes within `timeoutMs`.
/// Catches hangs / stalls in the streaming engine.
///
/// Does NOT validate transmitted byte correctness — that's covered by the host
/// bit-exactness test in tests/fl/channels/wave8.cpp added in #2559, and by
/// the RX-loopback path in the existing autoresearch CLI when run with a
/// jumper wire.
///
/// Invoked via the `parlioStreamValidate` JSON-RPC handler registered in
/// AutoResearchRemote.cpp.

#pragma once

#include "FastLED.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/chipsets/led_timing.h"
#include "fl/stl/int.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"

#if defined(ESP32) && FASTLED_ESP32_HAS_PARLIO
#include "platforms/esp/32/drivers/parlio/parlio_engine.h"  // ok platform headers - AutoResearch is the canonical ESP32 PARLIO live-driver scratch sketch and needs the deep driver header for streaming diagnostics
#endif

#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
#include <Arduino.h>  // micros()
#endif

namespace autoresearch {
namespace parlio_stream {

constexpr int kMaxIterations = 16;  // result struct iter timing array fixed size
constexpr int kMaxLanes = 16;

inline bool isFastLedOutputPinValid(int pin) {
    if (pin < 0 || pin >= 64) {
        return false;
    }
#if defined(_FL_VALID_PIN_MASK)
    return (_FL_VALID_PIN_MASK & (1ULL << pin)) != 0;
#else
    return true;
#endif
}

struct ValidateResult {
    bool channels_ok;          // true if all PARLIO channels created cleanly
    bool completed;            // true if every iteration completed within timeout
    int base_tx_pin;           // first PARLIO TX pin used
    bool explicit_tx_pins;     // true when caller supplied a non-contiguous pin map
    int lanes;                 // PARLIO lane count tested
    int leds_per_lane;         // LEDs per lane
    int tx_pins[kMaxLanes];    // effective pin per lane
    int iterations;            // number of show() iterations run
    uint32_t per_iter_us[kMaxIterations];  // per-iteration show()+wait() total time
    uint32_t per_iter_show_us[kMaxIterations];  // per-iter show() return time (pre-encode + submit)
    uint32_t per_iter_wait_us[kMaxIterations];  // per-iter wait() time (TX completion)
    uint32_t steady_avg_us;        // average total of iters 1..N-1 (skips iter 0 setup)
    uint32_t steady_avg_show_us;   // average show() time of iters 1..N-1
    uint32_t steady_avg_wait_us;   // average wait() time of iters 1..N-1
    uint32_t tx_done_count;        // PARLIO txDone ISR calls from final metrics
    uint32_t worker_isr_count;     // PARLIO worker ISR calls from final metrics
    uint32_t underrun_count;       // ring-empty-with-bytes-pending events
    uint32_t ring_count;           // final PARLIO ring occupancy
    uint32_t bytes_total;          // final engine total source bytes
    uint32_t bytes_transmitted;    // final engine transmitted source bytes
    int failed_iter;           // index of first iter that exceeded timeout, or -1
    uint32_t timeout_ms;       // effective timeout passed in
    bool ring_error;           // true if PARLIO flagged ring accounting/buffer error
    bool hardware_idle;        // true if hardware went idle before stream completed
};

/// @brief Run the PARLIO streaming functional test.
///
/// @param base_tx_pin  First PARLIO TX pin; lanes occupy [base_tx_pin .. base_tx_pin+num_lanes-1]
/// @param num_lanes    Number of PARLIO lanes (1-16). Use 16 to exercise BF1+pipe4.
/// @param num_leds     LEDs per lane.
/// @param iterations   Number of back-to-back show()+wait() cycles (capped at kMaxIterations).
/// @param timeout_ms   Per-iteration timeout. Test fails if any iter exceeds this.
/// @param tx_pins      Optional explicit per-lane pins. If null, uses contiguous base_tx_pin+lane.
inline ValidateResult validateParlioStreaming(int base_tx_pin,
                                              int num_lanes,
                                              int num_leds,
                                              int iterations,
                                              uint32_t timeout_ms,
                                              const int* tx_pins = nullptr) {
    ValidateResult r{};
    r.channels_ok = false;
    r.completed = false;
    r.explicit_tx_pins = tx_pins != nullptr;
    r.base_tx_pin = base_tx_pin;
    r.lanes = num_lanes;
    r.leds_per_lane = num_leds;
    r.iterations = iterations;
    r.timeout_ms = timeout_ms;
    r.failed_iter = -1;
    for (int lane = 0; lane < kMaxLanes; ++lane) {
        r.tx_pins[lane] = -1;
    }

    if (iterations < 1) iterations = 1;
    if (iterations > kMaxIterations) iterations = kMaxIterations;
    r.iterations = iterations;
    if (!tx_pins && base_tx_pin < 0) return r;
    if (num_lanes < 1 || num_lanes > 16) return r;
    if (num_leds < 1) return r;
    if (tx_pins) {
        base_tx_pin = tx_pins[0];
        r.base_tx_pin = base_tx_pin;
        for (int lane = 0; lane < num_lanes; ++lane) {
            if (tx_pins[lane] < 0) return r;
        }
    }

#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
    // Reuse a single static heap-resident LED buffer sized for the canonical
    // worst case (16 lanes × 256 LEDs). Test callers must stay within these
    // bounds. Static to avoid repeated allocation across RPC calls.
    constexpr int kMaxLEDs = 256;
    if (num_leds > kMaxLEDs) return r;
    static CRGB leds[kMaxLanes][kMaxLEDs];
    for (int lane = 0; lane < num_lanes; ++lane) {
        for (int i = 0; i < num_leds; ++i) {
            leds[lane][i] = CRGB(0xF0, 0x0F, 0xAA);  // Pattern A (mixed bits)
        }
    }

    fl::ChipsetTimingConfig timing =
        fl::makeTimingConfig<fl::TIMING_WS2812B_V5>();

    fl::ChannelOptions parlio_opts;
    parlio_opts.mBus = fl::Bus::FLEX_IO;
    parlio_opts.mBusWhich = 0;

    FastLED.clear(ClearFlags::CHANNELS);

    fl::vector<fl::shared_ptr<fl::Channel>> channels;
    for (int lane = 0; lane < num_lanes; ++lane) {
        const int tx_pin = tx_pins ? tx_pins[lane] : (base_tx_pin + lane);
        r.tx_pins[lane] = tx_pin;
        if (!isFastLedOutputPinValid(tx_pin)) {
            FastLED.clear(ClearFlags::CHANNELS);
            return r;
        }
        fl::ChannelConfig cfg(
            tx_pin,
            timing,
            fl::span<CRGB>(leds[lane], num_leds),
            RGB,
            parlio_opts);
        auto ch = FastLED.add(cfg);
        if (!ch) {
            FastLED.clear(ClearFlags::CHANNELS);
            return r;
        }
        channels.push_back(ch);
    }
    r.channels_ok = true;

    bool ok = true;
    uint32_t steady_total = 0;
    uint32_t steady_show_total = 0;
    uint32_t steady_wait_total = 0;
    for (int iter = 0; iter < iterations; ++iter) {
        const uint32_t t0 = micros();
        FastLED.show();
        const uint32_t t_show = micros();
        FastLED.wait(timeout_ms);
        const uint32_t t1 = micros();
        const uint32_t show_us = t_show - t0;
        const uint32_t wait_us = t1 - t_show;
        const uint32_t dt = t1 - t0;
        r.per_iter_us[iter] = dt;
        r.per_iter_show_us[iter] = show_us;
        r.per_iter_wait_us[iter] = wait_us;
        if (dt > timeout_ms * 1000u) {
            r.failed_iter = iter;
            ok = false;
            break;
        }
        if (iter > 0) {
            steady_total += dt;
            steady_show_total += show_us;
            steady_wait_total += wait_us;
        }
    }

    FastLED.clear(ClearFlags::CHANNELS);

#if defined(ESP32) && FASTLED_ESP32_HAS_PARLIO
    {
        auto metrics = fl::detail::ParlioEngine::getInstance().getDebugMetrics();
        r.tx_done_count = metrics.mTxDoneCount;
        r.worker_isr_count = metrics.mWorkerIsrCount;
        r.underrun_count = metrics.mUnderrunCount;
        r.ring_count = metrics.mRingCount;
        r.bytes_total = metrics.mBytesTotal;
        r.bytes_transmitted = metrics.mBytesTransmitted;
        r.ring_error = metrics.mRingError;
        r.hardware_idle = metrics.mHardwareIdle;
    }
#endif

    r.completed = ok;
    if (ok && iterations > 1) {
        r.steady_avg_us = steady_total / (iterations - 1);
        r.steady_avg_show_us = steady_show_total / (iterations - 1);
        r.steady_avg_wait_us = steady_wait_total / (iterations - 1);
    } else if (ok) {
        r.steady_avg_us = r.per_iter_us[0];
        r.steady_avg_show_us = r.per_iter_show_us[0];
        r.steady_avg_wait_us = r.per_iter_wait_us[0];
    }
#else
    (void)base_tx_pin;
    (void)timeout_ms;
#endif
    return r;
}

}  // namespace parlio_stream
}  // namespace autoresearch
