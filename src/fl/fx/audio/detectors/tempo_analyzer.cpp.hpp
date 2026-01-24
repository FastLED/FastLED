#include "fl/fx/audio/detectors/tempo_analyzer.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/math.h"

namespace fl {

TempoAnalyzer::TempoAnalyzer()
    : mCurrentBPM(120.0f)
    , mConfidence(0.0f)
    , mIsStable(false)
    , mStability(0.0f)
    , mMinBPM(60.0f)
    , mMaxBPM(180.0f)
    , mStabilityThreshold(0.8f)
    , mPreviousFlux(0.0f)
    , mAdaptiveThreshold(0.0f)
    , mStableFrameCount(0)
{
    mHypotheses.reserve(MAX_HYPOTHESES);
    mOnsetTimes.reserve(MAX_ONSET_HISTORY);
    mFluxHistory.reserve(FLUX_HISTORY_SIZE);
    mBPMHistory.reserve(BPM_HISTORY_SIZE);
}

TempoAnalyzer::~TempoAnalyzer() = default;

void TempoAnalyzer::update(shared_ptr<AudioContext> context) {
    const FFTBins& fft = context->getFFT(16);
    u32 timestamp = context->getTimestamp();

    // Calculate spectral flux for onset detection
    float flux = calculateSpectralFlux(fft);

    // Update adaptive threshold
    updateAdaptiveThreshold();

    // Detect onsets
    if (detectOnset(timestamp)) {
        mOnsetTimes.push_back(timestamp);
        if (mOnsetTimes.size() > MAX_ONSET_HISTORY) {
            mOnsetTimes.erase(mOnsetTimes.begin());
        }

        // Update tempo hypotheses
        updateHypotheses(timestamp);
    }

    mPreviousFlux = flux;

    // Prune weak hypotheses
    pruneHypotheses();

    // Update current tempo based on best hypothesis
    float previousBPM = mCurrentBPM;
    updateCurrentTempo();

    // Update stability analysis
    updateStability();

    // Fire callbacks
    if (onTempo) {
        onTempo(mCurrentBPM);
    }

    if (onTempoWithConfidence) {
        onTempoWithConfidence(mCurrentBPM, mConfidence);
    }

    // Check for tempo change
    float bpmDiff = fl::fl_abs(mCurrentBPM - previousBPM);
    if (bpmDiff > 5.0f && onTempoChange) {
        onTempoChange(mCurrentBPM);
    }

    // Check for stability change
    static bool wasStable = false;
    if (mIsStable && !wasStable && onTempoStable) {
        onTempoStable();
    } else if (!mIsStable && wasStable && onTempoUnstable) {
        onTempoUnstable();
    }
    wasStable = mIsStable;
}

void TempoAnalyzer::reset() {
    mCurrentBPM = 120.0f;
    mConfidence = 0.0f;
    mIsStable = false;
    mStability = 0.0f;
    mPreviousFlux = 0.0f;
    mAdaptiveThreshold = 0.0f;
    mStableFrameCount = 0;
    mHypotheses.clear();
    mOnsetTimes.clear();
    mFluxHistory.clear();
    mBPMHistory.clear();
}

float TempoAnalyzer::calculateSpectralFlux(const FFTBins& fft) {
    // Focus on low-to-mid frequencies for beat detection
    float flux = 0.0f;
    size numBins = fl::fl_min(static_cast<size>(8), fft.bins_raw.size());

    for (size i = 0; i < numBins; i++) {
        float diff = fft.bins_raw[i] - mPreviousFlux;
        if (diff > 0.0f) {
            flux += diff;
        }
    }

    return flux / static_cast<float>(numBins);
}

void TempoAnalyzer::updateAdaptiveThreshold() {
    // Add current flux to history
    if (mFluxHistory.size() >= FLUX_HISTORY_SIZE) {
        mFluxHistory.erase(mFluxHistory.begin());
    }
    mFluxHistory.push_back(mPreviousFlux);

    // Calculate adaptive threshold
    if (!mFluxHistory.empty()) {
        float sum = 0.0f;
        for (size i = 0; i < mFluxHistory.size(); i++) {
            sum += mFluxHistory[i];
        }
        float mean = sum / static_cast<float>(mFluxHistory.size());
        mAdaptiveThreshold = mean * 1.5f;
    }
}

bool TempoAnalyzer::detectOnset(u32 timestamp) {
    // Simple onset detection: flux exceeds adaptive threshold
    if (mPreviousFlux <= mAdaptiveThreshold) {
        return false;
    }

    // Avoid detecting onsets too close together
    if (!mOnsetTimes.empty()) {
        u32 timeSinceLastOnset = timestamp - mOnsetTimes.back();
        if (timeSinceLastOnset < 50) {  // 50ms minimum gap
            return false;
        }
    }

    return true;
}

void TempoAnalyzer::updateHypotheses(u32 timestamp) {
    // For each existing onset, create/update hypotheses
    for (size i = 0; i < mOnsetTimes.size(); i++) {
        if (i == mOnsetTimes.size() - 1) continue;  // Skip the just-added onset

        u32 interval = timestamp - mOnsetTimes[i];

        // Check if interval is in valid BPM range
        if (interval < MIN_BEAT_INTERVAL_MS || interval > MAX_BEAT_INTERVAL_MS) {
            continue;
        }

        float bpm = 60000.0f / static_cast<float>(interval);

        // Check if this BPM is within user-defined range
        if (bpm < mMinBPM || bpm > mMaxBPM) {
            continue;
        }

        // Check if we already have a similar hypothesis
        bool foundExisting = false;
        for (size j = 0; j < mHypotheses.size(); j++) {
            float bpmDiff = fl::fl_abs(mHypotheses[j].bpm - bpm);
            if (bpmDiff < 3.0f) {  // Within 3 BPM tolerance
                // Update existing hypothesis
                mHypotheses[j].bpm = (mHypotheses[j].bpm + bpm) * 0.5f;
                mHypotheses[j].score += calculateIntervalScore(interval);
                mHypotheses[j].lastOnsetTime = timestamp;
                mHypotheses[j].onsetCount++;
                foundExisting = true;
                break;
            }
        }

        // Create new hypothesis if not found and we have room
        if (!foundExisting && mHypotheses.size() < MAX_HYPOTHESES) {
            TempoHypothesis hyp;
            hyp.bpm = bpm;
            hyp.score = calculateIntervalScore(interval);
            hyp.lastOnsetTime = timestamp;
            hyp.onsetCount = 1;
            mHypotheses.push_back(hyp);
        }
    }
}

void TempoAnalyzer::pruneHypotheses() {
    // Remove hypotheses that haven't been updated recently
    for (size i = 0; i < mHypotheses.size(); ) {
        // Decay score over time
        mHypotheses[i].score *= 0.95f;

        if (mHypotheses[i].score < 0.1f) {
            mHypotheses.erase(mHypotheses.begin() + i);
        } else {
            i++;
        }
    }

    // Sort by score (highest first)
    for (size i = 0; i < mHypotheses.size(); i++) {
        for (size j = i + 1; j < mHypotheses.size(); j++) {
            if (mHypotheses[j].score > mHypotheses[i].score) {
                TempoHypothesis temp = mHypotheses[i];
                mHypotheses[i] = mHypotheses[j];
                mHypotheses[j] = temp;
            }
        }
    }

    // Keep only top hypotheses
    if (mHypotheses.size() > MAX_HYPOTHESES) {
        mHypotheses.resize(MAX_HYPOTHESES);
    }
}

void TempoAnalyzer::updateCurrentTempo() {
    if (mHypotheses.empty()) {
        mConfidence = 0.0f;
        return;
    }

    // Use the best hypothesis
    const TempoHypothesis& best = mHypotheses[0];
    mCurrentBPM = best.bpm;
    mConfidence = calculateTempoConfidence(best);

    // Add to BPM history
    mBPMHistory.push_back(mCurrentBPM);
    if (mBPMHistory.size() > BPM_HISTORY_SIZE) {
        mBPMHistory.erase(mBPMHistory.begin());
    }
}

void TempoAnalyzer::updateStability() {
    if (mBPMHistory.size() < 5) {
        mStability = 0.0f;
        mIsStable = false;
        mStableFrameCount = 0;
        return;
    }

    // Calculate variance of recent BPM estimates
    float sum = 0.0f;
    for (size i = 0; i < mBPMHistory.size(); i++) {
        sum += mBPMHistory[i];
    }
    float mean = sum / static_cast<float>(mBPMHistory.size());

    float variance = 0.0f;
    for (size i = 0; i < mBPMHistory.size(); i++) {
        float diff = mBPMHistory[i] - mean;
        variance += diff * diff;
    }
    variance /= static_cast<float>(mBPMHistory.size());

    // Convert variance to stability score (low variance = high stability)
    float stddev = fl::sqrt(variance);
    mStability = fl::fl_max(0.0f, 1.0f - (stddev / 10.0f));

    // Check if stable
    if (mStability >= mStabilityThreshold) {
        mStableFrameCount++;
        if (mStableFrameCount >= STABLE_FRAMES_REQUIRED) {
            mIsStable = true;
        }
    } else {
        mStableFrameCount = 0;
        mIsStable = false;
    }
}

float TempoAnalyzer::calculateIntervalScore(u32 interval) {
    // Higher score for intervals in typical music range (60-140 BPM)
    float bpm = 60000.0f / static_cast<float>(interval);

    float targetBPM = (mMinBPM + mMaxBPM) * 0.5f;
    float diff = fl::fl_abs(bpm - targetBPM);
    float normalizedDiff = diff / (mMaxBPM - mMinBPM);

    return fl::fl_max(0.1f, 1.0f - normalizedDiff);
}

float TempoAnalyzer::calculateTempoConfidence(const TempoHypothesis& hyp) {
    // Confidence based on:
    // 1. Hypothesis score
    // 2. Number of onsets supporting it
    // 3. Stability

    float scoreComponent = fl::fl_min(1.0f, hyp.score);
    float onsetComponent = fl::fl_min(1.0f, static_cast<float>(hyp.onsetCount) / 10.0f);
    float stabilityComponent = mStability;

    return (scoreComponent * 0.4f + onsetComponent * 0.3f + stabilityComponent * 0.3f);
}

} // namespace fl
