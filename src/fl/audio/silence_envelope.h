// SilenceEnvelope — decay-to-target helper for silence gating.
//
// Detectors compose a SilenceEnvelope into their update() to translate the
// boolean `Context::isSilent()` flag into a smooth per-metric decay. During
// audio the envelope is a pass-through; during silence it exponentially
// decays the cached value toward `targetValue`. On silence→audio transition
// it snaps back to the fresh value so beats hit hard with zero attack lag.
//
// Example:
//   SilenceEnvelope env;                 // default tau = 0.5s, target = 0
//   float vol = env.update(context->isSilent(), rawVol, dt);

#pragma once

#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {

class SilenceEnvelope {
public:
    struct Config {
        // Time constant for exponential decay toward targetValue during silence.
        // After ~3*decayTauSeconds the envelope is within 5% of target.
        float decayTauSeconds = 0.5f;
        // Value the envelope decays toward during silence (usually 0).
        float targetValue = 0.0f;
    };

    SilenceEnvelope() FL_NO_EXCEPT;
    explicit SilenceEnvelope(const Config& cfg) FL_NO_EXCEPT;

    void configure(const Config& cfg) FL_NO_EXCEPT { mConfig = cfg; }

    // Pass-through when !isSilent (returns currentValue, caches it internally).
    // Exponential decay toward Config::targetValue when isSilent. Snaps back
    // to currentValue on silence→audio transition so re-attack has no lag.
    // `dt` is seconds since last call; use computeAudioDt() from audio_context.h.
    float update(bool isSilent, float currentValue, float dt) FL_NO_EXCEPT;

    // Snap the envelope to `initialValue` (used on reset or explicit jumps).
    void reset(float initialValue = 0.0f) FL_NO_EXCEPT;

    // True once the envelope has decayed to within `epsilon` of targetValue.
    bool isGated(float epsilon = 1e-4f) const FL_NO_EXCEPT;

    // Current envelope output (last value returned by update()).
    float value() const FL_NO_EXCEPT { return mCurrent; }

    const Config& config() const FL_NO_EXCEPT { return mConfig; }

private:
    Config mConfig;
    float mCurrent = 0.0f;
};

} // namespace audio
} // namespace fl
