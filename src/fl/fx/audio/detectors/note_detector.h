#pragma once

#include "fl/stl/memory.h"
#include "fl/audio/audio_context.h"
#include "fl/fx/audio/detectors/pitch.h"

namespace fl {

class NoteDetector {
public:
    // Constants
    static constexpr uint8_t NO_NOTE = 255;
    static constexpr float A4_FREQUENCY = 440.0f;
    static constexpr uint8_t A4_MIDI_NOTE = 69;

    // Constructors
    NoteDetector();
    explicit NoteDetector(shared_ptr<PitchDetector> pitchDetector);
    ~NoteDetector();

    // Update and reset
    void update(shared_ptr<AudioContext> context);
    void reset();

    // Callback types
    using NoteVelocityCallback = void(*)(uint8_t note);
    using NoteStartCallback = void(*)(uint8_t note, uint8_t velocity);

    // Note-related callback functions
    NoteStartCallback onNoteOn = nullptr;      // Triggered on new note start with velocity
    NoteVelocityCallback onNoteOff = nullptr;  // Triggered on note end
    NoteStartCallback onNoteChange = nullptr;  // Triggered on note change

    // Conversion utilities
    uint8_t frequencyToMidiNote(float hz) const;
    float midiNoteToFrequency(uint8_t note) const;

    // Getters
    uint8_t getCurrentNote() const { return mCurrentNote; }
    uint8_t getLastVelocity() const { return mLastVelocity; }
    float getCurrentPitch() const { return mCurrentPitch; }
    float getPitchBend() const { return mPitchBend; }

    // Configuration
    void setNoteOnThreshold(float threshold) { mNoteOnThreshold = threshold; }
    void setNoteOffThreshold(float threshold) { mNoteOffThreshold = threshold; }
    void setMinNoteDuration(uint32_t ms) { mMinNoteDuration = ms; }
    void setNoteChangeThreshold(int semitones) { mNoteChangeThreshold = semitones; }
    void setVelocitySensitivity(float sensitivity) { mVelocitySensitivity = sensitivity; }

private:
    // Pitch detector (may be owned or borrowed)
    shared_ptr<PitchDetector> mPitchDetector;
    bool mOwnsPitchDetector;

    // Note state tracking
    uint8_t mCurrentNote;
    uint8_t mLastVelocity;
    bool mNoteActive;
    float mCurrentPitch;
    float mPitchBend;
    float mNoteOnEnergy;
    uint32_t mNoteOnTime;
    uint32_t mLastUpdateTime;

    // Detection thresholds
    float mNoteOnThreshold;
    float mNoteOffThreshold;
    uint32_t mMinNoteDuration;
    int mNoteChangeThreshold;
    float mVelocitySensitivity;

    // Note detection helpers
    float calculatePitchBend(float hz, uint8_t note) const;
    uint8_t calculateVelocity(float energy, float confidence) const;

    // Triggers
    bool shouldTriggerNoteOn(float confidence, float pitch) const;
    bool shouldTriggerNoteOff(float confidence, bool voiced) const;
    bool shouldTriggerNoteChange(uint8_t newNote, uint8_t currentNote) const;
};

} // namespace fl
