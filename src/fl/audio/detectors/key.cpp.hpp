// KeyDetector implementation

#include "fl/audio/detectors/key.h"
#include "fl/audio/audio_context.h"
#include "fl/audio/fft/fft.h"
#include "fl/stl/math.h"
#include "fl/system/log.h"
#include "fl/stl/stdio.h"

namespace fl {

// Krumhansl-Schmuckler key profiles
// Based on empirical studies of Western tonal music perception
// Values represent perceptual importance of each scale degree

// Major key profile (Krumhansl & Kessler, 1982)
const float KeyDetector::MAJOR_PROFILE[12] = {
    6.35f,  // Tonic (C)
    2.23f,  // Minor 2nd (C#)
    3.48f,  // Major 2nd (D)
    2.33f,  // Minor 3rd (Eb)
    4.38f,  // Major 3rd (E)
    4.09f,  // Perfect 4th (F)
    2.52f,  // Tritone (F#)
    5.19f,  // Perfect 5th (G)
    2.39f,  // Minor 6th (Ab)
    3.66f,  // Major 6th (A)
    2.29f,  // Minor 7th (Bb)
    2.88f   // Major 7th (B)
};

// Minor key profile (Krumhansl & Kessler, 1982)
const float KeyDetector::MINOR_PROFILE[12] = {
    6.33f,  // Tonic (A)
    2.68f,  // Minor 2nd (A#)
    3.52f,  // Major 2nd (B)
    5.38f,  // Minor 3rd (C)
    2.60f,  // Major 3rd (C#)
    3.53f,  // Perfect 4th (D)
    2.54f,  // Tritone (D#)
    4.75f,  // Perfect 5th (E)
    3.98f,  // Minor 6th (F)
    2.69f,  // Major 6th (F#)
    3.34f,  // Minor 7th (G)
    3.17f   // Major 7th (G#)
};

// Note names for root note display
static const char* NOTE_NAMES[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

//------------------------------------------------------------------------------
// Key struct methods
//------------------------------------------------------------------------------

const char* Key::getRootName() const {
    if (rootNote >= 12) return "?";
    return NOTE_NAMES[rootNote];
}

void Key::getKeyName(char* buffer, size_t bufferSize) const {
    if (bufferSize < 8) return;  // Need space for "C# min\0"
    const char* root = getRootName();
    const char* quality = getQuality();
    fl::snprintf(buffer, bufferSize, "%s %s", root, quality);
}

//------------------------------------------------------------------------------
// KeyDetector implementation
//------------------------------------------------------------------------------

KeyDetector::KeyDetector()
    : mCurrentKey()
    , mPreviousKey()
    , mKeyStartTime(0)
    , mKeyActive(false)
    , mConfidenceThreshold(0.65f)
    , mMinKeyDuration(2000)
    , mAveragingFrames(8)
    , mHistoryIndex(0)
    , mHistorySize(0)
{
    // Initialize chroma history arrays
    for (int i = 0; i < 12; i++) {
        mChromaHistory[i].reserve(mAveragingFrames);
    }

    // Pre-compute profile statistics once (eliminates 24 recomputations per frame)
    initializeProfileStats();
}

KeyDetector::~KeyDetector() = default;

void KeyDetector::initializeProfileStats() {
    // Pre-compute statistics for MAJOR_PROFILE
    float majorSum = 0.0f, majorSqSum = 0.0f;
    for (int i = 0; i < 12; i++) {
        majorSum += MAJOR_PROFILE[i];
        majorSqSum += MAJOR_PROFILE[i] * MAJOR_PROFILE[i];
    }
    mMajorProfileMean = majorSum / 12.0f;
    mMajorProfileStdDev = fl::sqrtf(majorSqSum / 12.0f - mMajorProfileMean * mMajorProfileMean);

    // Pre-compute statistics for MINOR_PROFILE
    float minorSum = 0.0f, minorSqSum = 0.0f;
    for (int i = 0; i < 12; i++) {
        minorSum += MINOR_PROFILE[i];
        minorSqSum += MINOR_PROFILE[i] * MINOR_PROFILE[i];
    }
    mMinorProfileMean = minorSum / 12.0f;
    mMinorProfileStdDev = fl::sqrtf(minorSqSum / 12.0f - mMinorProfileMean * mMinorProfileMean);
}

void KeyDetector::update(shared_ptr<AudioContext> context) {
    // Get FFT data
    mRetainedFFT = context->getFFT(32);  // Use more bins for better pitch resolution
    const FFTBins& fft = *mRetainedFFT;
    u32 timestamp = context->getTimestamp();

    // Extract chroma features
    float chroma[12] = {0};
    extractChroma(fft, chroma);
    normalizeChroma(chroma);

    // Update temporal averaging
    updateChromaHistory(chroma);

    // Get averaged chroma for stable detection
    float avgChroma[12] = {0};
    getAveragedChroma(avgChroma);

    // Detect key from averaged chroma
    Key detectedKey = detectKey(avgChroma, timestamp);

    // Update key duration if same key
    if (mKeyActive && detectedKey == mCurrentKey) {
        mCurrentKey.duration = timestamp - mKeyStartTime;
    }

    // Check for key change
    if (detectedKey != mCurrentKey) {
        // Only accept key change if:
        // 1. Confidence is above threshold
        // 2. Previous key was held for minimum duration OR new key is much stronger
        bool acceptChange = false;

        if (detectedKey.confidence >= mConfidenceThreshold) {
            if (!mKeyActive) {
                // No previous key, accept new key
                acceptChange = true;
            } else if (mCurrentKey.duration >= mMinKeyDuration) {
                // Previous key held long enough, allow change
                acceptChange = true;
            } else if (detectedKey.confidence > mCurrentKey.confidence * 1.2f) {
                // New key is significantly stronger, allow early change
                acceptChange = true;
            }
        }

        if (acceptChange) {
            mPreviousKey = mCurrentKey;
            mCurrentKey = detectedKey;
            mKeyStartTime = timestamp;
            mKeyActive = true;

            FL_DBG("Key change: " << mCurrentKey.getRootName() << " "
                   << mCurrentKey.getQuality() << " (confidence: "
                   << mCurrentKey.confidence << ")");

            mFireKeyChange = true;
        }
    }

    // Check for key end (confidence drop)
    if (mKeyActive && detectedKey.confidence < mConfidenceThreshold * 0.5f) {
        FL_DBG("Key ended: " << mCurrentKey.getRootName() << " " << mCurrentKey.getQuality());

        mFireKeyEnd = true;
        mKeyActive = false;
        mCurrentKey.confidence = 0.0f;
    }

    // Set onKey flag every frame if key is active
    if (mKeyActive) mFireKey = true;
}

void KeyDetector::reset() {
    mCurrentKey = Key();
    mPreviousKey = Key();
    mKeyStartTime = 0;
    mKeyActive = false;
    mHistoryIndex = 0;
    mHistorySize = 0;

    // Clear chroma history
    for (int i = 0; i < 12; i++) {
        mChromaHistory[i].clear();
    }
}

//------------------------------------------------------------------------------
// Chroma extraction and processing
//------------------------------------------------------------------------------

void KeyDetector::extractChroma(const FFTBins& fft, float* chroma) {
    // Initialize chroma to zero
    for (int i = 0; i < 12; i++) {
        chroma[i] = 0.0f;
    }

    // Map FFT bins to pitch classes using linearly-rebinned magnitudes.
    // Linear bins give evenly-spaced frequency mapping: freq = fmin + bin * binWidth.
    fl::span<const float> linearBins = fft.linear();
    const int numBins = static_cast<int>(linearBins.size());
    const float fmin = fft.linearFmin();
    const float fmax = fft.linearFmax();
    const float linearBinWidth = (numBins > 0) ? (fmax - fmin) / static_cast<float>(numBins) : 1.0f;

    // Process each linear bin
    for (int bin = 0; bin < numBins; bin++) {
        float magnitude = linearBins[bin];
        if (magnitude < 1e-6f) continue;

        // Calculate frequency for this linearly-spaced bin
        float freq = fmin + (static_cast<float>(bin) + 0.5f) * linearBinWidth;

        // Skip frequencies below 60 Hz (below C2)
        if (freq < 60.0f) continue;

        // Convert frequency to MIDI note number
        // MIDI note = 69 + 12 * log2(freq / 440)
        float midiNote = 69.0f + 12.0f * (fl::logf(freq / 440.0f) / fl::logf(2.0f));

        // Get pitch class (0-11)
        int pitchClass = static_cast<int>(midiNote + 0.5f) % 12;
        if (pitchClass < 0) pitchClass += 12;

        // Accumulate magnitude into pitch class
        chroma[pitchClass] += magnitude;
    }
}

void KeyDetector::normalizeChroma(float* chroma) {
    // Find maximum value
    float maxVal = 0.0f;
    for (int i = 0; i < 12; i++) {
        if (chroma[i] > maxVal) {
            maxVal = chroma[i];
        }
    }

    // Normalize to 0-1 range
    if (maxVal > 1e-6f) {
        for (int i = 0; i < 12; i++) {
            chroma[i] /= maxVal;
        }
    }
}

void KeyDetector::updateChromaHistory(const float* chroma) {
    // Add current chroma to history (circular buffer)
    for (int i = 0; i < 12; i++) {
        if (static_cast<int>(mChromaHistory[i].size()) < mAveragingFrames) {
            mChromaHistory[i].push_back(chroma[i]);
        } else {
            mChromaHistory[i][mHistoryIndex] = chroma[i];
        }
    }

    mHistoryIndex = (mHistoryIndex + 1) % mAveragingFrames;
    if (mHistorySize < mAveragingFrames) {
        mHistorySize++;
    }
}

void KeyDetector::getAveragedChroma(float* chroma) {
    if (mHistorySize == 0) {
        // No history yet, return zeros
        for (int i = 0; i < 12; i++) {
            chroma[i] = 0.0f;
        }
        return;
    }

    // Average across history
    for (int i = 0; i < 12; i++) {
        float sum = 0.0f;
        for (int j = 0; j < mHistorySize; j++) {
            sum += mChromaHistory[i][j];
        }
        chroma[i] = sum / mHistorySize;
    }
}

//------------------------------------------------------------------------------
// Key detection using Krumhansl-Schmuckler algorithm
//------------------------------------------------------------------------------

Key KeyDetector::detectKey(const float* chroma, u32 timestamp) {
    float bestCorrelation = -1.0f;
    u8 bestRoot = 0;
    bool bestIsMinor = false;

    // Try all 24 possible keys (12 major + 12 minor)
    for (int root = 0; root < 12; root++) {
        // Try major key
        float majorCorr = correlateWithProfile(chroma, MAJOR_PROFILE, root);
        if (majorCorr > bestCorrelation) {
            bestCorrelation = majorCorr;
            bestRoot = root;
            bestIsMinor = false;
        }

        // Try minor key
        float minorCorr = correlateWithProfile(chroma, MINOR_PROFILE, root);
        if (minorCorr > bestCorrelation) {
            bestCorrelation = minorCorr;
            bestRoot = root;
            bestIsMinor = true;
        }
    }

    // Convert correlation to 0-1 confidence
    // Correlation ranges from -1 to 1, but we typically see 0.5-0.9 for good matches
    float confidence = (bestCorrelation + 1.0f) / 2.0f;  // Map [-1,1] to [0,1]
    confidence = fl::max(0.0f, fl::min(1.0f, confidence));

    return Key(bestRoot, bestIsMinor, confidence, timestamp);
}

float KeyDetector::correlateWithProfile(const float* chroma, const float* profile, int rootNote) {
    // Pearson correlation coefficient between chroma and rotated profile
    // Uses pre-computed profile statistics (calculated once in constructor)

    // Use pre-computed profile statistics based on which profile
    float profileMean, profileStdDev;
    if (profile == MAJOR_PROFILE) {
        profileMean = mMajorProfileMean;
        profileStdDev = mMajorProfileStdDev;
    } else {
        profileMean = mMinorProfileMean;
        profileStdDev = mMinorProfileStdDev;
    }
    if (profileStdDev < 1e-6f) profileStdDev = 1.0f;

    // Calculate chroma mean and std dev
    float chromaSum = 0.0f;
    float chromaSqSum = 0.0f;
    for (int i = 0; i < 12; i++) {
        chromaSum += chroma[i];
        chromaSqSum += chroma[i] * chroma[i];
    }
    float chromaMean = chromaSum / 12.0f;
    float chromaStdDev = sqrtf((chromaSqSum / 12.0f) - (chromaMean * chromaMean));
    if (chromaStdDev < 1e-6f) chromaStdDev = 1.0f;

    // Calculate correlation with rotated profile
    float correlation = 0.0f;
    for (int i = 0; i < 12; i++) {
        int profileIdx = (i - rootNote + 12) % 12;  // Rotate profile to match root
        float chromaNorm = (chroma[i] - chromaMean) / chromaStdDev;
        float profileNorm = (profile[profileIdx] - profileMean) / profileStdDev;
        correlation += chromaNorm * profileNorm;
    }
    correlation /= 12.0f;  // Average

    return correlation;
}

void KeyDetector::fireCallbacks() {
    if (mFireKeyEnd) {
        if (onKeyEnd) onKeyEnd();
        mFireKeyEnd = false;
    }
    if (mFireKeyChange) {
        if (onKeyChange) onKeyChange(mCurrentKey);
        mFireKeyChange = false;
    }
    if (mFireKey) {
        if (onKey) onKey(mCurrentKey);
        mFireKey = false;
    }
}

} // namespace fl
