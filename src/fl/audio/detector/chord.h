#pragma once

#include "fl/audio/audio_detector.h"
#include "fl/stl/function.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/flat_map.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace audio {
namespace detector {

// Chord types supported by the detector
enum class ChordType {
    MAJOR,
    MINOR,
    DIMINISHED,
    AUGMENTED,
    MAJOR7,
    MINOR7,
    DOMINANT7,
    SUSPENDED2,
    SUSPENDED4,
    UNKNOWN
};

// Forward declaration of chord template structure (defined in .cpp.hpp)
struct ChordTemplate;

// Detected chord information
struct Chord {
    int rootNote;           // MIDI note number (0-11 for C-B)
    ChordType type;         // Chord type
    float confidence;       // Detection confidence (0.0-1.0)
    u32 timestamp;     // When detected

    Chord() FL_NOEXCEPT : rootNote(-1), type(ChordType::UNKNOWN), confidence(0.0f), timestamp(0) {}
    Chord(int root, ChordType t, float conf, u32 ts)
        : rootNote(root), type(t), confidence(conf), timestamp(ts) {}

    bool isValid() const { return rootNote >= 0 && rootNote < 12; }

    // Declared here, defined in chord.cpp
    const char* getRootName() const;
    const char* getTypeName() const;
};

class ChordDetector : public Detector {
public:
    ChordDetector() FL_NOEXCEPT;
    ~ChordDetector() FL_NOEXCEPT override;

    void update(shared_ptr<Context> context) override;
    void fireCallbacks() override;
    bool needsFFT() const override { return true; }
    bool needsFFTHistory() const override { return true; }
    const char* getName() const override { return "ChordDetector"; }
    void reset() override;

    // Callbacks (multiple listeners supported)
    function_list<void(const Chord& chord)> onChord;
    function_list<void(const Chord& chord)> onChordChange;
    function_list<void()> onChordEnd;

    // State access
    const Chord& getCurrentChord() const { return mCurrentChord; }
    bool hasChord() const { return mCurrentChord.isValid(); }

    // Configuration
    void setConfidenceThreshold(float threshold) { mConfidenceThreshold = threshold; }
    void setMinDuration(u32 ms) { mMinChordDuration = ms; }

private:
    // Current state
    Chord mCurrentChord;
    Chord mPreviousChord;
    u32 mChordStartTime;
    u32 mChordEndTime;

    // Configuration
    float mConfidenceThreshold;
    u32 mMinChordDuration;

    // Chroma feature (pitch class profile)
    float mChroma[12];  // Energy for each pitch class (C, C#, D, ...)
    float mPrevChroma[12];

    bool mFireChordChange = false;
    bool mFireChordEnd = false;
    bool mFireChord = false;

    // Template lookup map (O(1) access, pre-computed at init)
    // Maps ChordType (as int) to ChordTemplate pointer for fast template lookups
    flat_map<int, const ChordTemplate*> mTemplateMap;

    shared_ptr<const fft::Bins> mRetainedFFT;

    // Detection methods
    void initializeTemplateMap();  // Pre-compute template lookups
    void calculateChroma(const fft::Bins& fft);
    Chord detectChord(const float* chroma, u32 timestamp);
    float matchChordPattern(const float* chroma, int root, ChordType type);
    bool isSimilarChord(const Chord& a, const Chord& b);

    // Helper methods
    void normalizeChroma(float* chroma);
    float chromaDistance(const float* a, const float* b);
};

} // namespace detector
} // namespace audio
} // namespace fl
