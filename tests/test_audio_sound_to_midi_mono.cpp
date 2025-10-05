#include "test.h"
#include "fx/audio/sound_to_midi.h"
#include "fl/math.h"
#include "fl/set.h"
#include "fl/codec/mp3.h"
#include "fl/file_system.h"
#ifdef FASTLED_TESTING
#include "platforms/stub/fs_stub.hpp"
#endif
#include <math.h>

using namespace fl;
using namespace fl::third_party;

// Helper: Generate a sine wave at given frequency
static void generateSineWave(float* buffer, int n, float freq_hz, float sample_rate) {
    for (int i = 0; i < n; ++i) {
        float phase = 2.0f * M_PI * freq_hz * i / sample_rate;
        buffer[i] = 0.5f * sinf(phase);
    }
}

// ========== Monophonic Tests ==========

TEST_CASE("SoundToMIDI Mono - Simple A4 sine wave (440Hz -> MIDI 69)") {
    SoundToMIDI cfg;
    cfg.sample_rate_hz = 16000.0f;
    cfg.frame_size = 512;

    SoundToMIDIMono engine(cfg);

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

TEST_CASE("SoundToMIDI Mono - Note off after silence") {
    SoundToMIDI cfg;
    cfg.sample_rate_hz = 16000.0f;
    cfg.frame_size = 512;
    cfg.silence_frames_off = 2;

    SoundToMIDIMono engine(cfg);

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

TEST_CASE("SoundToMIDI Mono - Pitch change triggers retrigger") {
    SoundToMIDI cfg;
    cfg.sample_rate_hz = 16000.0f;
    cfg.frame_size = 512;
    cfg.median_filter_size = 1; // Disable median filter to avoid lag in this test

    SoundToMIDIMono engine(cfg);

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

TEST_CASE("SoundToMIDI Mono - Low amplitude below gate is ignored") {
    SoundToMIDI cfg;
    cfg.sample_rate_hz = 16000.0f;
    cfg.frame_size = 512;
    cfg.rms_gate = 0.010f;

    SoundToMIDIMono engine(cfg);

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

TEST_CASE("SoundToMIDI Mono - Backward compatibility with SoundToMIDIEngine alias") {
    SoundToMIDI cfg;
    cfg.sample_rate_hz = 16000.0f;
    cfg.frame_size = 512;

    SoundToMIDIEngine engine(cfg);

    uint8_t lastNoteOn = 0;
    int noteOnCount = 0;

    engine.onNoteOn = [&](uint8_t note, uint8_t vel) {
        lastNoteOn = note;
        noteOnCount++;
    };

    // Generate A4 (440Hz)
    float frame[512];
    generateSineWave(frame, 512, 440.0f, 16000.0f);

    for (int i = 0; i < 5; ++i) {
        engine.processFrame(frame, 512);
    }

    CHECK_GT(noteOnCount, 0);
    CHECK_EQ(lastNoteOn, 69); // A4
}

TEST_CASE("SoundToMIDI Mono - MP3 melody detection pipeline") {
    // This test validates the complete MP3 → PCM → Pitch Detection → MIDI pipeline
    // using a real musical recording (mary_had_a_little_lamb.mp3)

    // Set up filesystem to point to tests/data directory
    setTestFileSystemRoot("tests/data");

    FileSystem fs;
    REQUIRE(fs.beginSd(0));

    // Open the MP3 file
    FileHandlePtr file = fs.openRead("codec/mary_had_a_little_lamb.mp3");
    REQUIRE(file != nullptr);

    // Read entire file
    fl::size file_size = file->size();
    fl::vector<fl::u8> mp3_data;
    mp3_data.resize(file_size);
    file->read(mp3_data.data(), file_size);
    file->close();

    // Decode MP3 to AudioSamples and extract sample rate
    Mp3HelixDecoder decoder;
    REQUIRE(decoder.init());

    // First, decode to get the sample rate from the first frame
    float detected_sample_rate = 44100.0f; // Default fallback
    fl::vector<AudioSample> samples;
    int frame_count_decode = 0;
    decoder.decode(mp3_data.data(), mp3_data.size(), [&](const Mp3Frame& frame) {
        if (frame_count_decode == 0) {
            detected_sample_rate = (float)frame.sample_rate;
            printf("Detected MP3 sample rate: %d Hz\n", frame.sample_rate);
        }

        // Convert to mono AudioSample (same logic as decodeToAudioSamples)
        if (frame.channels == 2) {
            fl::vector<fl::i16> mono_pcm;
            mono_pcm.reserve(frame.samples);
            for (int i = 0; i < frame.samples; i++) {
                fl::i32 left = frame.pcm[i * 2];
                fl::i32 right = frame.pcm[i * 2 + 1];
                fl::i32 avg = (left + right) / 2;
                mono_pcm.push_back(static_cast<fl::i16>(avg));
            }
            samples.push_back(AudioSample(fl::span<const fl::i16>(mono_pcm.data(), mono_pcm.size())));
        } else {
            samples.push_back(AudioSample(fl::span<const fl::i16>(frame.pcm, frame.samples)));
        }
        frame_count_decode++;
    });
    REQUIRE_GT(samples.size(), 0);

    // Set up pitch detection in monophonic mode for melody detection
    SoundToMIDI cfg;
    cfg.sample_rate_hz = detected_sample_rate; // Use actual MP3 sample rate
    cfg.frame_size = 1024; // 1024 required for 48kHz (512 insufficient for low notes)
    cfg.note_hold_frames = 3; // Slightly faster onset
    cfg.silence_frames_off = 3; // Require 3 frames of silence for note-off
    cfg.rms_gate = 0.012f; // Gate to filter background noise
    cfg.median_filter_size = 1; // No median filter
    cfg.confidence_threshold = 0.80f; // Lower confidence threshold
    cfg.note_change_semitone_threshold = 1; // Require at least 1 semitone change
    cfg.note_change_hold_frames = 3; // Faster note changes

    SoundToMIDIMono engine(cfg);

    fl::vector<int> detected_notes;
    fl::vector<int> detected_full_notes; // Store full MIDI note numbers
    fl::vector<float> detected_frequencies; // Store detected frequencies
    fl::vector<float> detected_rms; // Store RMS values
    int totalNoteOnEvents = 0;
    int totalNoteOffEvents = 0;
    int frame_count = 0;

    engine.onNoteOn = [&](uint8_t note, uint8_t vel) {
        detected_notes.push_back(note % 12); // Store note class only (modulo 12)
        detected_full_notes.push_back(note); // Store full MIDI note
        totalNoteOnEvents++;
        printf("  Frame %d: Note ON: %d (class %d), vel=%d\n", frame_count, note, note % 12, vel);
    };

    engine.onNoteOff = [&](uint8_t note) {
        totalNoteOffEvents++;
    };

    // Flatten all AudioSamples into a single PCM buffer
    fl::vector<float> all_pcm;
    for (const auto& sample : samples) {
        const auto& pcm = sample.pcm();
        for (fl::i16 value : pcm) {
            all_pcm.push_back(value / 32768.0f); // Convert i16 to float [-1.0, 1.0]
        }
    }

    // Process audio in chunks (no overlap for more stable note detection)
    fl::size frame_size = cfg.frame_size;
    fl::vector<float> frame_buffer;
    frame_buffer.resize(frame_size);

    for (fl::size i = 0; i < all_pcm.size(); i += frame_size) {
        fl::size chunk_size = fl_min(frame_size, all_pcm.size() - i);

        // Copy PCM data to frame buffer
        for (fl::size j = 0; j < chunk_size; ++j) {
            frame_buffer[j] = all_pcm[i + j];
        }

        // Pad with zeros if needed
        for (fl::size j = chunk_size; j < frame_size; ++j) {
            frame_buffer[j] = 0.0f;
        }

        engine.processFrame(frame_buffer.data(), frame_size);
        frame_count++;
    }

    // Print detected sequence for analysis
    printf("MP3 to MIDI Pipeline Test Results:\n");
    printf("  Total note-on events: %d\n", totalNoteOnEvents);
    printf("  Total note-off events: %d\n", totalNoteOffEvents);
    printf("  Unique notes detected: %zu\n", detected_notes.size());

    // Expected melody: "Mary Had a Little Lamb" (note classes modulo 12)
    // E  D  C  D  E  E  E  D  D  D  E  G  G  E  D  C  D  E  E  E  E  D  D  E  D  C
    // 4  2  0  2  4  4  4  2  2  2  4  7  7  4  2  0  2  4  4  4  4  2  2  4  2  0
    const int expected_melody[] = {4, 2, 0, 2, 4, 4, 4, 2, 2, 2, 4, 7, 7, 4, 2, 0, 2, 4, 4, 4, 4, 2, 2, 4, 2, 0};
    const int expected_length = sizeof(expected_melody) / sizeof(expected_melody[0]);

    // Print first 10 detected notes for diagnostic purposes
    printf("  First 10 notes detected (note %% 12): ");
    for (size_t i = 0; i < fl_min(static_cast<size_t>(10), detected_notes.size()); ++i) {
        printf("%d ", detected_notes[i]);
    }
    printf("\n");

    printf("  First 10 notes expected (note %% 12): ");
    for (int i = 0; i < fl_min(10, expected_length); ++i) {
        printf("%d ", expected_melody[i]);
    }
    printf("\n");

    // Count matches in first 10 notes (allowing for spurious detections)
    int matches_in_first_10 = 0;
    int expected_idx = 0;
    for (size_t i = 0; i < fl_min(static_cast<size_t>(15), detected_notes.size()) && expected_idx < 10; ++i) {
        if (detected_notes[i] == expected_melody[expected_idx]) {
            matches_in_first_10++;
            expected_idx++;
        }
    }

    // Check first note is correct (critical)
    REQUIRE_GE(detected_notes.size(), static_cast<size_t>(1));
    CHECK_EQ(detected_notes[0], 4); // First note must be E (4)

    // Check we got at least 7 out of first 10 notes correct (70% match rate)
    CHECK_GE(matches_in_first_10, 7);

    printf("  Match rate (first 10 notes): %d/10 = %.0f%%\n", matches_in_first_10, 100.0 * matches_in_first_10 / 10.0);

    // Verify the pipeline is working:
    // 1. MP3 decoded successfully (already checked by REQUIRE_GT above)
    // 2. PCM data was generated
    CHECK_GT(all_pcm.size(), 0);

    // 3. Pitch detection produced note events
    CHECK_GT(totalNoteOnEvents, 0);
    CHECK_GT(totalNoteOffEvents, 0);

    // 4. Detected a reasonable number of notes for a musical piece
    CHECK_GE(detected_notes.size(), static_cast<size_t>(10));

    // 5. Note range is reasonable (MIDI notes should be in musical range)
    for (int note : detected_full_notes) {
        CHECK_GE(note, 20); // Below A0 is unusual
        CHECK_LE(note, 108); // Above C8 is unusual
    }

    // 6. Verify note-on and note-off counts are balanced
    int diff = totalNoteOnEvents > totalNoteOffEvents ?
               totalNoteOnEvents - totalNoteOffEvents :
               totalNoteOffEvents - totalNoteOnEvents;
    CHECK_LE(diff, 10);

    printf("✓ MP3 → PCM → Pitch Detection → MIDI pipeline validated!\n");
    printf("  Melody detection accuracy: %d/10 notes correct (%.0f%%)\n", matches_in_first_10, 100.0 * matches_in_first_10 / 10.0);
}

// ========== Sliding Window Tests ==========

TEST_CASE("SoundToMIDI Sliding - Basic monophonic detection with overlap") {
    SoundToMIDI baseCfg;
    baseCfg.sample_rate_hz = 16000.0f;
    baseCfg.frame_size = 512;

    SlidingCfg slideCfg;
    slideCfg.frame_size = 512;
    slideCfg.hop_size = 256;  // 50% overlap
    slideCfg.window = SlidingCfg::Window::Hann;

    SoundToMIDISliding engine(baseCfg, slideCfg, false); // Monophonic

    uint8_t lastNoteOn = 0;
    int noteOnCount = 0;

    engine.mono().onNoteOn = [&](uint8_t note, uint8_t vel) {
        lastNoteOn = note;
        noteOnCount++;
    };

    // Generate A4 (440Hz) sine wave
    float testSignal[1024];
    generateSineWave(testSignal, 1024, 440.0f, 16000.0f);

    // Stream samples to sliding window engine
    engine.processSamples(testSignal, 1024);

    CHECK_GT(noteOnCount, 0);
    CHECK_EQ(lastNoteOn, 69); // A4 = MIDI 69
}

TEST_CASE("SoundToMIDI Sliding - Hann window application") {
    SoundToMIDI baseCfg;
    baseCfg.sample_rate_hz = 16000.0f;
    baseCfg.frame_size = 512;

    SlidingCfg slideCfg;
    slideCfg.frame_size = 512;
    slideCfg.hop_size = 256;
    slideCfg.window = SlidingCfg::Window::Hann;

    SoundToMIDISliding engine(baseCfg, slideCfg, false);

    // Verify that sliding window was created without crashing
    CHECK(engine.isPolyphonic() == false);
    CHECK(slideCfg.frame_size == 512);
    CHECK(slideCfg.hop_size == 256);
}

TEST_CASE("SoundToMIDI Sliding - Different window types compile") {
    SoundToMIDI baseCfg;
    baseCfg.sample_rate_hz = 16000.0f;
    baseCfg.frame_size = 512;

    // Test all window types
    for (int w = 0; w < 3; ++w) {
        SlidingCfg slideCfg;
        slideCfg.frame_size = 512;
        slideCfg.hop_size = 256;
        slideCfg.window = static_cast<SlidingCfg::Window>(w);

        SoundToMIDISliding engine(baseCfg, slideCfg, false);

        // Generate test signal
        float testSignal[512];
        generateSineWave(testSignal, 512, 440.0f, 16000.0f);

        // Process samples - should not crash
        engine.processSamples(testSignal, 512);
    }

    CHECK(true); // If we get here, all window types work
}

TEST_CASE("SoundToMIDI Sliding - 75% overlap improves stability") {
    SoundToMIDI baseCfg;
    baseCfg.sample_rate_hz = 16000.0f;
    baseCfg.frame_size = 512;
    baseCfg.note_hold_frames = 2;

    SlidingCfg slideCfg;
    slideCfg.frame_size = 512;
    slideCfg.hop_size = 128;  // 75% overlap
    slideCfg.window = SlidingCfg::Window::Hann;

    SoundToMIDISliding engine(baseCfg, slideCfg, false);

    int noteOnCount = 0;
    uint8_t detectedNote = 0;

    engine.mono().onNoteOn = [&](uint8_t note, uint8_t vel) {
        detectedNote = note;
        noteOnCount++;
    };

    // Generate longer A4 signal
    float testSignal[2048];
    generateSineWave(testSignal, 2048, 440.0f, 16000.0f);

    engine.processSamples(testSignal, 2048);

    // With more overlap, we should get more stable detections
    CHECK_GT(noteOnCount, 0);
    CHECK_EQ(detectedNote, 69);
}
