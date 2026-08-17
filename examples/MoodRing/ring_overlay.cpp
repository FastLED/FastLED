// ring_overlay.cpp - trails and expanding pulse rings on the 1D strip.
#include "ring_overlay.h"

#include "fl/math/math.h"

namespace mood_ring {

namespace {

// How long a pulse ring takes to travel from the origin to the far side.
constexpr fl::u32 kPulseLifetimeMs = 700;

// Ring half-width as a fraction of strip length. Downbeats are visibly fatter
// so a bar boundary reads as larger than an ordinary beat.
constexpr float kSigmaBeat = 0.035f;
constexpr float kSigmaDownbeat = 0.060f;

/// Smoothstep falloff. Cheaper than a true Gaussian and visually equivalent at
/// this scale, with no libm dependency.
float smoothFalloff(float distance, float sigma) {
    if (sigma <= 0.0f) return 0.0f;
    float t = 1.0f - (distance / sigma);
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

/// Shortest distance between two indices on a closed ring.
float ringDistance(int a, int b, int n) {
    int d = a - b;
    if (d < 0) d = -d;
    const int wrapped = n - d;
    return static_cast<float>(d < wrapped ? d : wrapped);
}

} // namespace

void RingOverlay::apply(fl::span<CRGB> leds, const VisualControlBus &bus,
                        fl::u32 nowMs) {
    if (leds.empty()) return;

    if (config.trails) {
        const float decay = (config.trailDecayOverride >= 0.0f)
                                ? config.trailDecayOverride
                                : bus.decayAmount;
        compositeTrails(leds, decay);
    } else {
        // Drop the history so re-enabling trails does not smear a stale frame
        // from minutes ago onto the current one.
        mTrail.clear();
    }

    if (config.pulse) {
        if (bus.pulseStrength > 0.0f) {
            emitPulse(bus.pulseStrength, bus.pulseIsDownbeat, nowMs);
        }
        drawPulses(leds, nowMs);
    }
}

void RingOverlay::compositeTrails(fl::span<CRGB> leds, float decay) {
    const fl::size n = leds.size();
    if (mTrail.size() != n) {
        mTrail.assign(n, CRGB::Black);
    }

    // decay is "how much persists", so the surviving fraction is decay itself.
    float keep = decay;
    if (keep < 0.0f) keep = 0.0f;
    if (keep > 0.98f) keep = 0.98f; // never fully self-sustaining
    const fl::u8 keep8 = static_cast<fl::u8>(keep * 255.0f);

    for (fl::size i = 0; i < n; ++i) {
        CRGB &t = mTrail[i];
        t.nscale8(keep8);
        // Feed a quarter of the live frame into the history each frame.
        t += CRGB(leds[i].r >> 2, leds[i].g >> 2, leds[i].b >> 2);
        // Blend the echo back on top. CRGB::operator+= saturates.
        leds[i] += t;
    }
}

void RingOverlay::emitPulse(float strength, bool downbeat, fl::u32 nowMs) {
    int slot = -1;
    for (int i = 0; i < kMaxPulses; ++i) {
        if (!mPulses[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        // All busy: recycle the oldest.
        fl::u32 oldest = mPulses[0].startMs;
        slot = 0;
        for (int i = 1; i < kMaxPulses; ++i) {
            if (mPulses[i].startMs < oldest) {
                oldest = mPulses[i].startMs;
                slot = i;
            }
        }
    }
    mPulses[slot].startMs = nowMs;
    mPulses[slot].strength = strength;
    mPulses[slot].downbeat = downbeat;
    mPulses[slot].active = true;
}

void RingOverlay::drawPulses(fl::span<CRGB> leds, fl::u32 nowMs) {
    const int n = static_cast<int>(leds.size());
    if (n <= 0) return;

    int origin = config.pulseOrigin % n;
    if (origin < 0) origin += n;

    const float halfRing = static_cast<float>(n) * 0.5f;

    for (int pi = 0; pi < kMaxPulses; ++pi) {
        Pulse &pulse = mPulses[pi];
        if (!pulse.active) continue;

        const fl::u32 elapsed = nowMs - pulse.startMs;
        if (elapsed >= kPulseLifetimeMs) {
            pulse.active = false;
            continue;
        }

        const float life =
            static_cast<float>(elapsed) / static_cast<float>(kPulseLifetimeMs);
        // Radius sweeps from the origin to the far side over the lifetime.
        const float radius = life * halfRing;
        const float sigma =
            static_cast<float>(n) *
            (pulse.downbeat ? kSigmaDownbeat : kSigmaBeat);
        // Fade out as it travels so the ring dissolves instead of vanishing.
        const float amplitude = pulse.strength * (1.0f - life);
        if (amplitude <= 0.0f) continue;

        // Downbeats flip to a contrasting hue; ordinary beats stay neutral so
        // the bar boundary is distinguishable by colour as well as by width.
        const CRGB tint = pulse.downbeat ? CRGB(255, 170, 60) : CRGB(255, 255, 255);

        for (int i = 0; i < n; ++i) {
            const float d = ringDistance(i, origin, n);
            const float k = smoothFalloff(fl::fabs(d - radius), sigma);
            if (k <= 0.0f) continue;
            const fl::u8 level =
                static_cast<fl::u8>(fl::min(255.0f, amplitude * k * 255.0f));
            CRGB add = tint;
            add.nscale8(level);
            leds[i] += add;
        }
    }
}

int RingOverlay::activePulseCount() const {
    int count = 0;
    for (int i = 0; i < kMaxPulses; ++i) {
        if (mPulses[i].active) ++count;
    }
    return count;
}

} // namespace mood_ring
