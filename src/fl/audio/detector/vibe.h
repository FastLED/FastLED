// Vibe - Self-normalizing audio-reactive analysis for FastLED
//
// Produces self-normalizing, FPS-independent bass/mid/treb levels with
// asymmetric attack/decay smoothing. Algorithm ported from Ryan Geiss's
// MilkDrop v2.25c visualizer's DoCustomSoundAnalysis() function.
//
// CORRECTED ALGORITHM (as of 2026-03-07):
// Now uses slow symmetric EMA for long-term normalization, matching the
// MilkDrop v2.25c algorithm: long_avg = long_avg*0.992 + avg*0.008 at 30fps.
// First frame initializes all averages directly (no smoothing lag), preventing
// false spikes and startup transients.
//
// Key properties:
//   - Self-normalizing: relative levels center around 1.0 using slow EMA
//   - Asymmetric smoothing: fast attack (beats hit hard), slow decay (graceful fade)
//   - Dual timescale: short-term smoothing for beat tracking, slow EMA for normalization
//   - FPS-independent: identical behavior at 30fps, 60fps, or 144fps
//   - Spike detection: bass > bassAtt indicates a beat is happening (energy rising)
//   - First-frame init: no false spikes on startup (matches MilkDrop)
//
// Usage:
//   Vibe vibe;
//   vibe.onBassSpike.add([]() { /* beat! */ });
//   // In update loop:
//   vibe.update(context);
//   float zoom = 1.0f + 0.1f * (vibe.getBass() - 1.0f);

#pragma once

// NOTE (#2328): The defensive `#ifndef FL_AUDIO_DETECTOR_VIBE_H` guard that
// was added in #2325 has been removed. It existed because Meson was emitting
// -I flags with mixed spellings (backslash vs forward-slash, relative vs
// absolute) on Windows, which defeated clang's #pragma once file-identity
// check. All `-I` flags are now absolute + forward-slash + spelled
// identically (see ci/meson/shared/meson.build path_norm_*), so #pragma once
// is sufficient on its own.

#include "fl/audio/audio_detector.h"
#include "fl/audio/fft/fft.h"
#include "fl/audio/silence_envelope.h"
#include "fl/stl/function.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {
namespace detector {

struct VibeLevels {
    // Self-normalizing relative levels (~1.0 = average for current song)
    float bass = 1.0f;
    float mid = 1.0f;
    float treb = 1.0f;
    float vol = 1.0f;       // (bass + mid + treb) / 3

    // Spike detection (true when energy is rising — a beat/transient)
    bool bassSpike = false;
    bool midSpike = false;
    bool trebSpike = false;

    // Absolute values (non-normalized)
    float bassRaw = 0.0f;      // Immediate absolute band energy
    float midRaw = 0.0f;
    float trebRaw = 0.0f;
    float bassAvg = 0.0f;      // Short-term smoothed absolute
    float midAvg = 0.0f;
    float trebAvg = 0.0f;
    float bassLongAvg = 0.0f;  // Long-term average absolute
    float midLongAvg = 0.0f;
    float trebLongAvg = 0.0f;
};

class Vibe : public Detector {
public:
    Vibe() FL_NOEXCEPT;
    ~Vibe() FL_NOEXCEPT override;

    // Detector interface
    void update(shared_ptr<Context> context) FL_NOEXCEPT override;
    void fireCallbacks() FL_NOEXCEPT override;
    bool needsFFT() const FL_NOEXCEPT override { return true; }
    const char* getName() const FL_NOEXCEPT override { return "Vibe"; }
    void reset() FL_NOEXCEPT override;
    void setSampleRate(int sampleRate) FL_NOEXCEPT override { mSampleRate = sampleRate; }

    // ---- Self-normalizing relative levels (the primary API) ----
    // These hover around 1.0 for the current song. >1 = louder than average,
    // <1 = quieter.

    float getBass() const FL_NOEXCEPT { return mImmRel[0]; }
    float getMid() const FL_NOEXCEPT { return mImmRel[1]; }
    float getTreb() const FL_NOEXCEPT { return mImmRel[2]; }
    float getVol() const FL_NOEXCEPT { return (mImmRel[0] + mImmRel[1] + mImmRel[2]) / 3.0f; }

