// ring_overlay.h - engine-agnostic post-process passes on the 1D ring.
//
// Runs after the engine has drawn and before FastLED.show(). It never touches
// Animartrix internals, which is what makes it reusable: when the NoiseRing
// engine lands it inherits trails and pulses for free.
//
// See issue #3885.
#pragma once

#include "FastLED.h"
#include "fl/stl/span.h"
#include "fl/stl/stdint.h"
#include "fl/stl/vector.h"

#include "visual_control_bus.h"

namespace mood_ring {

struct OverlayConfig {
    bool trails = true;
    bool pulse = true;
    int pulseOrigin = 0;           ///< ring index a pulse expands from
    float trailDecayOverride = -1.0f; ///< <0 = follow bus.decayAmount
};

/// Trails plus expanding pulse rings, composited onto the strip in place.
class RingOverlay {
  public:
    OverlayConfig config;

    /// Composite this frame. Safe to call with an empty span.
    void apply(fl::span<CRGB> leds, const VisualControlBus &bus, fl::u32 nowMs);

    /// How many pulse rings are currently in flight (for the debug readout).
    int activePulseCount() const;

  private:
    void compositeTrails(fl::span<CRGB> leds, float decay);
    void emitPulse(float strength, bool downbeat, fl::u32 nowMs);
    void drawPulses(fl::span<CRGB> leds, fl::u32 nowMs);

    /// A pulse ring travelling outward from the origin index.
    struct Pulse {
        fl::u32 startMs = 0;
        float strength = 0.0f;
        bool downbeat = false;
        bool active = false;
    };

    // Bounded so a dense passage cannot grow unbounded state. Oldest is
    // recycled when full, which reads correctly: the newest hits are the ones
    // the eye tracks.
    static const int kMaxPulses = 8;
    Pulse mPulses[kMaxPulses];

    fl::vector<CRGB> mTrail; ///< persistence shadow buffer, sized on first use
};

} // namespace mood_ring
