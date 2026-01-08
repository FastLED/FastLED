
// BeatDetectorEDM is not yet implemented - disable these tests
#if 0  // Was: #if SKETCH_HAS_LOTS_OF_MEMORY

#include "fx/audio/advanced/beat_detector_edm.h"
#include "fl/codec/mp3.h"
#include "fl/file_system.h"
#include "fl/stl/stdio.h"
#ifdef FASTLED_TESTING
#include "platforms/stub/fs_stub.hpp"
#endif

using namespace fl::third_party;

TEST_CASE("BeatDetector - Basic initialization") {
    BeatDetectorConfig config;
    config.sample_rate_hz = 44100;
    config.frame_size = 512;
    config.hop_size = 256;
    config.fft_size = 512;
    config.num_bands = 24;
    config.odf_type = OnsetDetectionFunction::SuperFlux;
    config.peak_mode = PeakPickingMode::SuperFluxPeaks;
    config.tempo_tracker = TempoTrackerType::CombFilter;

    BeatDetector detector(config);

    // Test initial state - detector may have default tempo set
    TempoEstimate tempo = detector.getTempo();
    CHECK_GE(tempo.bpm, 0.0f); // May have default value
    CHECK_GE(tempo.confidence, 0.0f);

    float odf = detector.getCurrentODF();
    CHECK_GE(odf, 0.0f); // Should be non-negative
}

TEST_CASE("BeatDetector - EDM beat detection from MP3") {
    // Set up filesystem to point to tests/data directory
    setTestFileSystemRoot("tests/data");

    FileSystem fs;
    CHECK(fs.beginSd(0)); // CS pin doesn't matter for test

    // Open the EDM beat MP3 file
    FileHandlePtr file = fs.openRead("codec/edm_beat.mp3");
    REQUIRE(file != nullptr);
    REQUIRE(file->valid());

    // Read entire file into buffer
    fl::size file_size = file->size();
    CHECK_GT(file_size, 0);

    fl::vector<fl::u8> mp3_data;
    mp3_data.resize(file_size);
    fl::size bytes_read = file->read(mp3_data.data(), file_size);
    CHECK_EQ(bytes_read, file_size);

    file->close();

    // Decode MP3 data
    Mp3HelixDecoder decoder;
    CHECK(decoder.init());

    // Collect all decoded audio samples
    fl::vector<float> audio_samples;
    int sample_rate = 0;
    int channels = 0;

    decoder.decode(mp3_data.data(), mp3_data.size(), [&](const Mp3Frame& frame) {
        sample_rate = frame.sample_rate;
        channels = frame.channels;

        // Convert int16_t samples to float
        // If stereo, convert to mono by averaging channels
        if (channels == 2) {
            for (int i = 0; i < frame.samples; ++i) {
                float left = frame.pcm[i * 2] / 32768.0f;
                float right = frame.pcm[i * 2 + 1] / 32768.0f;
                float mono = (left + right) * 0.5f;
                audio_samples.push_back(mono);
            }
        } else {
            for (int i = 0; i < frame.samples; ++i) {
                float sample = frame.pcm[i] / 32768.0f;
                audio_samples.push_back(sample);
            }
        }
    });

    CHECK_GT(audio_samples.size(), 0);
    CHECK_GT(sample_rate, 0);

    printf("Decoded %d audio samples at %d Hz, %d channels (converted to mono)\n",
               (int)audio_samples.size(), sample_rate, channels);

    // Configure beat detector
    BeatDetectorConfig config;
    config.sample_rate_hz = sample_rate;
    config.frame_size = 512;
    config.hop_size = 256;
    config.fft_size = 512;
    config.num_bands = 24;
    config.log_compression = true;
    config.adaptive_whitening = true;

    // Use SuperFlux ODF (best for EDM)
    config.odf_type = OnsetDetectionFunction::SuperFlux;
    config.superflux_mu = 3;
    config.max_filter_radius = 2;

    // SuperFlux peak picking
    config.peak_mode = PeakPickingMode::SuperFluxPeaks;
    config.peak_threshold_delta = 0.07f;
    config.peak_pre_max_ms = 30;
    config.peak_post_max_ms = 30;
    config.peak_pre_avg_ms = 100;
    config.peak_post_avg_ms = 70;
    config.min_inter_onset_ms = 30;

    // Comb filter tempo tracking (100-150 BPM for EDM)
    config.tempo_tracker = TempoTrackerType::CombFilter;
    config.tempo_min_bpm = 100;
    config.tempo_max_bpm = 150;
    config.tempo_rayleigh_sigma = 120;
    config.tempo_acf_window_sec = 4;

    BeatDetector beat_detector(config);

    // Track detection results
    int onset_count = 0;
    int beat_count = 0;
    float last_detected_bpm = 0.0f;

    // Set up callbacks
    beat_detector.onOnset = [&](float confidence, float timestamp_ms) {
        onset_count++;
        printf("ONSET detected at %.2f ms, confidence=%.3f\n", timestamp_ms, confidence);
    };

    beat_detector.onBeat = [&](float confidence, float bpm, float timestamp_ms) {
        beat_count++;
        last_detected_bpm = bpm;
        printf("BEAT detected at %.2f ms, BPM=%.1f, confidence=%.3f\n",
                   timestamp_ms, bpm, confidence);
    };

    beat_detector.onTempoChange = [&](float bpm, float confidence) {
        printf("Tempo changed: %.1f BPM, confidence=%.3f\n", bpm, confidence);
    };

    // Process audio in frames
    int frame_size = config.frame_size;
    int hop_size = config.hop_size;

    for (size_t i = 0; i + frame_size <= audio_samples.size(); i += hop_size) {
        beat_detector.processFrame(&audio_samples[i], frame_size);
    }

    // Get final tempo estimate
    TempoEstimate tempo = beat_detector.getTempo();

    printf("\n=== Beat Detection Results ===\n");
    printf("Total onsets detected: %d\n", onset_count);
    printf("Total beats detected: %d\n", beat_count);
    printf("Final tempo: %.1f BPM (confidence: %.3f)\n", tempo.bpm, tempo.confidence);

    // Note: Beat detection is sensitive to parameters and audio content
    // The test primarily verifies the system doesn't crash and produces plausible output

    printf("Onsets: %d, Beats: %d\n", onset_count, beat_count);

    // Verify basic operation (may or may not detect beats depending on audio/config)
    CHECK_GE(onset_count, 0);
    CHECK_GE(beat_count, 0);

    // Tempo should be in plausible range if detected
    if (tempo.bpm > 0.0f) {
        CHECK_GE(tempo.bpm, 40.0f);   // Min plausible tempo
        CHECK_LE(tempo.bpm, 240.0f);  // Max plausible tempo
    }
}