    // ---- Smoothed relative levels ("attenuated") ----
    // Short-term smoothed versions. When bass > bassAtt, a beat is happening.

    float getBassAtt() const FL_NOEXCEPT { return mAvgRel[0]; }
    float getMidAtt() const FL_NOEXCEPT { return mAvgRel[1]; }
    float getTrebAtt() const FL_NOEXCEPT { return mAvgRel[2]; }
    float getVolAtt() const FL_NOEXCEPT { return (mAvgRel[0] + mAvgRel[1] + mAvgRel[2]) / 3.0f; }

    // ---- Spike detection ----
    // True when the immediate relative level exceeds the smoothed level,
    // meaning energy is rising — a transient/beat is in progress.

    bool isBassSpike() const FL_NOEXCEPT { return mBassSpike; }
    bool isMidSpike() const FL_NOEXCEPT { return mMidSpike; }
    bool isTrebSpike() const FL_NOEXCEPT { return mTrebSpike; }

    // ---- Raw absolute values (for advanced use) ----

    float getBassRaw() const FL_NOEXCEPT { return mImm[0]; }
    float getMidRaw() const FL_NOEXCEPT { return mImm[1]; }
    float getTrebRaw() const FL_NOEXCEPT { return mImm[2]; }

    float getBassAvg() const FL_NOEXCEPT { return mAvg[0]; }
    float getMidAvg() const FL_NOEXCEPT { return mAvg[1]; }
    float getTrebAvg() const FL_NOEXCEPT { return mAvg[2]; }

    float getBassLongAvg() const FL_NOEXCEPT { return mLongAvg[0]; }
    float getMidLongAvg() const FL_NOEXCEPT { return mLongAvg[1]; }
    float getTrebLongAvg() const FL_NOEXCEPT { return mLongAvg[2]; }

    // ---- Callbacks ----

    // Fired every frame with comprehensive vibe levels
    function_list<void(const VibeLevels&)> onVibeLevels;
    // Fired when a spike is detected in each band (rising edge only)
    function_list<void()> onBassSpike;
    function_list<void()> onMidSpike;
    function_list<void()> onTrebSpike;

    // ---- Configuration ----

    // Target FPS for rate adjustment (default 30.0).
    // Only needed if the audio update rate doesn't match the actual frame rate.
    void setTargetFps(float fps) FL_NOEXCEPT { mTargetFps = fps; }

    // Diagnostic counters
    static int getPrivateFFTCount() FL_NOEXCEPT;
    static void resetPrivateFFTCount() FL_NOEXCEPT;

private:
    int mSampleRate = 44100;
    int mFrameCount = 0;
    float mTargetFps = 30.0f;

    // Band energy data
    float mImm[3] = {};       // Immediate band energy (absolute)
    float mAvg[3] = {};       // Short-term smoothed (absolute)
    // Long-term slow symmetric EMA (MilkDrop v2.25c algorithm)
    // Rate = 0.992 at 30fps (tau ≈ 4.2 seconds), tracks average level for normalization
    float mLongAvg[3] = {};   // Long-term average for self-normalization
    float mImmRel[3] = {1.0f, 1.0f, 1.0f};  // Immediate relative to song
    float mAvgRel[3] = {1.0f, 1.0f, 1.0f};  // Smoothed relative to song

    // Beat detection state
    bool mBassSpike = false;
    bool mMidSpike = false;
    bool mTrebSpike = false;
    bool mPrevBassSpike = false;
    bool mPrevMidSpike = false;
    bool mPrevTrebSpike = false;

    // Silence gate envelopes — decay mImmRel[i] and mAvgRel[i] toward zero
    // when Context::isSilent() is true. Fixes MilkDrop self-normalization's
    // stuck-at-1.0 behavior when music stops. Pass-through during audio so
    // beat dynamics are preserved exactly.
    SilenceEnvelope mImmRelEnv[3];
    SilenceEnvelope mAvgRelEnv[3];

    // FPS-independent rate adjustment
    static float adjustRateToFPS(float rateAtFps1, float fps1, float actualFps) FL_NOEXCEPT;
};

} // namespace detector
} // namespace audio
} // namespace fl
