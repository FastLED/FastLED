/// @file AutoResearchTimingDrift.h
/// @brief Issue #2994 repro: compounded per-sequence timing drift vs 3.10.3.
///
/// Replays the reporter's minimal sketch as faithfully as possible:
///   - 35-LED WS2812B ring on the legacy template addLeds path
///     (WS2812B<PIN> -> ClocklessControllerImpl -> ClocklessIdf5 -> Channel)
///     via LegacyClocklessProxy.
///   - FastLED.setMaxRefreshRate(800).
///   - millisDelay-gated 255-step fade: first step after 225ms, then 5ms
///     cadence; FastLED.show() throttled to a 5ms cadence; delay(1000)
///     between sequences.
///   - Records millis() elapsed per completed sequence — the value the
///     reporter printed over serial. Theoretical period:
///       225 + 254*5 + 1000 = 2495ms.
///     On 3.10.3 the reporter sees a rock-steady 2495; on master 2563-2752.
///
/// Deliberate deviations from the sketch (documented for reviewers):
///   - No setCpuFrequencyMhz(160): changing CPU clock mid-session risks the
///     RPC serial link; the current CPU MHz is reported instead.
///   - No .setCorrection(TypicalLEDStrip): color correction only scales
///     pixel values and does not affect pacing.
///   - The first recorded sequence is partial (no leading 225ms arm or
///     1000ms delay); the host skips index 0 when building the histogram.
///
/// Invoked via the `timingDriftTest` JSON-RPC handler registered in
/// AutoResearchRemote.cpp. NOTE: blocks ~2.5s per iteration.

#pragma once

#include "FastLED.h"
#include "fl/stl/int.h"
#include "LegacyClocklessProxy.h"

#include <Arduino.h>  // millis(), micros(), delay()

namespace autoresearch {
namespace timing_drift {

constexpr int kMaxIterations = 64;
constexpr int kMaxLeds = 256;

/// Faithful emulation of the millisDelay library used by the issue sketch:
/// start(t) arms a deadline; justFinished() fires true exactly once when
/// elapsed >= t; stop() disarms.
struct MillisDelay {
    uint32_t mStart = 0;
    uint32_t mDelay = 0;
    bool mRunning = false;

