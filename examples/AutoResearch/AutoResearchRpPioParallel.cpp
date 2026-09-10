// FastLED#3899: "PIO0 + PIO1 simultaneous execution passes without resource
// collision, stale state machine, or DMA ownership leak."
//
// runParallelTest drives both blocks at once, but for the PIO0+PIO1 pair it
// deliberately skips RX validation (AutoResearchRemoteRunParallelTest.cpp) and
// makes no resource assertions at all. A PASS there therefore proves only that
// show() completed. Each of the three failure modes named in the criterion is
// invisible to it.
//
// This asks the SDK claim system directly, which is ground truth regardless of
// how the channel engine is implemented:
//
//   collision      -> the two engines must never hold overlapping resources,
//                     and must never take more than one state machine each.
//   stale SM       -> between frames the engine must be back to baseline; a
//                     state machine kept or abandoned across a frame boundary
//                     shows up as residue.
//   ownership leak -> after teardown every count must return to baseline.
//
// Note ChannelEngineRpPio claims per transmission and calls deinitialize()
// when the transfer drains, so the interesting sample is *between*
// FastLED.show() and FastLED.wait(), not after. Sampling after the wait sees
// an idle engine and proves nothing.
//
// What this does NOT assert: that PIO0 and PIO1 are ever claimed at the *same
// instant*. FastLED.show() drains the frame before returning on this path, so
// the observable in-flight window is roughly one sample per frame -- far too
// coarse to tell genuine serialization apart from an unlucky sample. The
// `pio0HeldOne`/`pio1HeldOne`/`bothHeldAtOnce` fields are reported as
// diagnostics for that open question and deliberately do not gate `success`.
// Claiming serialization on this evidence would be unfounded.

#include "AutoResearchRpPioParallel.h"

#include "platforms/arm/rp/is_rp.h"

#if defined(FL_IS_RP2040) || defined(FL_IS_RP2350)

#include <Arduino.h>

#include "FastLED.h"
#include "fl/channels/channel.h"
#include "fl/channels/config.h"
#include "fl/chipsets/timing_traits.h"
#include "fl/stl/vector.h"

// IWYU pragma: begin_keep
#include "hardware/dma.h"
#include "hardware/pio.h"
// IWYU pragma: end_keep

namespace {

constexpr int kDefaultPinA = 2;
constexpr int kDefaultPinB = 4;
constexpr int kDefaultLeds = 256;
constexpr int kDefaultIterations = 8;
// Two CRGB buffers come out of the RPC arguments. 2 * 2000 * 3 B is 12 KB,
// comfortable on an RP2350 and far beyond any useful test size.
constexpr int kMaxLeds = 2000;
// Each iteration is a full show() plus a bounded wait, so this keeps the probe
// well inside AutoResearch's 5 s loop watchdog.
constexpr int kMaxIterations = 256;
// AutoResearch's loop watchdog is 5 s; leave room for the JSON response.
constexpr uint32_t kShowTimeoutMs = 2000;

PIO pioForIndex(int index) {
    if (index == 0) return pio0;
    if (index == 1) return pio1;
#if defined(FL_IS_RP2350)
    if (index == 2) return pio2;
#endif
    return nullptr;
}

/// Free state machines in one PIO block.
int freeSmsInBlock(int pio_index) {
    PIO pio = pioForIndex(pio_index);
    if (pio == nullptr) return 0;
    int free_count = 0;
    for (uint sm = 0; sm < 4; ++sm) {
        if (!pio_sm_is_claimed(pio, sm)) ++free_count;
    }
    return free_count;
}

int freeDmaChannels() {
    int free_count = 0;
    for (uint ch = 0; ch < NUM_DMA_CHANNELS; ++ch) {
        if (!dma_channel_is_claimed(ch)) ++free_count;
    }
    return free_count;
}

/// A snapshot of every resource the two engines could contend for.
struct ResourceSnapshot {
    int free_sms[NUM_PIOS] = {};
    int free_dma = 0;

