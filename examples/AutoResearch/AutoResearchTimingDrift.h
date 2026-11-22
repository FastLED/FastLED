/// @file AutoResearchTimingDrift.h
/// @brief Issue #2994 repro: compounded per-sequence timing drift vs 3.10.3.
///
/// Replays the reporter's minimal sketch as faithfully as possible:
///   - 35-LED WS2812B ring on the legacy template addLeds path
///     (WS2812B<PIN> -> ClocklessControllerImpl -> ClocklessIdf5 ->
///     SlimBridgeController -> RMT5 driver) via LegacyClocklessProxy.
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
#include "fl/channels/manager.h"
#include "fl/stl/int.h"
#include "fl/stl/singleton.h"
#include "fl/system/instruction_counter.h"
#include "LegacyClocklessProxy.h"
#include "platforms/is_platform.h"

#include <Arduino.h>  // millis(), micros(), delay()

// Cold-vs-warm mode (#5422): the ROM call that empties the flash cache. On the
// C3/C6/H2 the one L1 cache serves both code and rodata fetched from flash.
#if defined(FL_IS_ESP_32C6)
#include "esp32c6/rom/cache.h"  // ok platform headers - AutoResearch #5422 cache eviction probe
#define FL_TIMING_DRIFT_HAS_CACHE_FLUSH 1
#elif defined(FL_IS_ESP_32C3)
#include "esp32c3/rom/cache.h"  // ok platform headers - AutoResearch #5422 cache eviction probe
#define FL_TIMING_DRIFT_HAS_CACHE_FLUSH 1
#elif defined(FL_IS_ESP_32H2)
#include "esp32h2/rom/cache.h"  // ok platform headers - AutoResearch #5422 cache eviction probe
#define FL_TIMING_DRIFT_HAS_CACHE_FLUSH 1
#else
#define FL_TIMING_DRIFT_HAS_CACHE_FLUSH 0
#endif

