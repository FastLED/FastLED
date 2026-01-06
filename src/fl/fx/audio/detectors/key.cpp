// KeyDetector implementation

#include "fl/fx/audio/detectors/key.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/math.h"
#include "fl/dbg.h"
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
}

KeyDetector::~KeyDetector() = default;

void KeyDetector::update(shared_ptr<AudioContext> context) {
    // Get FFT data
    const FFTBins& fft = context->getFFT(32);  // Use more bins for better pitch resolution
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

            fireCallbacks(mCurrentKey, timestamp);
        }
    }

    // Check for key end (confidence drop)
    if (mKeyActive && detectedKey.confidence < mConfidenceThreshold * 0.5f) {
        FL_DBG("Key ended: " << mCurrentKey.getRootName() << " " << mCurrentKey.getQuality());

        if (onKeyEnd) {
            onKeyEnd();
        }
        mKeyActive = false;
        mCurrentKey.confidence = 0.0f;
    }

    // Fire onKey callback every frame if key is active
    if (mKeyActive && onKey) {
        onKey(mCurrentKey);
    }
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

    // Map FFT bins to pitch classes
    // Assume 44100 Hz sample rate and 512-point FFT
    const float sampleRate = 44100.0f;
    const int fftSize = 512;
    const float binWidth = sampleRate / fftSize;

    // Process each FFT bin
    for (size_t bin = 0; bin < fft.bins_raw.size(); bin++) {
        float magnitude = fft.bins_raw[bin];
        if (magnitude < 1e-6f) continue;

        // Calculate frequency for this bin
        float freq = bin * binWidth;

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
    confidence = fl::fl_max(0.0f, fl::fl_min(1.0f, confidence));

    return Key(bestRoot, bestIsMinor, confidence, timestamp);
}

float KeyDetector::correlateWithProfile(const float* chroma, const float* profile, int rootNote) {
    // Pearson correlation coefficient between chroma and rotated profile

    // First, normalize the profile weights
    float profileSum = 0.0f;
    float profileSqSum = 0.0f;
    for (int i = 0; i < 12; i++) {
        profileSum += profile[i];
        profileSqSum += profile[i] * profile[i];
    }
    float profileMean = profileSum / 12.0f;
    float profileStdDev = sqrtf((profileSqSum / 12.0f) - (profileMean * profileMean));
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

void KeyDetector::fireCallbacks(const Key& key, u32 /*timestamp*/) {
    // Fire onKeyChange if key actually changed
    if (mPreviousKey != key || !mKeyActive) {
        if (onKeyChange) {
            onKeyChange(key);
        }
    }
}

} // namespace fl
