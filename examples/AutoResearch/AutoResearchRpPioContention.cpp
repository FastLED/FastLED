// FastLED#1471 hardware reproduction: PIO state-machine contention.
//
// The reported symptom was random-coloured LEDs when Adafruit TinyUSB was
// linked alongside FastLED on an RP2040. TinyUSB claims PIO state machines;
// so does FastLED's clockless driver. The claim is that FastLED now routes
// every acquisition through RpPioDmaResourceManager (non-panicking
// pio_claim_unused_sm) and therefore never takes a state machine another
// library owns.
//
// That claim had never been executed on silicon. This diagnostic starves the
// PIO block directly -- no TinyUSB needed, since what matters is that the
// state machines are claimed by someone else -- and then asks FastLED to build
// a clockless controller on top of the starved block.

#include "AutoResearchRpPioContention.h"

#include "platforms/arm/rp/is_rp.h"

#if defined(FL_IS_RP2040) || defined(FL_IS_RP2350)

#include <Arduino.h>

#include "FastLED.h"
#include "LegacyClocklessProxy.h"

// IWYU pragma: begin_keep
#include "hardware/pio.h"
// IWYU pragma: end_keep

namespace {

// Pin 2 is inside LegacyClocklessProxy's dispatch table on every RP board and
// is not committed to UART0 (0/1) or the SPI loopback wiring (3, 8, 11).
constexpr int kContentionPin = 2;
constexpr int kContentionLeds = 8;

// Every state machine in every block: 2 blocks on RP2040, 3 on RP2350.
constexpr int kMaxStateMachines = NUM_PIOS * 4;

PIO pioForIndex(int index) {
    if (index == 0) return pio0;
    if (index == 1) return pio1;
#if defined(FL_IS_RP2350)
    if (index == 2) return pio2;
#endif
    return nullptr;
}

int countFreeStateMachines() {
    int free_count = 0;
    for (int pio_index = 0; pio_index < NUM_PIOS; ++pio_index) {
        PIO pio = pioForIndex(pio_index);
        if (pio == nullptr) continue;
        for (uint sm = 0; sm < 4; ++sm) {
            if (!pio_sm_is_claimed(pio, sm)) ++free_count;
        }
    }
    return free_count;
}

/// Claims every free state machine, standing in for TinyUSB.
class PioStarver {
  public:
    ~PioStarver() { release(); }

    int starve() {
        for (int pio_index = 0; pio_index < NUM_PIOS; ++pio_index) {
            PIO pio = pioForIndex(pio_index);
            if (pio == nullptr) continue;
            for (;;) {
                const int sm = pio_claim_unused_sm(pio, false);
                if (sm < 0) break;
                mPio[mCount] = pio;
                mSm[mCount] = sm;
                ++mCount;
            }
        }
        return mCount;
    }

    /// True while every state machine this object took is still marked claimed.
    bool claimsIntact() const {
        for (int i = 0; i < mCount; ++i) {
            if (!pio_sm_is_claimed(mPio[i], static_cast<uint>(mSm[i]))) {
                return false;
            }
        }
        return true;
    }

    void release() {
        while (mCount > 0) {
            --mCount;
            pio_sm_unclaim(mPio[mCount], static_cast<uint>(mSm[mCount]));
        }
    }

  private:
    PIO mPio[kMaxStateMachines] = {};
    int mSm[kMaxStateMachines] = {};
    int mCount = 0;
};

} // namespace

namespace autoresearch {

fl::json runRpPioContentionTest() {
    fl::json response = fl::json::object();
    response.set("numPios", static_cast<int64_t>(NUM_PIOS));
    response.set("pin", static_cast<int64_t>(kContentionPin));

    // Both proxies below are destroyed before this frame returns, so the
    // buffer can live on the stack.
    CRGB leds[kContentionLeds] = {};

    const int baseline_free = countFreeStateMachines();
    response.set("baselineFreeSms", static_cast<int64_t>(baseline_free));

    // Leg 1 (control): with state machines available, FastLED must take one.
    // Without this leg a "FastLED claimed nothing" result under starvation
    // would be indistinguishable from the driver being compiled out.
    fl::json control = fl::json::object();
    {
        LegacyClocklessProxy proxy(kContentionPin, leds, kContentionLeds);
        const int free_during = countFreeStateMachines();
        control.set("proxyValid", proxy.valid());
        control.set("freeDuring", static_cast<int64_t>(free_during));
        control.set("claimedOne",
                    free_during == baseline_free - 1);
        FastLED.show();
    }
    const int control_free_after = countFreeStateMachines();
    control.set("freeAfter", static_cast<int64_t>(control_free_after));
    control.set("released", control_free_after == baseline_free);
    response.set("control", control);

    // Leg 2: starve the block first, exactly as TinyUSB does at boot.
    fl::json contention = fl::json::object();
    bool no_theft = false;
    bool survived_show = false;
    {
        PioStarver starver;
        const int starved = starver.starve();
        contention.set("starvedSms", static_cast<int64_t>(starved));
        contention.set("freeAfterStarve",
                       static_cast<int64_t>(countFreeStateMachines()));

        LegacyClocklessProxy proxy(kContentionPin, leds, kContentionLeds);
        contention.set("proxyValid", proxy.valid());

        // The load-bearing assertion for #1471: the starving owner's claims
        // are all still its own, and nothing new was handed out.
        no_theft = starver.claimsIntact() && countFreeStateMachines() == 0;
        contention.set("claimsIntact", starver.claimsIntact());
        contention.set("noTheft", no_theft);

        // A disabled controller must still be safe to draw through.
        FastLED.show();
        survived_show = true;
        contention.set("survivedShow", survived_show);
    }
    const int contention_free_after = countFreeStateMachines();
    contention.set("freeAfter", static_cast<int64_t>(contention_free_after));
    response.set("contention", contention);

    // A failed acquisition must not strand the partial claims it made.
    const bool no_leak = contention_free_after == baseline_free;
    response.set("noLeak", no_leak);
    response.set("success", no_theft && survived_show && no_leak &&
                               control_free_after == baseline_free);
    return response;
}

} // namespace autoresearch

#else

namespace autoresearch {

fl::json runRpPioContentionTest() {
    fl::json response = fl::json::object();
    response.set("success", false);
    response.set("error", "RP PIO contention test requires an RP2040/RP2350");
    return response;
}

} // namespace autoresearch

#endif