    void start(uint32_t delay_ms) {
        mDelay = delay_ms;
        mStart = millis();
        mRunning = true;
    }
    void stop() { mRunning = false; }
    bool justFinished() {
        if (mRunning && (millis() - mStart) >= mDelay) {
            mRunning = false;
            return true;
        }
        return false;
    }
};

struct DriftResult {
    bool valid_pin;                    // false if pin outside LegacyClocklessProxy range (0-8)
    int pin;
    int num_leds;
    int iterations;                    // completed sequence count
    uint32_t cpu_mhz;                  // CPU frequency during the run (0 if unknown)
    uint32_t iter_ms[kMaxIterations];  // per-sequence wall time; index 0 is partial
    uint32_t show_count;               // FastLED.show() calls inside the fade loop
    uint32_t show_min_us;
    uint32_t show_max_us;
    uint64_t show_total_us;
};

inline DriftResult run(int pin, int num_leds, int iterations) {
    DriftResult r{};
    r.pin = pin;

    if (iterations < 1) iterations = 1;
    if (iterations > kMaxIterations) iterations = kMaxIterations;
    if (num_leds < 2) num_leds = 2;
    if (num_leds > kMaxLeds) num_leds = kMaxLeds;
    r.num_leds = num_leds;

    static CRGB leds[kMaxLeds];
    for (int i = 0; i < num_leds; ++i) {
        leds[i] = CRGB::Black;
    }

    // Probe whether the requested pin is dispatchable BEFORE mutating any
    // global state — invalid pins must not clear other RPC tests' channels.
    {
        LegacyClocklessProxy probe(pin, leds, num_leds);
        if (!probe.valid()) {
            return r;  // valid_pin stays false; no side effects
        }
    }

    // Clean slate: drop any channels left over from earlier RPC tests so
    // show() drives only the legacy controller under test.
    FastLED.clear(ClearFlags::CHANNELS);

    LegacyClocklessProxy proxy(pin, leds, num_leds);
    r.valid_pin = true;

#if defined(ARDUINO_ARCH_ESP32)
    r.cpu_mhz = getCpuFrequencyMhz();
#endif

    // sketch setup() — addLeds happened in the proxy above.
    FastLED.setMaxRefreshRate(800);
    FastLED.show();

    constexpr uint32_t kFastLedUpdateMs = 5;
    MillisDelay ms_fast_led;
    MillisDelay ms_delay_post;

    ms_delay_post.start(0);
    ms_fast_led.start(kFastLedUpdateMs);

    int i_post_fade = 255;
    uint32_t i_iteration_time = millis();
    int completed = 0;

    r.show_min_us = 0xFFFFFFFFu;

    auto timedShow = [&]() {
        const uint32_t t0 = micros();
        FastLED.show();
        const uint32_t dt = micros() - t0;
        r.show_count++;
        r.show_total_us += dt;
        if (dt < r.show_min_us) r.show_min_us = dt;
        if (dt > r.show_max_us) r.show_max_us = dt;
    };

    auto updateLEDs = [&]() {
        if (ms_fast_led.justFinished()) {
            timedShow();
            ms_fast_led.start(kFastLedUpdateMs);
        }
    };

    const int i_ring_start = 0;
    const int i_ring_end = num_leds - 1;
    const int i_ring_divisor = 7;

    // Each sequence runs ~2.5 s and we may queue up to kMaxIterations of
    // them, so feed the AutoResearch watchdog (5 s timeout) on a coarse
    // cadence to keep the device alive through long runs.
    uint32_t last_wdt_feed_ms = millis();
    constexpr uint32_t kWdtFeedIntervalMs = 1000;
    auto maybeFeedWatchdog = [&]() {
        const uint32_t now = millis();
        if (now - last_wdt_feed_ms >= kWdtFeedIntervalMs) {
            FastLED.watchdog().feed();
            last_wdt_feed_ms = now;
        }
    };

    // Transliteration of the sketch's loop(); `continue` stands in for the
    // sketch's early `return` (Arduino re-enters loop()).
    while (completed < iterations) {
        if (i_post_fade > 0 && ms_delay_post.justFinished()) {
            const int i_ring_counter =
                ((255 - i_post_fade) / i_ring_divisor) + i_ring_start;

            if (i_ring_counter <= i_ring_end) {
                leds[i_ring_counter] = CRGB::Red;
            }
            if (i_ring_counter - 1 >= i_ring_start &&
                i_ring_counter - 1 <= i_ring_end) {
                leds[i_ring_counter - 1] = CRGB::Black;
            }

            i_post_fade--;

            if (i_post_fade == 0) {
                const uint32_t now = millis();
                r.iter_ms[completed] = now - i_iteration_time;
                completed++;
                i_iteration_time = now;
                ms_delay_post.stop();

                for (int i = i_ring_start; i <= i_ring_end; i++) {
                    leds[i] = CRGB::Black;
                }

                updateLEDs();
                FastLED.watchdog().feed();  // before the 1 s sleep
                delay(1000);
                FastLED.watchdog().feed();  // after the 1 s sleep
                last_wdt_feed_ms = millis();
                i_post_fade = 255;
                ms_delay_post.start(225);
                continue;
            } else {
                ms_delay_post.start(5);
            }
        }
        updateLEDs();
        maybeFeedWatchdog();
    }

    r.iterations = completed;
    if (r.show_count == 0) {
        r.show_min_us = 0;
    }

    // Teardown: restore unthrottled refresh (boot default). The proxy
    // destructor removes the legacy controller from the draw list.
    FastLED.setMaxRefreshRate(0);
    return r;
}

}  // namespace timing_drift
}  // namespace autoresearch
