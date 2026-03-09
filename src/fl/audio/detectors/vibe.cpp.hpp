// VibeDetector - Self-normalizing audio analysis
//
// Algorithm ported from MilkDrop v2.25c by Ryan Geiss.
// Three-stage processing: immediate FFT → asymmetric EMA → slow EMA normalization.
//
// Uses 3 LINEAR FFT bins that map directly to bass/mid/treb bands.
// Linear bins at 20-11025 Hz with 3 bins gives ~3668 Hz per bin:
//   bin 0: 20-3688 Hz (bass), bin 1: 3688-7356 Hz (mid), bin 2: 7356-11025 Hz (treb)

#include "fl/audio/detectors/vibe.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/math.h"

namespace fl {

static int sVibeFFTCount = 0;
int VibeDetector::getPrivateFFTCount() { return sVibeFFTCount; }
void VibeDetector::resetPrivateFFTCount() { sVibeFFTCount = 0; }


VibeDetector::VibeDetector()
{}

VibeDetector::~VibeDetector() = default;

void VibeDetector::update(shared_ptr<AudioContext> context) {
    if (!context) {
        return;
    }

    mFrameCount++;
    mSampleRate = context->getSampleRate();

    // --- Step 1: Get 3-band energy from shared context ---
    BandEnergy energy = context->getBandEnergy();
    sVibeFFTCount++;

    span<const i16> pcm = context->getPCM();

    // --- Step 2: Read bass/mid/treb directly ---
    mImm[0] = energy.bass;
    mImm[1] = energy.mid;
    mImm[2] = energy.treb;

    // --- Step 3: Temporal blending (MilkDrop v2.25c algorithm) ---
    // Compute effective FPS from audio buffer duration
    float dt = computeAudioDt(pcm.size(), mSampleRate);
    float actualFps = (dt > 0.0f) ? (1.0f / dt) : mTargetFps;

    if (mFrameCount == 1) {
        // MilkDrop first-frame initialization: set averages directly
        // Prevents false spikes and startup transients
        for (int i = 0; i < 3; i++) {
            mAvg[i] = mImm[i];
            mLongAvg[i] = mImm[i];
        }
    } else {
        // Pre-compute FPS-adjusted rates (avoids redundant powf in loop)
        float attackRate = adjustRateToFPS(0.2f, 30.0f, actualFps);
        float decayRate = adjustRateToFPS(0.5f, 30.0f, actualFps);
        float longRate = adjustRateToFPS(0.992f, 30.0f, actualFps);

        for (int i = 0; i < 3; i++) {
            // Short-term average: asymmetric attack/decay
            // Fast attack (rate=0.2 at 30fps → ~80% new signal on beats)
            // Slow decay  (rate=0.5 at 30fps → graceful fadeout)
            float rate = (mImm[i] > mAvg[i]) ? attackRate : decayRate;
            mAvg[i] = mAvg[i] * rate + mImm[i] * (1.0f - rate);

            // Long-term average: slow symmetric EMA (MilkDrop v2.25c algorithm)
            // Rate = 0.992 at 30fps → tau ≈ 4.2 seconds
            // Tracks the overall energy level for self-normalization.
            // Unlike a running maximum, this centers relative levels around 1.0,
            // giving beats proper excursions above 1.0 and quiet sections below 1.0.
            mLongAvg[i] = mLongAvg[i] * longRate + mAvg[i] * (1.0f - longRate);
        }
    }

    // --- Step 4: Self-normalizing relative levels ---
    // Division by long-term average makes levels independent of volume/genre.
    // Values hover around 1.0; >1 means louder than average, <1 means quieter.
    for (int i = 0; i < 3; i++) {
        if (mLongAvg[i] < 0.001f) {
            mImmRel[i] = 1.0f;
            mAvgRel[i] = 1.0f;
        } else {
            mImmRel[i] = mImm[i] / mLongAvg[i];
            mAvgRel[i] = mAvg[i] / mLongAvg[i];
        }
    }

    // --- Step 5: Spike detection ---
    // When immediate exceeds smoothed, energy is rising — a beat is in progress.
    mPrevBassSpike = mBassSpike;
    mPrevMidSpike = mMidSpike;
    mPrevTrebSpike = mTrebSpike;

    mBassSpike = mImmRel[0] > mAvgRel[0];
    mMidSpike = mImmRel[1] > mAvgRel[1];
    mTrebSpike = mImmRel[2] > mAvgRel[2];
}

void VibeDetector::fireCallbacks() {
    if (onVibeLevels) {
        VibeLevels levels;
        levels.bass = mImmRel[0];
        levels.mid = mImmRel[1];
        levels.treb = mImmRel[2];
        levels.vol = (mImmRel[0] + mImmRel[1] + mImmRel[2]) / 3.0f;
        levels.bassSpike = mBassSpike;
        levels.midSpike = mMidSpike;
        levels.trebSpike = mTrebSpike;
        levels.bassRaw = mImm[0];
        levels.midRaw = mImm[1];
        levels.trebRaw = mImm[2];
        levels.bassAvg = mAvg[0];
        levels.midAvg = mAvg[1];
        levels.trebAvg = mAvg[2];
        levels.bassLongAvg = mLongAvg[0];
        levels.midLongAvg = mLongAvg[1];
        levels.trebLongAvg = mLongAvg[2];
        onVibeLevels(levels);
    }

    // Fire spike callbacks on rising edge (transition from no-spike to spike)
    if (mBassSpike && !mPrevBassSpike && onBassSpike) {
        onBassSpike();
    }
    if (mMidSpike && !mPrevMidSpike && onMidSpike) {
        onMidSpike();
    }
    if (mTrebSpike && !mPrevTrebSpike && onTrebSpike) {
        onTrebSpike();
    }
}

void VibeDetector::reset() {
    mFrameCount = 0;
    for (int i = 0; i < 3; i++) {
        mImm[i] = 0.0f;
        mAvg[i] = 0.0f;
        mLongAvg[i] = 0.0f;
        mImmRel[i] = 1.0f;
        mAvgRel[i] = 1.0f;
    }
    mBassSpike = false;
    mMidSpike = false;
    mTrebSpike = false;
    mPrevBassSpike = false;
    mPrevMidSpike = false;
    mPrevTrebSpike = false;
}

// FPS-independent rate adjustment:
// Converts a per-frame smoothing rate tuned at fps1 to the equivalent rate
// at actualFps, preserving the per-second behavior.
//
// At 30fps with rate=0.5: per-second retention = 0.5^30 ≈ 9.3e-10
// At 60fps the per-frame rate adjusts to ~0.707 so 0.707^60 ≈ 9.3e-10 (same)
float VibeDetector::adjustRateToFPS(float rateAtFps1, float fps1,
                                     float actualFps) {
    if (actualFps <= 0.0f) {
        return rateAtFps1;
    }
    float perSecond = fl::powf(rateAtFps1, fps1);
    return fl::powf(perSecond, 1.0f / actualFps);
}

} // namespace fl