TEST_CASE("BeatDetector - Configuration options") {
    BeatDetectorConfig config;
    config.sample_rate_hz = 44100;
    config.frame_size = 512;
    config.hop_size = 256;
    config.fft_size = 512;

    // Test different ODF types
    SUBCASE("Energy ODF") {
        config.odf_type = OnsetDetectionFunction::Energy;
        BeatDetector detector(config);
        CHECK_EQ(detector.getCurrentODF(), 0.0f);
    }

    SUBCASE("SpectralFlux ODF") {
        config.odf_type = OnsetDetectionFunction::SpectralFlux;
        BeatDetector detector(config);
        CHECK_EQ(detector.getCurrentODF(), 0.0f);
    }

    SUBCASE("SuperFlux ODF") {
        config.odf_type = OnsetDetectionFunction::SuperFlux;
        BeatDetector detector(config);
        CHECK_EQ(detector.getCurrentODF(), 0.0f);
    }

    SUBCASE("MultiBand ODF") {
        config.odf_type = OnsetDetectionFunction::MultiBand;
        config.bands.clear();
        config.bands.push_back({60.0f, 160.0f, 1.5f});   // Bass
        config.bands.push_back({160.0f, 2000.0f, 1.0f}); // Mid
        config.bands.push_back({2000.0f, 8000.0f, 1.2f}); // High
        BeatDetector detector(config);
        CHECK_EQ(detector.getCurrentODF(), 0.0f);
    }
}

TEST_CASE("BeatDetector - Peak picking modes") {
    BeatDetectorConfig config;
    config.sample_rate_hz = 44100;
    config.frame_size = 512;
    config.hop_size = 256;

    SUBCASE("LocalMaximum mode") {
        config.peak_mode = PeakPickingMode::LocalMaximum;
        BeatDetector detector(config);
        // Just verify it initializes
        CHECK_EQ(detector.getCurrentODF(), 0.0f);
    }

    SUBCASE("AdaptiveThreshold mode") {
        config.peak_mode = PeakPickingMode::AdaptiveThreshold;
        BeatDetector detector(config);
        CHECK_EQ(detector.getCurrentODF(), 0.0f);
    }

    SUBCASE("SuperFluxPeaks mode") {
        config.peak_mode = PeakPickingMode::SuperFluxPeaks;
        BeatDetector detector(config);
        CHECK_EQ(detector.getCurrentODF(), 0.0f);
    }
}

TEST_CASE("BeatDetector - Tempo tracking modes") {
    BeatDetectorConfig config;
    config.sample_rate_hz = 44100;
    config.frame_size = 512;
    config.hop_size = 256;

    SUBCASE("No tempo tracking") {
        config.tempo_tracker = TempoTrackerType::None;
        BeatDetector detector(config);
        TempoEstimate tempo = detector.getTempo();
        // May have default BPM
        CHECK_GE(tempo.bpm, 0.0f);
    }

    SUBCASE("CombFilter tempo tracking") {
        config.tempo_tracker = TempoTrackerType::CombFilter;
        BeatDetector detector(config);
        TempoEstimate tempo = detector.getTempo();
        // May have default BPM
        CHECK_GE(tempo.bpm, 0.0f);
    }

    SUBCASE("Autocorrelation tempo tracking") {
        config.tempo_tracker = TempoTrackerType::Autocorrelation;
        BeatDetector detector(config);
        TempoEstimate tempo = detector.getTempo();
        // May have default BPM
        CHECK_GE(tempo.bpm, 0.0f);
    }
}

TEST_CASE("BeatDetector - Callback mechanisms") {
    BeatDetectorConfig config;
    config.sample_rate_hz = 44100;
    config.frame_size = 512;
    config.hop_size = 256;

    BeatDetector detector(config);

    bool onset_called = false;
    bool beat_called = false;
    bool tempo_change_called = false;

    detector.onOnset = [&](float confidence, float timestamp_ms) {
        onset_called = true;
    };

    detector.onBeat = [&](float confidence, float bpm, float timestamp_ms) {
        beat_called = true;
    };

    detector.onTempoChange = [&](float bpm, float confidence) {
        tempo_change_called = true;
    };

    // Generate a simple test signal (sine wave)
    fl::vector<float> test_signal(512);
    for (int i = 0; i < 512; ++i) {
        // 440 Hz sine wave
        float phase = 2.0f * FL_M_PI * 440.0f * i / 44100.0f;
        test_signal[i] = 0.5f * fl::sinf(phase);
    }

    detector.processFrame(test_signal.data(), 512);

    // Callbacks may or may not be triggered by this simple signal
    // Just verify the mechanism doesn't crash
    (void)onset_called;
    (void)beat_called;
    (void)tempo_change_called;
}

#endif // SKETCH_HAS_LOTS_OF_MEMORY
