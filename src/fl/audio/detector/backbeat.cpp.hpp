#include "fl/audio/detector/backbeat.h"
#include "fl/audio/audio_context.h"
#include "fl/math/math.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {
namespace detector {

Backbeat::Backbeat(shared_ptr<Beat> beatDetector)
    : mBeatDetector(beatDetector)
    , mDownbeatDetector(nullptr)
    , mOwnsBeatDetector(false)
    , mOwnsDownbeatDetector(false)
    , mBackbeatDetected(false)
    , mLastBackbeatNumber(0)
    , mConfidence(0.0f)
    , mCurrentStrength(0.0f)
    , mBackbeatRatio(1.0f)
    , mConfidenceThreshold(0.6f)
    , mBassThreshold(1.2f)
    , mMidThreshold(1.3f)
    , mHighThreshold(1.1f)
    , mBackbeatMask(0x0A)  // Bits 1 and 3 = beats 2 and 4 in 4/4
    , mAdaptive(true)
    , mCurrentBeat(1)
    , mBeatsPerMeasure(4)
    , mPreviousWasBeat(false)
    , mBackbeatMean(1.0f)
    , mNonBackbeatMean(0.8f)
    , mAdaptiveThreshold(1.0f)
    , mProfileAlpha(0.1f)
{
    mPreviousAccent = {0.0f, 0.0f, 0.0f, 0.0f};
    mBackbeatSpectralProfile.reserve(SPECTRAL_PROFILE_SIZE);
    for (size i = 0; i < SPECTRAL_PROFILE_SIZE; i++) {
        mBackbeatSpectralProfile.push_back(0.0f);
    }
}

Backbeat::Backbeat(shared_ptr<Beat> beatDetector,
                                    shared_ptr<Downbeat> downbeatDetector)
    : Backbeat(beatDetector)
{
    mDownbeatDetector = downbeatDetector;
    mOwnsDownbeatDetector = false;
}

Backbeat::Backbeat()
    : Backbeat(make_shared<Beat>())
{
    mOwnsBeatDetector = true;
}

Backbeat::~Backbeat() FL_NOEXCEPT = default;

void Backbeat::update(shared_ptr<Context> context) {
    // Update Beat if we own it
    if (mOwnsBeatDetector && mBeatDetector) {
        updateBeatDetector(context);
    }

    // Update Downbeat if we own it
    if (mOwnsDownbeatDetector && mDownbeatDetector) {
        mDownbeatDetector->update(context);
    }

    // Reset backbeat flag
    mBackbeatDetected = false;

    // Check if a beat occurred
    bool beatDetected = mBeatDetector && mBeatDetector->isBeat();

    // Update beat position tracking
    updateBeatPosition();

    // Process beat if detected
    if (beatDetected) {
        // Get fft::FFT for multi-band analysis
        mRetainedFFT = context->getFFT16();
        const fft::Bins& fft = *mRetainedFFT;

        // Calculate multi-band accent
        MultibandAccent accent = calculateMultibandAccent(fft);

        // Detect backbeat accent strength
        float accentStrength = detectBackbeatAccent(accent);
        mCurrentStrength = accentStrength;

        // Detect backbeat
        mBackbeatDetected = detectBackbeat(accentStrength, fft);

        if (mBackbeatDetected) {
            mLastBackbeatNumber = mCurrentBeat;

            // Update backbeat profile
            if (mAdaptive) {
                updateBackbeatProfile(fft);
            }

            // Store accent in backbeat history
            if (mBackbeatAccents.size() >= MAX_ACCENT_HISTORY) {
                mBackbeatAccents.pop_front();
            }
            mBackbeatAccents.push_back(accentStrength);
        } else {
            // Store accent in non-backbeat history
            if (mNonBackbeatAccents.size() >= MAX_ACCENT_HISTORY) {
                mNonBackbeatAccents.pop_front();
            }
            mNonBackbeatAccents.push_back(accentStrength);
        }

        // Update adaptive thresholds
        if (mAdaptive) {
            updateAdaptiveThresholds();
        }

        // Store current accent for next frame
        mPreviousAccent = accent;
    }
}

void Backbeat::fireCallbacks() {
    if (mBackbeatDetected) {
        if (onBackbeat) {
            onBackbeat(mCurrentBeat, mConfidence, mCurrentStrength);
        }
    }
}

void Backbeat::reset() {
    mBackbeatDetected = false;
    mLastBackbeatNumber = 0;
    mConfidence = 0.0f;
    mCurrentStrength = 0.0f;
    mBackbeatRatio = 1.0f;
    mCurrentBeat = 1;
    mBeatsPerMeasure = 4;
    mPreviousWasBeat = false;
    mPreviousAccent = {0.0f, 0.0f, 0.0f, 0.0f};
    mBackbeatAccents.clear();
    mNonBackbeatAccents.clear();
    mBackbeatMean = 1.0f;
    mNonBackbeatMean = 0.8f;
    mAdaptiveThreshold = 1.0f;

    for (size i = 0; i < mBackbeatSpectralProfile.size(); i++) {
        mBackbeatSpectralProfile[i] = 0.0f;
    }

    mRetainedFFT.reset();

    if (mOwnsBeatDetector && mBeatDetector) {
        mBeatDetector->reset();
    }
    if (mOwnsDownbeatDetector && mDownbeatDetector) {
        mDownbeatDetector->reset();
    }
}

void Backbeat::setBeatDetector(shared_ptr<Beat> beatDetector) {
    if (beatDetector) {
        mBeatDetector = beatDetector;
        mOwnsBeatDetector = false;
    }
}

void Backbeat::setDownbeatDetector(shared_ptr<Downbeat> downbeatDetector) {
    if (downbeatDetector) {
        mDownbeatDetector = downbeatDetector;
        mOwnsDownbeatDetector = false;
    }
}

void Backbeat::updateBeatDetector(shared_ptr<Context> context) {
    if (mBeatDetector) {
        mBeatDetector->update(context);
    }
}

void Backbeat::updateBeatPosition() {
    if (!mBeatDetector) {
        return;
    }

    bool currentlyBeat = mBeatDetector->isBeat();

    // Detect beat transition (rising edge)
    if (currentlyBeat && !mPreviousWasBeat) {
        // Beat just occurred, advance position
        if (mDownbeatDetector) {
            // Use Downbeat for accurate position
            mCurrentBeat = mDownbeatDetector->getCurrentBeat();
            mBeatsPerMeasure = mDownbeatDetector->getBeatsPerMeasure();
        } else {
            // Standalone mode: assume 4/4 and cycle
            mCurrentBeat++;
            if (mCurrentBeat > mBeatsPerMeasure) {
                mCurrentBeat = 1;
            }
        }
    }

    mPreviousWasBeat = currentlyBeat;
}

MultibandAccent Backbeat::calculateMultibandAccent(const fft::Bins& fft) {
    MultibandAccent accent;

    // Define band ranges for 16-bin CQ log-spaced fft::FFT
    // Bass: bins 0-3 (~175-400 Hz) - kick drum fundamentals
    // Mid: bins 4-10 (~460-2100 Hz) - snare fundamental and harmonics
    // High: bins 11-15 (~2450-4698 Hz) - hi-hats, cymbals

    size numBins = fft.raw().size();
    if (numBins == 0) {
        accent = {0.0f, 0.0f, 0.0f, 0.0f};
        return accent;
    }

    // Calculate current energy for each band
    float bassEnergy = 0.0f;
    float midEnergy = 0.0f;
    float highEnergy = 0.0f;

    // Adjust band ranges based on actual bin count
    size bassEnd = fl::min(static_cast<size>(4), numBins);
    size midStart = bassEnd;
    size midEnd = fl::min(static_cast<size>(11), numBins);
    size highStart = midEnd;
    size highEnd = numBins;

    // Sum energy in each band
    for (size i = 0; i < bassEnd; i++) {
        bassEnergy += fft.raw()[i];
    }
    for (size i = midStart; i < midEnd; i++) {
        midEnergy += fft.raw()[i];
    }
    for (size i = highStart; i < highEnd; i++) {
        highEnergy += fft.raw()[i];
    }

    // Normalize by bin count
    bassEnergy /= static_cast<float>(bassEnd);
    if (midEnd > midStart) {
        midEnergy /= static_cast<float>(midEnd - midStart);
    }
    if (highEnd > highStart) {
        highEnergy /= static_cast<float>(highEnd - highStart);
    }

    // Calculate accent strength for each band (current vs previous)
    // Using logarithmic ratio with normalization
    const float epsilon = 1e-6f;
    const float maxRatio = 10.0f;  // Maximum expected energy increase

    float bassRatio = (mPreviousAccent.bass > epsilon)
        ? (bassEnergy / mPreviousAccent.bass)
        : 1.0f;
    float midRatio = (mPreviousAccent.mid > epsilon)
        ? (midEnergy / mPreviousAccent.mid)
        : 1.0f;
    float highRatio = (mPreviousAccent.high > epsilon)
        ? (highEnergy / mPreviousAccent.high)
        : 1.0f;

    // Apply logarithmic scaling and normalize to 0-1 range
    float logMaxRatio = fl::log10f(1.0f + maxRatio);
    accent.bass = fl::clamp(fl::log10f(1.0f + bassRatio) / logMaxRatio, 0.0f, 1.0f);
    accent.mid = fl::clamp(fl::log10f(1.0f + midRatio) / logMaxRatio, 0.0f, 1.0f);
    accent.high = fl::clamp(fl::log10f(1.0f + highRatio) / logMaxRatio, 0.0f, 1.0f);

    // Store raw energies for next comparison (not normalized accents)
    accent.bass = bassEnergy;
    accent.mid = midEnergy;
    accent.high = highEnergy;

    // Actually calculate accents properly
    float bassAccent = fl::clamp(fl::log10f(1.0f + bassRatio) / logMaxRatio, 0.0f, 1.0f);
    float midAccent = fl::clamp(fl::log10f(1.0f + midRatio) / logMaxRatio, 0.0f, 1.0f);
    float highAccent = fl::clamp(fl::log10f(1.0f + highRatio) / logMaxRatio, 0.0f, 1.0f);

    // Weighted combination emphasizing mid-range (snare)
    // Bass: 0.3, Mid: 0.5, High: 0.2
    accent.total = (bassAccent * 0.3f) + (midAccent * 0.5f) + (highAccent * 0.2f);

    // Return struct with raw energies stored for next frame
    return {bassEnergy, midEnergy, highEnergy, accent.total};
}

float Backbeat::detectBackbeatAccent(const MultibandAccent& accent) {
    // The accent strength is already calculated in calculateMultibandAccent
    // This method could apply additional processing if needed
    return accent.total;
}

bool Backbeat::isBackbeatPosition() const {
    // Check if current beat number matches backbeat mask
    // Beat numbers are 1-based, mask is 0-based
    if (mCurrentBeat == 0 || mCurrentBeat > 8) {
        return false;  // Invalid beat number
    }

    u8 bitIndex = mCurrentBeat - 1;  // Convert to 0-based
    return (mBackbeatMask & (1 << bitIndex)) != 0;
}

bool Backbeat::detectBackbeat(float accentStrength, const fft::Bins& fft) {
    // Check if we're at a backbeat position
    bool atBackbeatPosition = isBackbeatPosition();

    if (!atBackbeatPosition) {
        mConfidence = 0.0f;
        return false;
    }

    // Calculate accent confidence (relative to adaptive threshold)
    float accentConfidence = 0.0f;
    if (mAdaptive && mBackbeatMean > mNonBackbeatMean) {
        // Compare to learned distribution
        float separation = mBackbeatMean - mNonBackbeatMean;
        if (separation > 1e-6f) {
            accentConfidence = (accentStrength - mNonBackbeatMean) / separation;
            accentConfidence = fl::clamp(accentConfidence, 0.0f, 1.0f);
        }
    } else {
        // Use fixed threshold
        accentConfidence = (accentStrength >= mAdaptiveThreshold) ? 1.0f : 0.0f;
    }

    // Position confidence (we're at backbeat position)
    float positionConfidence = 1.0f;

    // Pattern confidence (spectral similarity to learned profile)
    float patternConfidence = calculatePatternConfidence(fft);

    // Combined confidence
    // Weights: accent 40%, position 30%, pattern 30%
    mConfidence = (accentConfidence * 0.4f) +
                  (positionConfidence * 0.3f) +
                  (patternConfidence * 0.3f);

    // Detect backbeat if confidence exceeds threshold
    return (mConfidence >= mConfidenceThreshold);
}

void Backbeat::updateAdaptiveThresholds() {
    // Update mean accent strengths for backbeat and non-backbeat positions
    if (!mBackbeatAccents.empty()) {
        float sum = 0.0f;
        for (size i = 0; i < mBackbeatAccents.size(); i++) {
            sum += mBackbeatAccents[i];
        }
        mBackbeatMean = sum / static_cast<float>(mBackbeatAccents.size());
    }

    if (!mNonBackbeatAccents.empty()) {
        float sum = 0.0f;
        for (size i = 0; i < mNonBackbeatAccents.size(); i++) {
            sum += mNonBackbeatAccents[i];
        }
        mNonBackbeatMean = sum / static_cast<float>(mNonBackbeatAccents.size());
    }

    // Adaptive threshold is midpoint between means
    if (mBackbeatMean > mNonBackbeatMean) {
        mAdaptiveThreshold = (mBackbeatMean + mNonBackbeatMean) / 2.0f;
    }

    // Calculate backbeat ratio (for state access)
    if (mNonBackbeatMean > 1e-6f) {
        mBackbeatRatio = mBackbeatMean / mNonBackbeatMean;
    } else {
        mBackbeatRatio = 1.0f;
    }
}

void Backbeat::updateBackbeatProfile(const fft::Bins& fft) {
    // Update spectral profile using exponential moving average
    // This learns the typical frequency content of backbeats

    size profileSize = fl::min(mBackbeatSpectralProfile.size(), fft.raw().size());

    for (size i = 0; i < profileSize; i++) {
        // EMA: profile = alpha * new + (1 - alpha) * old
        mBackbeatSpectralProfile[i] =
            (mProfileAlpha * fft.raw()[i]) +
            ((1.0f - mProfileAlpha) * mBackbeatSpectralProfile[i]);
    }
}

float Backbeat::calculatePatternConfidence(const fft::Bins& fft) {
    // Calculate spectral correlation between current spectrum and learned profile
    // Returns 0-1 indicating how well current spectrum matches typical backbeat

    // If profile is not yet learned, return neutral confidence
    bool profileLearned = false;
    for (size i = 0; i < mBackbeatSpectralProfile.size(); i++) {
        if (mBackbeatSpectralProfile[i] > 1e-6f) {
            profileLearned = true;
            break;
        }
    }

    if (!profileLearned) {
        return 0.5f;  // Neutral confidence
    }

    // Calculate normalized correlation
    size compareSize = fl::min(mBackbeatSpectralProfile.size(), fft.raw().size());

    float dotProduct = 0.0f;
    float profileMag = 0.0f;
    float currentMag = 0.0f;

    for (size i = 0; i < compareSize; i++) {
        dotProduct += mBackbeatSpectralProfile[i] * fft.raw()[i];
        profileMag += mBackbeatSpectralProfile[i] * mBackbeatSpectralProfile[i];
        currentMag += fft.raw()[i] * fft.raw()[i];
    }

    // Cosine similarity
    const float epsilon = 1e-6f;
    float denominator = fl::sqrt(profileMag * currentMag);

    if (denominator < epsilon) {
        return 0.5f;  // Neutral if magnitudes too small
    }

    float correlation = dotProduct / denominator;

    // Clamp to 0-1 range
    return fl::clamp(correlation, 0.0f, 1.0f);
}

} // namespace detector
} // namespace audio
} // namespace fl
