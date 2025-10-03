#include "test.h"
#include "fx/audio/pitch_to_midi.h"
#include "fl/math.h"
#include <math.h>

using namespace fl;

// Helper: Generate a sine wave at given frequency
static void generateSineWave(float* buffer, int n, float freq_hz, float sample_rate) {
    for (int i = 0; i < n; ++i) {
        float phase = 2.0f * M_PI * freq_hz * i / sample_rate;
        buffer[i] = 0.5f * sinf(phase);
    }
}

TEST_CASE("PitchToMIDI - Simple A4 sine wave (440Hz -> MIDI 69)") {
    PitchToMIDI cfg;
    cfg.sample_rate_hz = 16000.0f;
    cfg.frame_size = 512;

    PitchToMIDIEngine engine(cfg);

    uint8_t lastNoteOn = 0;
    uint8_t lastVelocity = 0;
    uint8_t lastNoteOff = 0;
    int noteOnCount = 0;
    int noteOffCount = 0;

    engine.onNoteOn = [&](uint8_t note, uint8_t vel) {
        lastNoteOn = note;
        lastVelocity = vel;
        noteOnCount++;
    };

    engine.onNoteOff = [&](uint8_t note) {
        lastNoteOff = note;
        noteOffCount++;
    };

    // Generate A4 (440Hz) which should be MIDI note 69
    float frame[512];
    generateSineWave(frame, 512, 440.0f, 16000.0f);

    // Process enough frames to trigger note-on (need note_hold_frames = 3)
    for (int i = 0; i < 5; ++i) {
        engine.processFrame(frame, 512);
    }

    CHECK_GT(noteOnCount, 0);
    CHECK_EQ(lastNoteOn, 69); // A4 = MIDI note 69
    CHECK_GT(lastVelocity, 0);
}

TEST_CASE("PitchToMIDI - Note off after silence") {
    PitchToMIDI cfg;
    cfg.sample_rate_hz = 16000.0f;
    cfg.frame_size = 512;
    cfg.silence_frames_off = 2;

    PitchToMIDIEngine engine(cfg);

    uint8_t lastNoteOff = 0;
    int noteOnCount = 0;
    int noteOffCount = 0;

    engine.onNoteOn = [&](uint8_t note, uint8_t vel) {
        noteOnCount++;
    };

    engine.onNoteOff = [&](uint8_t note) {
        lastNoteOff = note;
        noteOffCount++;
    };

    // Generate A4 (440Hz)
    float frame[512];
    generateSineWave(frame, 512, 440.0f, 16000.0f);

    // Process enough to trigger note-on
    for (int i = 0; i < 5; ++i) {
        engine.processFrame(frame, 512);
    }

    CHECK_GT(noteOnCount, 0);

    // Now send silence (zero amplitude)
    float silence[512] = {0};
    for (int i = 0; i < 5; ++i) {
        engine.processFrame(silence, 512);
    }

    CHECK_GT(noteOffCount, 0);
    CHECK_EQ(lastNoteOff, 69); // Should turn off A4
}

TEST_CASE("PitchToMIDI - Pitch change triggers retrigger") {
    PitchToMIDI cfg;
    cfg.sample_rate_hz = 16000.0f;
    cfg.frame_size = 512;

    PitchToMIDIEngine engine(cfg);

    uint8_t firstNote = 0;
    uint8_t secondNote = 0;
    int noteOnCount = 0;
    int noteOffCount = 0;

    engine.onNoteOn = [&](uint8_t note, uint8_t vel) {
        if (noteOnCount == 0) {
            firstNote = note;
        } else if (noteOnCount == 1) {
            secondNote = note;
        }
        noteOnCount++;
    };

    engine.onNoteOff = [&](uint8_t note) {
        noteOffCount++;
    };

    // Generate A4 (440Hz) - MIDI 69
    float frameA4[512];
    generateSineWave(frameA4, 512, 440.0f, 16000.0f);

    for (int i = 0; i < 5; ++i) {
        engine.processFrame(frameA4, 512);
    }

    CHECK_EQ(noteOnCount, 1);
    CHECK_EQ(firstNote, 69);

    // Generate C5 (523.25Hz) - MIDI 72 (3 semitones higher)
    float frameC5[512];
    generateSineWave(frameC5, 512, 523.25f, 16000.0f);

    for (int i = 0; i < 5; ++i) {
        engine.processFrame(frameC5, 512);
    }

    CHECK_EQ(noteOnCount, 2); // Should have triggered second note-on
    CHECK_EQ(noteOffCount, 1); // Should have turned off first note
    CHECK_EQ(secondNote, 72);
}

TEST_CASE("PitchToMIDI - Low amplitude below gate is ignored") {
    PitchToMIDI cfg;
    cfg.sample_rate_hz = 16000.0f;
    cfg.frame_size = 512;
    cfg.rms_gate = 0.010f;

    PitchToMIDIEngine engine(cfg);

    int noteOnCount = 0;

    engine.onNoteOn = [&](uint8_t note, uint8_t vel) {
        noteOnCount++;
    };

    // Generate very low amplitude signal (below gate)
    float frame[512];
    for (int i = 0; i < 512; ++i) {
        float phase = 2.0f * M_PI * 440.0f * i / 16000.0f;
        frame[i] = 0.001f * sinf(phase); // Very quiet
    }

    for (int i = 0; i < 10; ++i) {
        engine.processFrame(frame, 512);
    }

    CHECK_EQ(noteOnCount, 0); // Should not trigger note-on
}
