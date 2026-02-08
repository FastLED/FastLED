#pragma once

#include "fl/stl/memory.h"
#include "fl/audio/audio_context.h"
#include "fl/fx/audio/detectors/pitch.h"

namespace fl {

class NoteDetector {
public:
    // Constants
    static constexpr u8 NO_NOTE = 255;
    static constexpr float A4_FREQUENCY = 440.0f;
    static constexpr u8 A4_MIDI_NOTE = 69;

    // Constructors
    NoteDetector();
    explicit NoteDetector(shared_ptr<PitchDetector> pitchDetector);
    ~NoteDetector();

    // Update and reset
    void update(shared_ptr<AudioContext> context);
    void reset();

    // Callback types
    using NoteVelocityCallback = void(*)(u8 note);
    using NoteStartCallback = void(*)(u8 note, u8 velocity);

    // Note-related callback functions
    NoteStartCallback onNoteOn = nullptr;      // Triggered on new note start with velocity
    NoteVelocityCallback onNoteOff = nullptr;  // Triggered on note end
    NoteStartCallback onNoteChange = nullptr;  // Triggered on note change

    // Conversion utilities
    u8 frequencyToMidiNote(float hz) const;
    float midiNoteToFrequency(u8 note) const;

    // Getters
    u8 getCurrentNote() const { return mCurrentNote; }
    u8 getLastVelocity() const { return mLastVelocity; }
    float getCurrentPitch() const { return mCurrentPitch; }
    float getPitchBend() const { return mPitchBend; }

    // Configuration
    void setNoteOnThreshold(float threshold) { mNoteOnThreshold = threshold; }
    void setNoteOffThreshold(float threshold) { mNoteOffThreshold = threshold; }
    void setMinNoteDuration(u32 ms) { mMinNoteDuration = ms; }
    void setNoteChangeThreshold(int semitones) { mNoteChangeThreshold = semitones; }
    void setVelocitySensitivity(float sensitivity) { mVelocitySensitivity = sensitivity; }

private:
    // Pitch detector (may be owned or borrowed)
    shared_ptr<PitchDetector> mPitchDetector;
    bool mOwnsPitchDetector;

    // Note state tracking
    u8 mCurrentNote;
    u8 mLastVelocity;
    bool mNoteActive;
    float mCurrentPitch;
    float mPitchBend;
    float mNoteOnEnergy;
    u32 mNoteOnTime;
    u32 mLastUpdateTime;

    // Detection thresholds
    float mNoteOnThreshold;
    float mNoteOffThreshold;
    u32 mMinNoteDuration;
    int mNoteChangeThreshold;
    float mVelocitySensitivity;

    // Note detection helpers
    float calculatePitchBend(float hz, u8 note) const;
    u8 calculateVelocity(float energy, float confidence) const;

    // Triggers
    bool shouldTriggerNoteOn(float confidence, float pitch) const;
    bool shouldTriggerNoteOff(float confidence, bool voiced) const;
    bool shouldTriggerNoteChange(u8 newNote, u8 currentNote) const;
};

} // namespace fl
