#include "fl/fx/audio/detectors/chord.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/math.h"
#include "fl/dbg.h"

namespace fl {

// Chord templates (intervals relative to root)
// Major: root, major third (4 semitones), perfect fifth (7 semitones)
// Minor: root, minor third (3 semitones), perfect fifth (7 semitones)
// etc.

struct ChordTemplate {
    ChordType type;
    int intervals[5];  // Relative semitone intervals, -1 = unused
    int numNotes;
};

static const ChordTemplate kChordTemplates[] = {
    {ChordType::MAJOR,      {0, 4, 7, -1, -1}, 3},
    {ChordType::MINOR,      {0, 3, 7, -1, -1}, 3},
    {ChordType::DIMINISHED, {0, 3, 6, -1, -1}, 3},
    {ChordType::AUGMENTED,  {0, 4, 8, -1, -1}, 3},
    {ChordType::MAJOR7,     {0, 4, 7, 11, -1}, 4},
    {ChordType::MINOR7,     {0, 3, 7, 10, -1}, 4},
    {ChordType::DOMINANT7,  {0, 4, 7, 10, -1}, 4},
    {ChordType::SUSPENDED2, {0, 2, 7, -1, -1}, 3},
    {ChordType::SUSPENDED4, {0, 5, 7, -1, -1}, 3},
};
static const int kNumChordTemplates = sizeof(kChordTemplates) / sizeof(ChordTemplate);

ChordDetector::ChordDetector()
    : mChordStartTime(0)
    , mChordEndTime(0)
    , mConfidenceThreshold(0.6f)
    , mMinChordDuration(200)  // 200ms minimum chord duration
{
    for (int i = 0; i < 12; i++) {
        mChroma[i] = 0.0f;
        mPrevChroma[i] = 0.0f;
    }
}

ChordDetector::~ChordDetector() = default;

void ChordDetector::update(shared_ptr<AudioContext> context) {
    const FFTBins& fft = context->getFFT(32);  // Higher resolution for pitch detection
    uint32_t timestamp = context->getTimestamp();

    // Calculate chroma features
    calculateChroma(fft);

    // Detect current chord
    Chord detected = detectChord(mChroma, timestamp);

    // Check if chord changed
    if (detected.isValid() && detected.confidence >= mConfidenceThreshold) {
        if (!mCurrentChord.isValid() || !isSimilarChord(detected, mCurrentChord)) {
            // New chord detected
            if (mCurrentChord.isValid() && onChordEnd) {
                onChordEnd();
            }

            mPreviousChord = mCurrentChord;
            mCurrentChord = detected;
            mChordStartTime = timestamp;

            if (onChordChange) {
                onChordChange(mCurrentChord);
            }

            FL_DBG("Chord detected: " << mCurrentChord.getRootName()
                   << mCurrentChord.getTypeName()
                   << " (conf: " << mCurrentChord.confidence << ")");
        } else {
            // Same chord, update confidence
            mCurrentChord.confidence = detected.confidence;
            mCurrentChord.timestamp = timestamp;
        }

        // Fire onChord callback every frame when chord is active
        if (onChord) {
            onChord(mCurrentChord);
        }
    } else {
        // No valid chord or low confidence
        if (mCurrentChord.isValid()) {
            uint32_t duration = timestamp - mChordStartTime;
            if (duration >= mMinChordDuration) {
                mChordEndTime = timestamp;
                if (onChordEnd) {
                    onChordEnd();
                }
            }
            mCurrentChord = Chord();  // Reset to invalid
        }
    }

    // Save chroma for next frame
    for (int i = 0; i < 12; i++) {
        mPrevChroma[i] = mChroma[i];
    }
}

void ChordDetector::reset() {
    mCurrentChord = Chord();
    mPreviousChord = Chord();
    mChordStartTime = 0;
    mChordEndTime = 0;
    for (int i = 0; i < 12; i++) {
        mChroma[i] = 0.0f;
        mPrevChroma[i] = 0.0f;
    }
}

void ChordDetector::calculateChroma(const FFTBins& fft) {
    // Clear chroma
    for (int i = 0; i < 12; i++) {
        mChroma[i] = 0.0f;
    }

    // Map FFT bins to pitch classes (chroma)
    // Assuming 44100Hz sample rate and 512-1024 sample FFT
    // Each FFT bin represents a frequency range
    // We need to map frequencies to musical notes (12-tone equal temperament)

    const float sampleRate = 44100.0f;
    const int fftSize = 1024;  // Assumed FFT size
    const fl::size numBins = fft.size();

    for (fl::size binIdx = 0; binIdx < numBins; binIdx++) {
        float magnitude = fft.bins_raw[binIdx];
        if (magnitude < 1e-6f) continue;

        // Calculate frequency of this bin
        float freq = (static_cast<float>(binIdx) * sampleRate) / static_cast<float>(fftSize);

        // Skip frequencies below 60Hz (too low for chord detection)
        if (freq < 60.0f) continue;

        // Convert frequency to MIDI note number (A4 = 440Hz = MIDI 69)
        float midiNote = 69.0f + 12.0f * (fl::logf(freq / 440.0f) / fl::logf(2.0f));

        // Get pitch class (0-11)
        int pitchClass = static_cast<int>(midiNote + 0.5f) % 12;
        if (pitchClass < 0) pitchClass += 12;

        // Accumulate magnitude into chroma
        mChroma[pitchClass] += magnitude;
    }

    // Normalize chroma
    normalizeChroma(mChroma);
}

Chord ChordDetector::detectChord(const float* chroma, uint32_t timestamp) {
    float bestScore = 0.0f;
    int bestRoot = -1;
    ChordType bestType = ChordType::UNKNOWN;

    // Try all root notes (0-11)
    for (int root = 0; root < 12; root++) {
        // Try all chord templates
        for (int t = 0; t < kNumChordTemplates; t++) {
            float score = matchChordPattern(chroma, root, kChordTemplates[t].type);
            if (score > bestScore) {
                bestScore = score;
                bestRoot = root;
                bestType = kChordTemplates[t].type;
            }
        }
    }

    // Confidence threshold
    if (bestScore < 0.3f) {
        return Chord();  // No valid chord
    }

    return Chord(bestRoot, bestType, bestScore, timestamp);
}

float ChordDetector::matchChordPattern(const float* chroma, int root, ChordType type) {
    // Find the matching template
    const ChordTemplate* tmpl = nullptr;
    for (int i = 0; i < kNumChordTemplates; i++) {
        if (kChordTemplates[i].type == type) {
            tmpl = &kChordTemplates[i];
            break;
        }
    }
    if (!tmpl) return 0.0f;

    // Calculate match score
    float matchScore = 0.0f;
    float totalChroma = 0.0f;

    // Sum chroma energy for chord notes
    for (int i = 0; i < tmpl->numNotes; i++) {
        int interval = tmpl->intervals[i];
        if (interval < 0) break;
        int pitchClass = (root + interval) % 12;
        matchScore += chroma[pitchClass];
    }

    // Sum total chroma energy
    for (int i = 0; i < 12; i++) {
        totalChroma += chroma[i];
    }

    // Penalize energy in non-chord notes
    float nonChordEnergy = totalChroma - matchScore;

    // Score is ratio of chord notes to total energy, minus non-chord penalty
    float score = 0.0f;
    if (totalChroma > 1e-6f) {
        score = matchScore / totalChroma;
        score -= 0.3f * (nonChordEnergy / totalChroma);
        score = fl::fl_max(0.0f, score);
    }

    return score;
}

bool ChordDetector::isSimilarChord(const Chord& a, const Chord& b) {
    if (!a.isValid() || !b.isValid()) return false;

    // Same root and type = similar
    if (a.rootNote == b.rootNote && a.type == b.type) {
        return true;
    }

    // Check for enharmonic equivalents or closely related chords
    // For now, just exact match
    return false;
}

void ChordDetector::normalizeChroma(float* chroma) {
    // Find max value
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

float ChordDetector::chromaDistance(const float* a, const float* b) {
    float dist = 0.0f;
    for (int i = 0; i < 12; i++) {
        float diff = a[i] - b[i];
        dist += diff * diff;
    }
    return fl::sqrt(dist);
}

// Out-of-line definitions for Chord methods (needed for linking with unity builds)
const char* Chord::getRootName() const {
    static const char* noteNames[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    return (rootNote >= 0 && rootNote < 12) ? noteNames[rootNote] : "?";
}

const char* Chord::getTypeName() const {
    switch (type) {
        case ChordType::MAJOR: return "maj";
        case ChordType::MINOR: return "min";
        case ChordType::DIMINISHED: return "dim";
        case ChordType::AUGMENTED: return "aug";
        case ChordType::MAJOR7: return "maj7";
        case ChordType::MINOR7: return "min7";
        case ChordType::DOMINANT7: return "7";
        case ChordType::SUSPENDED2: return "sus2";
        case ChordType::SUSPENDED4: return "sus4";
        default: return "?";
    }
}

} // namespace fl