    void capture() {
        for (int i = 0; i < NUM_PIOS; ++i) free_sms[i] = freeSmsInBlock(i);
        free_dma = freeDmaChannels();
    }

    int totalFreeSms() const {
        int total = 0;
        for (int i = 0; i < NUM_PIOS; ++i) total += free_sms[i];
        return total;
    }

    bool operator==(const ResourceSnapshot& other) const {
        if (free_dma != other.free_dma) return false;
        for (int i = 0; i < NUM_PIOS; ++i) {
            if (free_sms[i] != other.free_sms[i]) return false;
        }
        return true;
    }

    fl::json toJson() const {
        fl::json out = fl::json::object();
        fl::json blocks = fl::json::array();
        for (int i = 0; i < NUM_PIOS; ++i) {
            blocks.push_back(static_cast<int64_t>(free_sms[i]));
        }
        out.set("freeSmsPerBlock", blocks);
        out.set("freeDma", static_cast<int64_t>(free_dma));
        return out;
    }
};

fl::ChannelPtr addPioChannel(int pio_which, int pin, fl::vector<CRGB>& leds) {
    fl::ChannelOptions opts;
    opts.mBus = fl::Bus::FLEX_IO;
    opts.mBusWhich = pio_which;
    fl::ChannelConfig config(pin, fl::makeTimingConfig<fl::TIMING_WS2812B_V5>(),
                             fl::span<CRGB>(leds), RGB,
                             opts);
    return FastLED.add(config);
}

} // namespace

