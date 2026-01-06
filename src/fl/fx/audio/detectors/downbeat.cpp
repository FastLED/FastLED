#include "fl/fx/audio/detectors/downbeat.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/math.h"
#include "fl/stl/array.h"

namespace fl {

DownbeatDetector::DownbeatDetector(shared_ptr<BeatDetector> beatDetector)
    : mBeatDetector(beatDetector)
    , mOwnsBeatDetector(false)
    , mDownbeatDetected(false)
    , mCurrentBeat(1)
    , mBeatsPerMeasure(4)
    , mMeasurePhase(0.0f)
    , mConfidence(0.0f)
    , mConfidenceThreshold(0.6f)
    , mAccentThreshold(1.2f)
    , mAutoMeterDetection(true)
    , mManualMeter(false)
    , mLastDownbeatTime(0)
    , mLastBeatTime(0)
    , mBeatsSinceDownbeat(0)
    , mPreviousEnergy(0.0f)
{
    mBeatAccents.reserve(MAX_BEAT_HISTORY);
    mMeterCandidates.reserve(METER_HISTORY_SIZE);
}

DownbeatDetector::DownbeatDetector()
    : DownbeatDetector(make_shared<BeatDetector>())
{
    mOwnsBeatDetector = true;
}

DownbeatDetector::~DownbeatDetector() = default;

void DownbeatDetector::update(shared_ptr<AudioContext> context) {
    // Update BeatDetector if we own it
    if (mOwnsBeatDetector) {
        updateBeatDetector(context);
    }

    // Get current state from BeatDetector
    bool beatDetected = mBeatDetector->isBeat();
    u32 timestamp = context->getTimestamp();

    // Reset downbeat flag
    mDownbeatDetected = false;

    // If beat detected, analyze for downbeat
    if (beatDetected) {
        // Get FFT for accent analysis
        const FFTBins& fft = context->getFFT(16);

        // Calculate current energy (bass-weighted for accent detection)
        float bassEnergy = 0.0f;
        for (size i = 0; i < fl::fl_min(static_cast<size>(4), fft.bins_raw.size()); i++) {
            bassEnergy += fft.bins_raw[i];
        }
        bassEnergy /= 4.0f;

        // Calculate accent strength
        float accent = calculateBeatAccent(fft, bassEnergy);

        // Store accent in history
        if (mBeatAccents.size() >= MAX_BEAT_HISTORY) {
            mBeatAccents.erase(mBeatAccents.begin());
        }
        mBeatAccents.push_back(accent);

        // Detect downbeat
        mDownbeatDetected = detectDownbeat(timestamp, accent);

        if (mDownbeatDetected) {
            mCurrentBeat = 1;
            mBeatsSinceDownbeat = 0;
            mLastDownbeatTime = timestamp;

            // Fire callbacks
            if (onDownbeat) {
                onDownbeat();
            }
            if (onMeasureBeat) {
                onMeasureBeat(1);
            }

            // Attempt meter detection if enabled
            if (mAutoMeterDetection && !mManualMeter) {
                detectMeter();
            }
        } else {
            // Not a downbeat, increment beat counter
            mBeatsSinceDownbeat++;
            mCurrentBeat = (mBeatsSinceDownbeat % mBeatsPerMeasure) + 1;

            // Fire beat callback
            if (onMeasureBeat) {
                onMeasureBeat(mCurrentBeat);
            }

            // Check if we should force a downbeat (measure boundary)
            if (mBeatsSinceDownbeat >= mBeatsPerMeasure) {
                // Force downbeat on measure boundary
                mDownbeatDetected = true;
                mCurrentBeat = 1;
                mBeatsSinceDownbeat = 0;
                mLastDownbeatTime = timestamp;

                if (onDownbeat) {
                    onDownbeat();
                }
            }
        }

        mLastBeatTime = timestamp;
        mPreviousEnergy = bassEnergy;
    }

    // Update measure phase
    updateMeasurePhase(timestamp);
    if (onMeasurePhase) {
        onMeasurePhase(mMeasurePhase);
    }
}

void DownbeatDetector::reset() {
    mDownbeatDetected = false;
    mCurrentBeat = 1;
    mBeatsPerMeasure = 4;
    mMeasurePhase = 0.0f;
    mConfidence = 0.0f;
    mLastDownbeatTime = 0;
    mLastBeatTime = 0;
    mBeatsSinceDownbeat = 0;
    mPreviousEnergy = 0.0f;
    mBeatAccents.clear();
    mMeterCandidates.clear();
    mManualMeter = false;

    if (mOwnsBeatDetector && mBeatDetector) {
        mBeatDetector->reset();
    }
}

void DownbeatDetector::setTimeSignature(u8 beatsPerMeasure) {
    if (beatsPerMeasure >= 2 && beatsPerMeasure <= 16) {
        u8 oldMeter = mBeatsPerMeasure;
        mBeatsPerMeasure = beatsPerMeasure;
        mManualMeter = true;
        mAutoMeterDetection = false;

        // Fire meter change callback if changed
        if (oldMeter != mBeatsPerMeasure && onMeterChange) {
            onMeterChange(mBeatsPerMeasure);
        }

        // Reset beat counter to avoid invalid state
        mCurrentBeat = 1;
        mBeatsSinceDownbeat = 0;
    }
}

void DownbeatDetector::setBeatDetector(shared_ptr<BeatDetector> beatDetector) {
    if (beatDetector) {
        mBeatDetector = beatDetector;
        mOwnsBeatDetector = false;
    }
}

void DownbeatDetector::updateBeatDetector(shared_ptr<AudioContext> context) {
    if (mBeatDetector) {
        mBeatDetector->update(context);
    }
}

float DownbeatDetector::calculateBeatAccent(const FFTBins& fft, float bassEnergy) {
    // Accent detection combines multiple factors:
    // 1. Energy increase (stronger accent = more energy)
    // 2. Bass energy (downbeats typically have more bass)
    // 3. Spectral flux (onset strength)

    // Energy increase relative to previous beat
    float energyRatio = 1.0f;
    if (mPreviousEnergy > 1e-6f) {
        energyRatio = bassEnergy / mPreviousEnergy;
    }

    // Calculate overall energy
    float totalEnergy = 0.0f;
    for (size i = 0; i < fft.bins_raw.size(); i++) {
        totalEnergy += fft.bins_raw[i];
    }
    totalEnergy /= static_cast<float>(fft.bins_raw.size());

    // Bass ratio (downbeats typically have relatively more bass)
    float bassRatio = 1.0f;
    if (totalEnergy > 1e-6f) {
        bassRatio = bassEnergy / totalEnergy;
    }

    // Combine factors (weighted average)
    float accent = (energyRatio * 0.4f) + (bassRatio * 0.3f) + (totalEnergy * 0.3f);

    return accent;
}

bool DownbeatDetector::detectDownbeat(u32 timestamp, float accent) {
    // Downbeat detection strategy:
    // 1. Check if we're at the expected measure boundary
    // 2. Check if accent is strong enough
    // 3. Check if pattern matches metric grouping

    // If we haven't detected any downbeats yet, consider this one
    if (mLastDownbeatTime == 0) {
        // Use accent strength for initial confidence instead of fixed 0.5
        // This provides a better estimate based on actual audio characteristics
        float meanAccent = 1.0f;
        if (!mBeatAccents.empty()) {
            float sum = 0.0f;
            for (size i = 0; i < mBeatAccents.size(); i++) {
                sum += mBeatAccents[i];
            }
            meanAccent = sum / static_cast<float>(mBeatAccents.size());
        }

        // Calculate accent-based confidence
        // If we have accent history, use it; otherwise use raw accent with moderate confidence
        float accentConfidence = meanAccent > 0.0f
            ? fl::clamp(accent / (meanAccent * mAccentThreshold), 0.0f, 1.0f)
            : fl::clamp(accent * 0.5f, 0.3f, 0.7f);  // Raw accent scaled to 30-70% range

        mConfidence = accentConfidence;
        return true;
    }

    // Calculate expected downbeat position
    u32 timeSinceDownbeat = timestamp - mLastDownbeatTime;
    float beatInterval = mBeatDetector->getBPM() > 0.0f
        ? (60000.0f / mBeatDetector->getBPM())
        : 500.0f;
    float expectedMeasureDuration = beatInterval * static_cast<float>(mBeatsPerMeasure);

    // Check if we're near expected measure boundary
    float timingError = fl::fl_abs(static_cast<float>(timeSinceDownbeat) - expectedMeasureDuration);
    float maxTimingError = beatInterval * 0.4f;  // Allow 40% timing error
    bool nearMeasureBoundary = (timingError < maxTimingError);

    // Check if accent is strong enough
    float meanAccent = 1.0f;
    if (!mBeatAccents.empty()) {
        float sum = 0.0f;
        for (size i = 0; i < mBeatAccents.size(); i++) {
            sum += mBeatAccents[i];
        }
        meanAccent = sum / static_cast<float>(mBeatAccents.size());
    }

    bool strongAccent = (accent > meanAccent * mAccentThreshold);

    // Check if we're at the beat counter boundary
    bool atBeatCounterBoundary = (mBeatsSinceDownbeat >= mBeatsPerMeasure - 1);

    // Calculate confidence
    float timingConfidence = 1.0f - (timingError / (beatInterval * 2.0f));
    timingConfidence = fl::clamp(timingConfidence, 0.0f, 1.0f);

    float accentConfidence = meanAccent > 0.0f
        ? fl::clamp(accent / (meanAccent * mAccentThreshold), 0.0f, 1.0f)
        : 0.5f;

    // Adaptive weighting: favor accent when at beat boundary (structural downbeat)
    // This improves recall for first downbeat after warm-up
    float accentWeight = atBeatCounterBoundary ? 0.7f : 0.5f;
    float timingWeight = atBeatCounterBoundary ? 0.3f : 0.5f;
    mConfidence = (timingConfidence * timingWeight) + (accentConfidence * accentWeight);

    // Additional confidence boost for structural downbeats (at beat boundary)
    // This compensates for timing uncertainties in the first few measures
    if (atBeatCounterBoundary && mConfidence < 0.6f) {
        mConfidence = fl::fl_max(mConfidence, 0.55f);  // Ensure minimum confidence at boundaries
    }

    // Determine if this is a downbeat
    bool isDownbeat = false;

    if (atBeatCounterBoundary) {
        // We're at expected measure boundary - high likelihood of downbeat
        isDownbeat = true;
    } else if (nearMeasureBoundary && strongAccent) {
        // Early/late downbeat with strong accent
        isDownbeat = (mConfidence >= mConfidenceThreshold);
    } else if (strongAccent && mBeatsSinceDownbeat == 0) {
        // Strong accent on first beat (might be downbeat)
        isDownbeat = (mConfidence >= mConfidenceThreshold);
    }

    return isDownbeat;
}

void DownbeatDetector::detectMeter() {
    // Analyze recent beat patterns to detect time signature
    // Look for recurring patterns in beat intervals and accents

    // Add current meter candidate based on beat count
    u8 detectedMeter = mBeatsPerMeasure;

    // Analyze accent patterns to infer meter
    if (mBeatAccents.size() >= 8) {
        // Look for recurring strong accents
        // Common meters: 4/4 (every 4 beats), 3/4 (every 3), 6/8 (every 6)

        size numAccents = mBeatAccents.size();
        fl::array<u8, 5> candidateMeters = {2, 3, 4, 6, 8};
        float bestScore = 0.0f;
        u8 bestMeter = 4;

        for (size m = 0; m < candidateMeters.size(); m++) {
            u8 meter = candidateMeters[m];
            float score = 0.0f;

            // Score based on accent pattern matching
            for (size i = 0; i < numAccents; i++) {
                if (i % meter == 0) {
                    // Expect strong accent at measure start
                    score += mBeatAccents[i];
                } else {
                    // Expect weaker accent elsewhere
                    score += (2.0f - mBeatAccents[i]) * 0.5f;
                }
            }

            if (score > bestScore) {
                bestScore = score;
                bestMeter = meter;
            }
        }

        detectedMeter = bestMeter;
    }

    // Add to meter history
    if (mMeterCandidates.size() >= METER_HISTORY_SIZE) {
        mMeterCandidates.erase(mMeterCandidates.begin());
    }
    mMeterCandidates.push_back(detectedMeter);

    // Find most common meter in recent history
    u8 consensusMeter = findMostCommonMeter();

    // Update meter if consensus differs and is stable
    if (consensusMeter != mBeatsPerMeasure && mMeterCandidates.size() >= METER_HISTORY_SIZE / 2) {
        (void)mBeatsPerMeasure;  // Suppress unused warning (used in comparison above)
        mBeatsPerMeasure = consensusMeter;

        // Fire meter change callback
        if (onMeterChange) {
            onMeterChange(mBeatsPerMeasure);
        }

        // Reset beat counter
        mCurrentBeat = 1;
        mBeatsSinceDownbeat = 0;
    }
}

void DownbeatDetector::updateMeasurePhase(u32 timestamp) {
    if (mLastDownbeatTime == 0) {
        mMeasurePhase = 0.0f;
        return;
    }

    u32 timeSinceDownbeat = timestamp - mLastDownbeatTime;
    float beatInterval = mBeatDetector->getBPM() > 0.0f
        ? (60000.0f / mBeatDetector->getBPM())
        : 500.0f;
    float measureDuration = beatInterval * static_cast<float>(mBeatsPerMeasure);

    if (measureDuration > 0.0f) {
        mMeasurePhase = static_cast<float>(timeSinceDownbeat) / measureDuration;

        // Wrap phase to [0, 1)
        while (mMeasurePhase >= 1.0f) {
            mMeasurePhase -= 1.0f;
        }
    } else {
        mMeasurePhase = 0.0f;
    }
}

u8 DownbeatDetector::findMostCommonMeter() const {
    if (mMeterCandidates.empty()) {
        return 4;  // Default to 4/4
    }

    // Count occurrences of each meter
    fl::array<u8, 16> counts = {};  // Support meters 2-16

    for (size i = 0; i < mMeterCandidates.size(); i++) {
        u8 meter = mMeterCandidates[i];
        if (meter >= 2 && meter < 16) {
            counts[meter]++;
        }
    }

    // Find most common
    u8 maxCount = 0;
    u8 mostCommonMeter = 4;

    for (u8 meter = 2; meter < 16; meter++) {
        if (counts[meter] > maxCount) {
            maxCount = counts[meter];
            mostCommonMeter = meter;
        }
    }

    return mostCommonMeter;
}

} // namespace fl
