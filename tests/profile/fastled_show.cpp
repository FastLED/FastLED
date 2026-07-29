// ok standalone
// FastLED.show() per-frame CPU cost profile
//
// Built for FastLED#3765 / #2994. The reporter of #2994 sees 68-257 ms of
// drift accumulated over 254 fade steps -- 268-1012 us per step -- against a
// theoretical 2495 ms sequence that 3.10.3 hit exactly. That per-step excess
// is the same order of magnitude as one WS2812 show() for their 35-LED strip
// (~1050 us of wire time), which points at show()'s DURATION rather than at
// the refresh-rate throttle (the throttle provably never engages in their
// sketch: show() is gated at 5 ms, far above the 1250 us threshold).
//
// WHAT THIS DOES AND DOES NOT COVER -- read before drawing conclusions.
//
// Covered: the CPU-side per-frame path visible on host -- EngineEvents, the
// three-pass controller walk in CFastLED::show(), dithering, scaling, and the
// channels dispatch introduced by #2428.
//
// NOT covered: wire time, DMA, and -- importantly -- per-frame encoding and
// bit transposition, because the host driver is a stub. A regression living
// in the encoder or in a driver wait loop will NOT show up here.
//
// First measurement on a desktop host: ~1.7 us/frame for 35 LEDs. That is far
// below #2994's 268-1012 us of excess, which is itself useful evidence -- the
// regression is almost certainly NOT in the CPU-side dispatch path -- but it
// does not localize where the regression IS.
//
// Usage:
//   ./fastled_show                 # human-readable
//   ./fastled_show baseline        # JSON for the profiling pipeline
//   bash profile fastled_show --iterations 50

#include "FastLED.h"
#include "fl/stl/int.h"
#include "fl/stl/stdio.h"
#include "fl/stl/chrono.h"
#include "profile_result.h"

namespace fl {

namespace {

constexpr int kDataPin = 2;

/// One timing sample set for a strip.
struct ShowStats {
    fl::u32 min_us;
    fl::u32 max_us;
    fl::u32 mean_us;
    fl::u32 total_us;
    int frames;
};

/// Time `frames` successive FastLED.show() calls.
///
/// Reports the MINIMUM as the headline number, not the mean: this measures a
/// code path, and on a loaded desktop the mean is dominated by scheduler
/// noise the firmware would never see. The minimum is the closest stable
/// estimate of the actual work, and a regression raises it just as reliably.
ShowStats timeShow(int frames) {
    ShowStats stats;
    stats.min_us = 0xFFFFFFFFu;
    stats.max_us = 0;
    stats.total_us = 0;
    stats.frames = frames;

    for (int i = 0; i < frames; ++i) {
        const fl::u32 t0 = fl::micros();
        FastLED.show();
        const fl::u32 dt = fl::micros() - t0;
        if (dt < stats.min_us) {
            stats.min_us = dt;
        }
        if (dt > stats.max_us) {
            stats.max_us = dt;
        }
        stats.total_us += dt;
    }
    stats.mean_us = frames > 0 ? stats.total_us / static_cast<fl::u32>(frames) : 0;
    return stats;
}

} // namespace

} // namespace fl

int main(int argc, char** argv) {
    const bool json_mode = (argc > 1);
    const char* variant = json_mode ? argv[1] : "baseline";

    // #2994's exact geometry: 35-LED WS2812 ring on the legacy addLeds path.
    static CRGB leds35[35];
    FastLED.addLeds<WS2812, fl::kDataPin, GRB>(leds35, 35);

    // Disable the refresh throttle so it cannot contaminate the measurement.
    // The whole point of #3765 is that the throttle is NOT the cause; leaving
    // it on would sometimes add a wait and muddy the per-frame number.
    FastLED.setMaxRefreshRate(0);

    for (int i = 0; i < 35; ++i) {
        leds35[i] = CRGB(i * 7, 255 - i * 7, 128);
    }

    // Warm up: the first shows pay lazy driver binding and any one-time
    // allocation, which is not what we are measuring.
    for (int i = 0; i < 32; ++i) {
        FastLED.show();
    }

    const int kFrames = 512;
    const fl::ShowStats s35 = fl::timeShow(kFrames);

    if (json_mode) {
        // parse_results.py builds a fixed ProfileResult dataclass, so extra
        // keys make it raise. Stick to the standard emitter.
        ProfileResultBuilder::print_result(variant, "fastled_show", kFrames,
                                           s35.total_us);
    } else {
        fl::printf("FastLED.show() CPU-side cost, 35-LED WS2812 (#2994 geometry)\n");
        fl::printf("  frames   : %d\n", kFrames);
        fl::printf("  min      : %u us   <- headline; least scheduler noise\n",
                   static_cast<unsigned>(s35.min_us));
        fl::printf("  mean     : %u us\n", static_cast<unsigned>(s35.mean_us));
        fl::printf("  max      : %u us\n", static_cast<unsigned>(s35.max_us));
        fl::printf("\n");
        fl::printf("Reference: #2994 reports 268-1012 us of excess per fade step.\n");
        fl::printf("Encoding/transposition and wire time are NOT included (host stub).\n");
    }

    return 0;
}