namespace autoresearch {

fl::json runRpPioParallelResourceTest(const fl::json& args) {
    fl::json response = fl::json::object();

    int pin_a = kDefaultPinA;
    int pin_b = kDefaultPinB;
    int num_leds = kDefaultLeds;
    int iterations = kDefaultIterations;
    if (args.is_object()) {
        auto read_int = [&args](const char* key, int fallback) -> int {
            if (args.contains(key) && args[key].is_int()) {
                return static_cast<int>(args[key].as_int().value());
            }
            return fallback;
        };
        pin_a = read_int("pinA", pin_a);
        pin_b = read_int("pinB", pin_b);
        num_leds = read_int("numLeds", num_leds);
        iterations = read_int("iterations", iterations);
    }
    // Upper bounds matter more than the lower ones here: both LED buffers are
    // allocated from this RPC's arguments, and two unbounded CRGB vectors will
    // exhaust an RP2350's RAM long before anything reports a problem. Reject
    // rather than clamp, so a caller asking for something impossible learns
    // that instead of silently measuring a different run.
    if (num_leds < 1 || num_leds > kMaxLeds || iterations < 1 ||
        iterations > kMaxIterations) {
        response.set("success", false);
        response.set("error", "InvalidArgs");
        response.set("maxNumLeds", static_cast<int64_t>(kMaxLeds));
        response.set("maxIterations", static_cast<int64_t>(kMaxIterations));
        return response;
    }

    response.set("pinA", static_cast<int64_t>(pin_a));
    response.set("pinB", static_cast<int64_t>(pin_b));
    response.set("numLeds", static_cast<int64_t>(num_leds));
    response.set("iterations", static_cast<int64_t>(iterations));

    ResourceSnapshot baseline;
    baseline.capture();
    response.set("baseline", baseline.toJson());

    // Start from a known driver state so neither engine is picked by priority.
    FastLED.clear(ClearFlags::CHANNELS);
    FastLED.setDriverEnabled("PIO0", true);
    FastLED.setDriverEnabled("PIO1", true);

    fl::vector<CRGB> leds_a(num_leds);
    fl::vector<CRGB> leds_b(num_leds);

    bool collision_free = false;
    bool no_residue = true;
    bool shows_completed = false;
    {
        fl::ChannelPtr channel_a = addPioChannel(0, pin_a, leds_a);
        fl::ChannelPtr channel_b = addPioChannel(1, pin_b, leds_b);
        response.set("channelACreated", static_cast<bool>(channel_a));
        response.set("channelBCreated", static_cast<bool>(channel_b));

        if (!channel_a || !channel_b) {
            FastLED.clear(ClearFlags::CHANNELS);
            response.set("success", false);
            response.set("error", "ChannelCreationFailed");
            return response;
        }

        for (int i = 0; i < num_leds; ++i) {
            leds_a[i] = CRGB(0xFF, 0x00, 0x00);
            leds_b[i] = CRGB(0x00, 0xFF, 0x00);
        }

        // Best in-flight sample across all frames: the one where the most
        // resources were held at once.
        ResourceSnapshot busiest;
        busiest.capture();
        int fewest_free = busiest.totalFreeSms() + busiest.free_dma;
        int frames_sampled = 0;

        // Sample continuously for the whole transmission rather than once per
        // frame. A single sample cannot tell "the two engines are never
        // concurrent" apart from "the sample landed between them".
        int samples_taken = 0;
        shows_completed = true;
        for (int iter = 0; iter < iterations && shows_completed; ++iter) {
            FastLED.show();

            const u32 deadline = millis() + kShowTimeoutMs;
            bool done = false;
            while (!done && static_cast<i32>(millis() - deadline) < 0) {
                ResourceSnapshot in_flight;
                in_flight.capture();
                const int held = in_flight.totalFreeSms() + in_flight.free_dma;
                if (held < fewest_free) {
                    fewest_free = held;
                    busiest = in_flight;
                }
                ++samples_taken;
                // 1 ms slices so the loop keeps sampling while DMA drains.
                done = FastLED.wait(1);
            }
            shows_completed = done;
            ++frames_sampled;

            // Between frames the engine must have given everything back.
            ResourceSnapshot between;
            between.capture();
            if (!(between == baseline)) no_residue = false;
        }
        response.set("samplesTaken", static_cast<int64_t>(samples_taken));
        response.set("showsCompleted", shows_completed);
        response.set("framesSampled", static_cast<int64_t>(frames_sampled));
        response.set("busiestInFlight", busiest.toJson());
        response.set("noResidueBetweenFrames", no_residue);

        // Diagnostics for the open simultaneity question (see header comment).
        const bool pio0_held_one = busiest.free_sms[0] == baseline.free_sms[0] - 1;
        const bool pio1_held_one = busiest.free_sms[1] == baseline.free_sms[1] - 1;
        response.set("pio0HeldOne", pio0_held_one);
        response.set("pio1HeldOne", pio1_held_one);
        response.set("bothHeldAtOnce", pio0_held_one && pio1_held_one);

        // What is actually asserted: neither engine ever over-claims, and the
        // probe genuinely observed a transmission in flight (otherwise a clean
        // baseline would pass vacuously).
        bool no_over_claim = busiest.free_dma >= baseline.free_dma - 2;
        for (int i = 0; i < NUM_PIOS; ++i) {
            if (busiest.free_sms[i] < baseline.free_sms[i] - 1) no_over_claim = false;
        }
        const bool observed_in_flight =
            busiest.totalFreeSms() + busiest.free_dma <
            baseline.totalFreeSms() + baseline.free_dma;
        collision_free = no_over_claim && observed_in_flight;
        response.set("noOverClaim", no_over_claim);
        response.set("observedInFlight", observed_in_flight);
        response.set("collisionFree", collision_free);
    }

    FastLED.clear(ClearFlags::CHANNELS);

    ResourceSnapshot released;
    released.capture();
    response.set("released", released.toJson());
    const bool no_leak = released == baseline;
    response.set("noLeak", no_leak);

    response.set("success", collision_free && shows_completed && no_residue &&
                                no_leak);
    return response;
}

} // namespace autoresearch

#else

namespace autoresearch {

fl::json runRpPioParallelResourceTest(const fl::json& args) {
    (void)args;
    fl::json response = fl::json::object();
    response.set("success", false);
    response.set("error", "RP PIO parallel resource test requires an RP2040/RP2350");
    return response;
}

} // namespace autoresearch

#endif