namespace autoresearch {
namespace timing_drift {

constexpr int kMaxIterations = 64;
constexpr int kMaxLeds = 256;

struct TimingDriftState {
    CRGB leds[kMaxLeds];
};

inline TimingDriftState& timingDriftState() {
    return fl::SingletonShared<TimingDriftState>::instance();
}

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
    bool valid_pin;                    // false if pin outside LegacyClocklessProxy range
    int pin;
    int num_leds;
    int iterations;                    // completed sequence count
    uint32_t cpu_mhz;                  // CPU frequency during the run (0 if unknown)
    uint32_t iter_ms[kMaxIterations];  // per-sequence wall time; index 0 is partial
    uint32_t show_count;               // FastLED.show() calls inside the fade loop
    uint32_t show_min_us;
    uint32_t show_max_us;
    uint32_t show_draining_count;
    uint32_t show_ready_count;
    uint32_t show_cycle_count;         // real cycles, in counter-off runs only
    uint64_t show_cycle_total;
    uint64_t show_total_us;
    uint32_t show_instr_count;         // show() calls with an instruction count (#5376)
    uint64_t show_instr_total;         // retired instructions across those calls
};

/// Cold-vs-warm show() pairs (#5422). Each pair: wait for the driver to go
/// READY, empty the flash cache, time show() (cold), wait for READY again,
/// time show() (warm). Counters follow the same split as DriftResult: either
/// instructions (countInstructions) or real cycles, never both in one run.
struct ColdWarmSide {
    uint32_t count;
    uint64_t total_us;
    uint64_t instr_total;   // countInstructions runs only
    uint64_t cycle_total;   // counter-off runs only
    uint32_t min_us;
    uint32_t max_us;
};

struct ColdWarmResult {
    bool valid_pin;
    bool supported;          // false where no cache flush exists (cold == warm)
    int pin;
    int num_leds;
    int repeats;
    uint32_t cpu_mhz;
    uint32_t ready_timeouts; // READY waits that gave up (should be 0)
    ColdWarmSide cold;
    ColdWarmSide warm;
};

// Instructions per show() come from fl::InstructionCounter (#5376): wall
// time moves 10-20 us with code layout alone; the instruction count does not.


/// The reporter's fade loop over whichever strip the caller registered,
/// timing every show(). Restores the unthrottled refresh rate on exit.
inline void measureShows(DriftResult& r, CRGB* leds, int num_leds, int iterations, bool countInstructions) {
#if defined(ARDUINO_ARCH_ESP32)
    r.cpu_mhz = getCpuFrequencyMhz();
#endif

    // sketch setup() — the caller registered the strip.
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
        fl::InstructionCounter counter;
        if (countInstructions) counter.start();
        // The C3/C6/H2 share the instruction and cycle counter. Read real
        // cycles only when start() has not repurposed it for instructions.
        const uint32_t cycleStart = countInstructions ? 0 : fl::InstructionCounter::readCycles();
        const uint32_t t0 = micros();
        FastLED.show();
        const uint32_t dt = micros() - t0;
        const uint32_t measuredCycles = countInstructions ? 0 : fl::InstructionCounter::readCycles() - cycleStart;
        const uint32_t instructions = countInstructions ? counter.stop(dt * r.cpu_mhz) : 0;
        if (countInstructions && fl::InstructionCounter::kSupported) {
            r.show_instr_total += instructions;
            r.show_instr_count++;
        }
        if (!countInstructions && fl::InstructionCounter::kSupported) {
            r.show_cycle_total += measuredCycles;
            r.show_cycle_count++;
        }
        const auto state = fl::ChannelManager::instance().poll();
        if (state == fl::IChannelDriver::DriverState::DRAINING) r.show_draining_count++;
        if (state == fl::IChannelDriver::DriverState::READY) r.show_ready_count++;
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

    // Teardown: restore unthrottled refresh (boot default).
    FastLED.setMaxRefreshRate(0);
}

/// Empty the flash cache so the next show() fetches its code and rodata from
/// flash. Interrupts stay off for the ROM call so no handler fetches through
/// a half-invalidated cache.
inline bool evictFlashCache() {
#if FL_TIMING_DRIFT_HAS_CACHE_FLUSH
    portDISABLE_INTERRUPTS();
    Cache_Invalidate_ICache_All();
    portENABLE_INTERRUPTS();
    return true;
#else
    return false;
#endif
}

/// Poll until every channel driver reports READY, so the next show() never
/// blocks on the previous frame's wire time. Returns false on timeout.
inline bool waitForDriversReady() {
    const uint32_t t0 = millis();
    while (fl::ChannelManager::instance().poll() != fl::IChannelDriver::DriverState::READY) {
        if (millis() - t0 > 50) return false;
    }
    return true;
}

inline void timeOneShow(ColdWarmSide& side, uint32_t cpu_mhz, bool countInstructions) {
    fl::InstructionCounter counter;
    if (countInstructions) counter.start();
    const uint32_t cycleStart = countInstructions ? 0 : fl::InstructionCounter::readCycles();
    const uint32_t t0 = micros();
    FastLED.show();
    const uint32_t dt = micros() - t0;
    const uint32_t cycles = countInstructions ? 0 : fl::InstructionCounter::readCycles() - cycleStart;
    const uint32_t instructions = countInstructions ? counter.stop(dt * cpu_mhz) : 0;
    side.count++;
    side.total_us += dt;
    side.instr_total += instructions;
    side.cycle_total += cycles;
    if (dt < side.min_us) side.min_us = dt;
    if (dt > side.max_us) side.max_us = dt;
}

/// The cold-vs-warm loop over whichever strip the caller registered.
inline void measureColdWarm(ColdWarmResult& r, CRGB* leds, int num_leds, int repeats, bool countInstructions) {
#if defined(ARDUINO_ARCH_ESP32)
    r.cpu_mhz = getCpuFrequencyMhz();
#endif
    r.supported = FL_TIMING_DRIFT_HAS_CACHE_FLUSH != 0;
    // No refresh-rate throttle: it would add a sleep to back-to-back shows.
    FastLED.setMaxRefreshRate(0);
    for (int i = 0; i < num_leds; ++i) {
        leds[i] = CRGB(static_cast<uint8_t>(i * 7), static_cast<uint8_t>(i * 3), static_cast<uint8_t>(255 - i));
    }
    FastLED.show();  // first show allocates; keep it out of the samples
    r.cold.min_us = r.warm.min_us = 0xFFFFFFFFu;
    for (int i = 0; i < repeats; ++i) {
        if (!waitForDriversReady()) r.ready_timeouts++;
        evictFlashCache();
        timeOneShow(r.cold, r.cpu_mhz, countInstructions);
        if (!waitForDriversReady()) r.ready_timeouts++;
        timeOneShow(r.warm, r.cpu_mhz, countInstructions);
        FastLED.watchdog().feed();
    }
    waitForDriversReady();
    r.repeats = repeats;
}

/// @param rgbw    legacy path only: the strip as SK6812-style RGBW (#5377).
/// @param channel the same strip through the channel API (`FastLED.add()`)
///                instead of the legacy `addLeds` template path (#5399).
inline DriftResult run(int pin, int num_leds, int iterations, bool rgbw = false,
                       bool channel = false, bool countInstructions = true) {
    DriftResult r{};
    r.pin = pin;

    if (iterations < 1) iterations = 1;
    if (iterations > kMaxIterations) iterations = kMaxIterations;
    if (num_leds < 2) num_leds = 2;
    if (num_leds > kMaxLeds) num_leds = kMaxLeds;
    r.num_leds = num_leds;

    CRGB* leds = timingDriftState().leds;
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
    // show() drives only the strip under test.
    FastLED.clear(ClearFlags::CHANNELS);

    if (channel) {
        // The legacy WS2812B<PIN> strip (800 kHz timing, GRB) registered
        // through the channel API, which encodes via PixelIterator (#5399).
        fl::ChannelConfig config(fl::makeClockless<fl::TIMING_WS2812_800KHZ>(pin),
                                 fl::span<CRGB>(leds, num_leds), GRB);
        auto ch = FastLED.add(config);
        if (!ch) {
            return r;  // valid_pin stays false
        }
        r.valid_pin = true;
        measureShows(r, leds, num_leds, iterations, countInstructions);
        FastLED.remove(ch);
        return r;
    }

    // The proxy destructor removes the legacy controller from the draw list.
    LegacyClocklessProxy proxy(pin, leds, num_leds, LegacyClocklessChipset::WS2812B, rgbw);
    r.valid_pin = true;
    measureShows(r, leds, num_leds, iterations, countInstructions);
    return r;
}

/// Cold-vs-warm show() on the legacy addLeds strip, or the channel API (#5422).
/// @param channelOrder channel API only. GRB matches the legacy strip; RGB is
///        the identity order, which skips PixelIterator's per-run reorder, so
///        the instruction difference between the two is that pass (#5378).
inline ColdWarmResult runColdWarm(int pin, int num_leds, int repeats, bool channel, bool countInstructions,
                                  EOrder channelOrder = GRB) {
    ColdWarmResult r{};
    r.pin = pin;
    if (repeats < 1) repeats = 1;
    if (repeats > 1000) repeats = 1000;
    if (num_leds < 2) num_leds = 2;
    if (num_leds > kMaxLeds) num_leds = kMaxLeds;
    r.num_leds = num_leds;

    CRGB* leds = timingDriftState().leds;
    {
        LegacyClocklessProxy probe(pin, leds, num_leds);
        if (!probe.valid()) {
            return r;
        }
    }
    FastLED.clear(ClearFlags::CHANNELS);

    if (channel) {
        fl::ChannelConfig config(fl::makeClockless<fl::TIMING_WS2812_800KHZ>(pin),
                                 fl::span<CRGB>(leds, num_leds), channelOrder);
        auto ch = FastLED.add(config);
        if (!ch) {
            return r;
        }
        r.valid_pin = true;
        measureColdWarm(r, leds, num_leds, repeats, countInstructions);
        FastLED.remove(ch);
        return r;
    }

    LegacyClocklessProxy proxy(pin, leds, num_leds, LegacyClocklessChipset::WS2812B, false);
    r.valid_pin = true;
    measureColdWarm(r, leds, num_leds, repeats, countInstructions);
    return r;
}

}  // namespace timing_drift
}  // namespace autoresearch
