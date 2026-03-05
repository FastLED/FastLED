// VibeDetector - Self-normalizing audio analysis
//
// Algorithm inspired by MilkDrop v2.25c by Ryan Geiss.
// See MILK_DROP_AUDIO_REACTIVE.md for detailed documentation of the algorithm.

#include "fl/audio/detectors/vibe.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/math.h"

namespace fl {

static int sVibeFFTCount = 0;
int VibeDetector::getPrivateFFTCount() { return sVibeFFTCount; }
void VibeDetector::resetPrivateFFTCount() { sVibeFFTCount = 0; }

namespace {

// FFT configuration: 64 CQ bins from 20-11025 Hz for good resolution
// across all 3 bands. The lower half of the spectrum (0-11025 Hz) is
// divided into 3 equal bands of ~85 linear bins each.
const int kVibeNumBins = 64;
const float kVibeFFTMinFreq = 20.0f;
const float kVibeFFTMaxFreq = 11025.0f;

// Band boundaries (Hz), derived from dividing the lower 256 linear bins
// (of 512 total) into thirds:
//   bass: 0 to ~3650 Hz
//   mid:  ~3650 to ~7350 Hz
//   treb: ~7350 to ~11025 Hz
const float kVibeBandBounds[4] = {20.0f, 3650.0f, 7350.0f, 11025.0f};

} // namespace

VibeDetector::VibeDetector()
{}

VibeDetector::~VibeDetector() = default;

void VibeDetector::update(shared_ptr<AudioContext> context) {
    if (!context) {
        return;
    }

    mFrameCount++;
    mSampleRate = context->getSampleRate();

    // --- Step 1: Get FFT from shared context (downsampled from master) ---
    mRetainedFFT = context->getFFT(kVibeNumBins, kVibeFFTMinFreq, kVibeFFTMaxFreq);
    const FFTBins& fftBins = *mRetainedFFT;
    sVibeFFTCount++;  // Diagnostic counter (no private FFT anymore)

    span<const i16> pcm = context->getPCM();

    const int numBins = static_cast<int>(fftBins.raw().size());
    if (numBins == 0) {
        return;
    }

    // --- Step 2: Sum spectrum into 3 bands ---
    // Sum CQ bins that fall within each frequency range using fractional
    // overlap, producing a sum (total energy) rather than a weighted average.
    for (int band = 0; band < 3; band++) {
        float bandMin = kVibeBandBounds[band];
        float bandMax = kVibeBandBounds[band + 1];
        float energy = 0.0f;

        for (int i = 0; i < numBins; i++) {
            // Compute frequency range for this CQ bin
            float binLow;
            float binHigh;
            if (i == 0) {
                binLow = kVibeFFTMinFreq;
            } else {
                binLow = fftBins.binBoundary(i - 1);
            }
            if (i == numBins - 1) {
                binHigh = kVibeFFTMaxFreq;
            } else {
                binHigh = fftBins.binBoundary(i);
            }

            // Fractional overlap with target band
            float overlapMin = fl::max(binLow, bandMin);
            float overlapMax = fl::min(binHigh, bandMax);
            if (overlapMax <= overlapMin) {
                continue;
            }

            float binWidth = binHigh - binLow;
            if (binWidth <= 0.0f) {
                continue;
            }
            float fraction = (overlapMax - overlapMin) / binWidth;
            energy += fftBins.raw()[i] * fraction;
        }

        mImm[band] = energy;
    }

    // --- Step 3: Temporal blending (asymmetric attack/decay) ---
    // Compute effective FPS from audio buffer duration
    float dt = computeAudioDt(pcm.size(), mSampleRate);
    float actualFps = (dt > 0.0f) ? (1.0f / dt) : mTargetFps;

    for (int i = 0; i < 3; i++) {
        // Short-term average: asymmetric attack/decay
        // Fast attack (rate=0.2 at 30fps → ~80% new signal on beats)
        // Slow decay  (rate=0.5 at 30fps → graceful fadeout)
        float rate;
        if (mImm[i] > mAvg[i]) {
            rate = 0.2f;   // fast attack
        } else {
            rate = 0.5f;   // slow decay
        }
        rate = adjustRateToFPS(rate, 30.0f, actualFps);
        mAvg[i] = mAvg[i] * rate + mImm[i] * (1.0f - rate);

        // Long-term average: song-level adaptation
        // Fast startup convergence (rate=0.9 for first 50 frames ≈ 1.7s at 30fps)
        // Very slow adaptation afterward (rate=0.992, half-life ~2.9s)
        if (mFrameCount < 50) {
            rate = 0.9f;
        } else {
            rate = 0.992f;
        }
        rate = adjustRateToFPS(rate, 30.0f, actualFps);
        mLongAvg[i] = mLongAvg[i] * rate + mImm[i] * (1.0f - rate);

        // Self-normalizing relative levels
        // Division by long-term average makes levels independent of volume/genre.
        // Values hover around 1.0; >1 means louder than recent average.
        if (fl::fabsf(mLongAvg[i]) < 0.001f) {
            mImmRel[i] = 1.0f;
        } else {
            mImmRel[i] = mImm[i] / mLongAvg[i];
        }

        if (fl::fabsf(mLongAvg[i]) < 0.001f) {
            mAvgRel[i] = 1.0f;
        } else {
            mAvgRel[i] = mAvg[i] / mLongAvg[i];
        }
    }

    // --- Step 4: Spike detection ---
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
