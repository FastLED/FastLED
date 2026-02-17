#include "fl/fx/audio/detectors/musical_beat_detector.h"
#include "fl/int.h"
#include "fl/stl/math.h"
#include "fl/stl/vector.h"

namespace fl {

MusicalBeatDetector::MusicalBeatDetector() = default;

MusicalBeatDetector::MusicalBeatDetector(const MusicalBeatDetectorConfig& config) {
    configure(config);
}

MusicalBeatDetector::~MusicalBeatDetector() = default;

void MusicalBeatDetector::configure(const MusicalBeatDetectorConfig& config) {
    mConfig = config;
    mIBIHistory.clear();
    mIBIHistory.reserve(mConfig.maxIBIHistory);
    reset();
}

void MusicalBeatDetector::processSample(bool onsetDetected, float onsetStrength) {
    mBeatDetected = false;
    mLastBeatConfidence = 0.0f;
    mCurrentFrame++;

    if (!onsetDetected) {
        return;
    }

    mStats.totalOnsets++;

    // Validate if this onset is a true musical beat
    if (validateBeat(onsetStrength)) {
        mBeatDetected = true;
        mStats.validatedBeats++;

        // Calculate inter-beat interval (IBI)
        u32 ibiFrames = mCurrentFrame - mLastBeatFrame;
        mLastBeatFrame = mCurrentFrame;

        // Convert IBI from frames to seconds
        float ibiSeconds = static_cast<float>(ibiFrames * mConfig.samplesPerFrame) /
                          static_cast<float>(mConfig.sampleRate);

        // Validate IBI is within BPM range
        if (isValidIBI(ibiSeconds)) {
            // Add to history
            if (mIBIHistory.size() >= mConfig.maxIBIHistory) {
                // Remove oldest IBI
                mIBIHistory.erase(mIBIHistory.begin());
            }
            mIBIHistory.push_back(ibiFrames);

            // Calculate beat confidence
            mLastBeatConfidence = calculateBeatConfidence(ibiSeconds);

            // Update BPM estimate
            updateBPMEstimate();

            // Update stats
            mStats.currentBPM = mCurrentBPM;
            mStats.averageIBI = getAverageIBI();
            mStats.ibiCount = static_cast<u32>(mIBIHistory.size());
        } else {
            // IBI out of valid range - reject beat
            mBeatDetected = false;
            mStats.rejectedOnsets++;
        }
    } else {
        mStats.rejectedOnsets++;
    }
}

bool MusicalBeatDetector::isBeat() const {
    return mBeatDetected && (mLastBeatConfidence >= mConfig.minBeatConfidence);
}

float MusicalBeatDetector::getBPM() const {
    return mCurrentBPM;
}

float MusicalBeatDetector::getBeatConfidence() const {
    return mLastBeatConfidence;
}

float MusicalBeatDetector::getAverageIBI() const {
    if (mIBIHistory.empty()) {
        return 0.0f;
    }

    u32 sum = 0;
    for (size i = 0; i < mIBIHistory.size(); ++i) {
        sum += mIBIHistory[i];
    }

    float avgFrames = static_cast<float>(sum) / static_cast<float>(mIBIHistory.size());
    return (avgFrames * static_cast<float>(mConfig.samplesPerFrame)) /
           static_cast<float>(mConfig.sampleRate);
}

void MusicalBeatDetector::reset() {
    mBeatDetected = false;
    mLastBeatConfidence = 0.0f;
    mCurrentBPM = 120.0f;  // Default to common tempo
    mLastBeatFrame = 0;
    mCurrentFrame = 0;
    mIBIHistory.clear();
    mAverageIBI = 0.0f;

    mStats.totalOnsets = 0;
    mStats.validatedBeats = 0;
    mStats.rejectedOnsets = 0;
    mStats.currentBPM = mCurrentBPM;
    mStats.averageIBI = 0.0f;
    mStats.ibiCount = 0;
}

bool MusicalBeatDetector::validateBeat(float onsetStrength) {
    // First beat always validates (no history to compare)
    if (mLastBeatFrame == 0) {
        return true;
    }

    // Calculate expected IBI based on current BPM
    float expectedIBI = 60.0f / mCurrentBPM;  // seconds per beat
    float expectedFrames = (expectedIBI * static_cast<float>(mConfig.sampleRate)) /
                          static_cast<float>(mConfig.samplesPerFrame);

    // Calculate actual IBI since last beat
    float actualFrames = static_cast<float>(mCurrentFrame - mLastBeatFrame);

    // Allow ±25% deviation from expected tempo
    float tolerance = 0.25f;
    float minExpected = expectedFrames * (1.0f - tolerance);
    float maxExpected = expectedFrames * (1.0f + tolerance);

    // If we have no IBI history yet, accept any beat within valid BPM range
    if (mIBIHistory.empty()) {
        float actualIBI = (actualFrames * static_cast<float>(mConfig.samplesPerFrame)) /
                         static_cast<float>(mConfig.sampleRate);
        return isValidIBI(actualIBI);
    }

    // Validate timing matches expected tempo
    return (actualFrames >= minExpected) && (actualFrames <= maxExpected);
}

float MusicalBeatDetector::calculateBeatConfidence(float currentIBI) {
    // Confidence based on temporal consistency
    if (mIBIHistory.size() < 2) {
        // Not enough history - return moderate confidence
        return 0.6f;
    }

    // Calculate IBI standard deviation
    float stdDev = calculateIBIStdDev();

    // Average IBI in seconds
    float avgIBI = getAverageIBI();

    // Coefficient of variation (normalized std dev)
    float cv = (avgIBI > 0.0f) ? (stdDev / avgIBI) : 1.0f;

    // Confidence inversely proportional to variability
    // cv = 0.0 (perfect consistency) → confidence = 1.0
    // cv = 0.2 (20% variation) → confidence ≈ 0.5
    // cv ≥ 0.5 (50%+ variation) → confidence → 0.0
    float confidence = fl::max(0.0f, 1.0f - (cv * 2.0f));

    // Boost confidence if current IBI matches average closely
    float ibiDiff = fl::abs(currentIBI - avgIBI);
    float ibiError = (avgIBI > 0.0f) ? (ibiDiff / avgIBI) : 1.0f;
    float ibiBonus = fl::max(0.0f, 1.0f - (ibiError * 4.0f));

    // Combined confidence (weighted average)
    return (confidence * 0.7f) + (ibiBonus * 0.3f);
}

void MusicalBeatDetector::updateBPMEstimate() {
    if (mIBIHistory.empty()) {
        return;
    }

    // Calculate BPM from average IBI
    float avgIBI = getAverageIBI();
    if (avgIBI <= 0.0f) {
        return;
    }

    float instantaneousBPM = 60.0f / avgIBI;

    // Clamp to valid range
    instantaneousBPM = fl::max(mConfig.minBPM, fl::min(mConfig.maxBPM, instantaneousBPM));

    // Smooth BPM estimate (exponential moving average)
    float alpha = mConfig.bpmSmoothingAlpha;
    mCurrentBPM = (alpha * mCurrentBPM) + ((1.0f - alpha) * instantaneousBPM);

    // Ensure BPM stays in valid range after smoothing
    mCurrentBPM = fl::max(mConfig.minBPM, fl::min(mConfig.maxBPM, mCurrentBPM));
}

bool MusicalBeatDetector::isValidIBI(float ibi) const {
    if (ibi <= 0.0f) {
        return false;
    }

    // Convert IBI to BPM
    float bpm = 60.0f / ibi;

    // Check if BPM is within valid range
    return (bpm >= mConfig.minBPM) && (bpm <= mConfig.maxBPM);
}

float MusicalBeatDetector::calculateIBIStdDev() const {
    if (mIBIHistory.size() < 2) {
        return 0.0f;
    }

    // Calculate mean IBI in frames
    u32 sum = 0;
    for (size i = 0; i < mIBIHistory.size(); ++i) {
        sum += mIBIHistory[i];
    }
    float mean = static_cast<float>(sum) / static_cast<float>(mIBIHistory.size());

    // Calculate variance
    float variance = 0.0f;
    for (size i = 0; i < mIBIHistory.size(); ++i) {
        float diff = static_cast<float>(mIBIHistory[i]) - mean;
        variance += diff * diff;
    }
    variance /= static_cast<float>(mIBIHistory.size());

    // Convert std dev from frames to seconds
    float stdDevFrames = fl::sqrt(variance);
    return (stdDevFrames * static_cast<float>(mConfig.samplesPerFrame)) /
           static_cast<float>(mConfig.sampleRate);
}

} // namespace fl
