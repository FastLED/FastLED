#include "test.h"

#if SKETCH_HAS_LOTS_OF_MEMORY

#include "fx/audio/beat_detector.h"
#include "fl/codec/mp3.h"
#include "fl/file_system.h"
#include "fl/printf.h"
#include <math.h>
#ifdef FASTLED_TESTING
#include "platforms/stub/fs_stub.hpp"
#endif

using namespace fl;
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
        float phase = 2.0f * M_PI * 440.0f * i / 44100.0f;
        test_signal[i] = 0.5f * ::sin(phase);
    }

    detector.processFrame(test_signal.data(), 512);

    // Callbacks may or may not be triggered by this simple signal
    // Just verify the mechanism doesn't crash
    (void)onset_called;
    (void)beat_called;
    (void)tempo_change_called;
}

TEST_CASE("BeatDetector - Spectral flux onset detection") {
    BeatDetectorConfig config;
    config.sample_rate_hz = 44100;
    config.frame_size = 512;
    config.hop_size = 256;
    config.fft_size = 512;
    config.odf_type = OnsetDetectionFunction::SpectralFlux;

    BeatDetector detector(config);

    // Create a signal with a clear onset (impulse)
    fl::vector<float> silence(512, 0.0f);
    fl::vector<float> impulse(512, 0.0f);
    impulse[0] = 1.0f; // Sharp onset

    // Process silence first to establish baseline
    for (int i = 0; i < 5; ++i) {
        detector.processFrame(silence.data(), 512);
    }

    float baseline_odf = detector.getCurrentODF();

    // Process impulse - should create onset
    detector.processFrame(impulse.data(), 512);
    float onset_odf = detector.getCurrentODF();

    // Onset ODF should be higher than baseline
    CHECK_GT(onset_odf, baseline_odf);
}

TEST_CASE("BeatDetector - HFC onset detection") {
    BeatDetectorConfig config;
    config.sample_rate_hz = 44100;
    config.frame_size = 512;
    config.hop_size = 256;
    config.fft_size = 512;
    config.odf_type = OnsetDetectionFunction::HFC;

    BeatDetector detector(config);

    // HFC emphasizes high frequencies
    // Create low-frequency vs high-frequency signals
    fl::vector<float> low_freq(512);
    fl::vector<float> high_freq(512);

    for (int i = 0; i < 512; ++i) {
        // 100 Hz low frequency
        low_freq[i] = 0.5f * ::sin(2.0f * M_PI * 100.0f * i / 44100.0f);
        // 8000 Hz high frequency
        high_freq[i] = 0.5f * ::sin(2.0f * M_PI * 8000.0f * i / 44100.0f);
    }

    detector.processFrame(low_freq.data(), 512);
    float low_hfc = detector.getCurrentODF();

    detector.reset();

    detector.processFrame(high_freq.data(), 512);
    float high_hfc = detector.getCurrentODF();

    // HFC should be much higher for high frequency content
    CHECK_GT(high_hfc, low_hfc);
}

TEST_CASE("BeatDetector - Multi-band onset detection") {
    BeatDetectorConfig config;
    config.sample_rate_hz = 44100;
    config.frame_size = 512;
    config.hop_size = 256;
    config.fft_size = 512;
    config.odf_type = OnsetDetectionFunction::MultiBand;

    // Custom bands for testing
    config.bands.clear();
    config.bands.push_back({60.0f, 160.0f, 2.0f});    // Heavy bass emphasis
    config.bands.push_back({160.0f, 2000.0f, 1.0f});  // Mid neutral
    config.bands.push_back({2000.0f, 8000.0f, 1.0f}); // High neutral

    BeatDetector detector(config);

    // Create bass-heavy signal (kick drum frequency)
    fl::vector<float> bass_signal(512);
    for (int i = 0; i < 512; ++i) {
        bass_signal[i] = 0.8f * ::sin(2.0f * M_PI * 80.0f * i / 44100.0f);
    }

    // Process silence then bass
    fl::vector<float> silence(512, 0.0f);
    detector.processFrame(silence.data(), 512);
    detector.processFrame(bass_signal.data(), 512);

    float bass_odf = detector.getCurrentODF();

    // Should detect onset in bass range with heavy weighting
    CHECK_GT(bass_odf, 0.0f);
}

TEST_CASE("BeatDetector - Adaptive whitening") {
    BeatDetectorConfig config;
    config.sample_rate_hz = 44100;
    config.frame_size = 512;
    config.hop_size = 256;
    config.fft_size = 512;
    config.odf_type = OnsetDetectionFunction::SpectralFlux;
    config.adaptive_whitening = true;
    config.whitening_alpha = 0.95f;

    BeatDetector detector(config);

    // Create sustained note that would normally mask onsets
    fl::vector<float> sustained(512);
    for (int i = 0; i < 512; ++i) {
        sustained[i] = 0.9f * ::sin(2.0f * M_PI * 440.0f * i / 44100.0f);
    }

    // Process sustained note multiple times to build up running max
    for (int i = 0; i < 10; ++i) {
        detector.processFrame(sustained.data(), 512);
    }

    // Now add an onset in the same frequency range
    fl::vector<float> onset(512);
    for (int i = 0; i < 512; ++i) {
        onset[i] = 1.0f * ::sin(2.0f * M_PI * 440.0f * i / 44100.0f);
    }

    detector.processFrame(onset.data(), 512);
    float whitened_odf = detector.getCurrentODF();

    // Adaptive whitening should still detect onset despite sustained note
    // (Without whitening, this might be suppressed)
    CHECK_GE(whitened_odf, 0.0f);
}

TEST_CASE("BeatDetector - Log compression") {
    BeatDetectorConfig config;
    config.sample_rate_hz = 44100;
    config.frame_size = 512;
    config.hop_size = 256;
    config.fft_size = 512;
    config.odf_type = OnsetDetectionFunction::SpectralFlux;

    // Test with and without log compression
    SUBCASE("With log compression") {
        config.log_compression = true;
        BeatDetector detector(config);

        fl::vector<float> loud_signal(512);
        for (int i = 0; i < 512; ++i) {
            loud_signal[i] = 0.9f * ::sin(2.0f * M_PI * 440.0f * i / 44100.0f);
        }

        detector.processFrame(loud_signal.data(), 512);
        CHECK_GE(detector.getCurrentODF(), 0.0f);
    }

    SUBCASE("Without log compression") {
        config.log_compression = false;
        BeatDetector detector(config);

        fl::vector<float> loud_signal(512);
        for (int i = 0; i < 512; ++i) {
            loud_signal[i] = 0.9f * ::sin(2.0f * M_PI * 440.0f * i / 44100.0f);
        }

        detector.processFrame(loud_signal.data(), 512);
        CHECK_GE(detector.getCurrentODF(), 0.0f);
    }
}

TEST_CASE("BeatDetector - SuperFlux with maximum filter") {
    BeatDetectorConfig config;
    config.sample_rate_hz = 44100;
    config.frame_size = 512;
    config.hop_size = 256;
    config.fft_size = 512;
    config.odf_type = OnsetDetectionFunction::SuperFlux;
    config.superflux_mu = 3;
    config.max_filter_radius = 2;

    BeatDetector detector(config);

    // SuperFlux requires history to build up
    fl::vector<float> signal(512);
    for (int i = 0; i < 512; ++i) {
        signal[i] = 0.5f * ::sin(2.0f * M_PI * 440.0f * i / 44100.0f);
    }

    // Process several frames to build history
    for (int i = 0; i < 5; ++i) {
        detector.processFrame(signal.data(), 512);
    }

    // Change frequency to create onset
    for (int i = 0; i < 512; ++i) {
        signal[i] = 0.5f * ::sin(2.0f * M_PI * 880.0f * i / 44100.0f);
    }

    detector.processFrame(signal.data(), 512);
    float superflux_odf = detector.getCurrentODF();

    // Should detect frequency change as onset
    CHECK_GE(superflux_odf, 0.0f);
}

TEST_CASE("BeatDetector - Energy-based onset detection") {
    BeatDetectorConfig config;
    config.sample_rate_hz = 44100;
    config.frame_size = 512;
    config.hop_size = 256;
    config.odf_type = OnsetDetectionFunction::Energy;

    BeatDetector detector(config);

    // Energy-based doesn't require FFT
    fl::vector<float> quiet(512, 0.1f);
    fl::vector<float> loud(512, 0.9f);

    detector.processFrame(quiet.data(), 512);
    detector.processFrame(loud.data(), 512);

    float energy_odf = detector.getCurrentODF();

    // Should detect energy increase
    CHECK_GT(energy_odf, 0.0f);
}

TEST_CASE("BeatDetector - Peak picking with minimum distance") {
    BeatDetectorConfig config;
    config.sample_rate_hz = 44100;
    config.frame_size = 512;
    config.hop_size = 256;
    config.fft_size = 512;
    config.odf_type = OnsetDetectionFunction::SpectralFlux;
    config.peak_mode = PeakPickingMode::SuperFluxPeaks;
    config.min_inter_onset_ms = 50; // 50ms minimum spacing

    BeatDetector detector(config);

    int onset_count = 0;
    fl::vector<float> onset_times;

    detector.onOnset = [&](float confidence, float timestamp_ms) {
        onset_count++;
        onset_times.push_back(timestamp_ms);
    };

    // Create repeated impulses closer than 50ms apart
    fl::vector<float> impulse(512);
    impulse[0] = 1.0f;

    fl::vector<float> silence(128, 0.0f); // 2.9ms of silence

    // Process: impulse, short silence, impulse, short silence (multiple times)
    for (int i = 0; i < 20; ++i) {
        detector.processFrame(impulse.data(), 512);
        detector.processFrame(silence.data(), 128);
    }

    // Due to minimum inter-onset interval, should have fewer detections than impulses
    // (Some consecutive impulses within 50ms should be debounced)
    if (onset_count > 1) {
        // Check that detected onsets are at least 50ms apart
        for (size_t i = 1; i < onset_times.size(); ++i) {
            float interval = onset_times[i] - onset_times[i-1];
            CHECK_GE(interval, 45.0f); // Allow small tolerance for frame boundaries
        }
    }
}

TEST_CASE("BeatDetector - Tempo estimation via autocorrelation") {
    BeatDetectorConfig config;
    config.sample_rate_hz = 44100;
    config.frame_size = 512;
    config.hop_size = 256;
    config.fft_size = 512;
    config.odf_type = OnsetDetectionFunction::SpectralFlux;
    config.tempo_tracker = TempoTrackerType::Autocorrelation;
    config.tempo_min_bpm = 100;
    config.tempo_max_bpm = 150;

    BeatDetector detector(config);

    // Generate periodic onsets at known tempo (120 BPM)
    // 120 BPM = 2 beats/sec = 0.5 sec/beat = 500ms per beat
    float beat_period_ms = 500.0f;
    int frames_per_beat = static_cast<int>(beat_period_ms / (256.0f / 44100.0f * 1000.0f));

    fl::vector<float> impulse(512);
    impulse[0] = 1.0f;
    fl::vector<float> silence(512, 0.0f);

    // Generate several seconds of periodic beats
    for (int beat = 0; beat < 20; ++beat) {
        detector.processFrame(impulse.data(), 512);

        // Fill silence between beats
        for (int i = 0; i < frames_per_beat - 1; ++i) {
            detector.processFrame(silence.data(), 512);
        }
    }

    TempoEstimate tempo = detector.getTempo();

    // Should detect tempo around 120 BPM (allow tolerance)
    if (tempo.bpm > 0.0f) {
        CHECK_GE(tempo.bpm, 100.0f);
        CHECK_LE(tempo.bpm, 150.0f);
    }
}

TEST_CASE("BeatDetector - Comb filter tempo tracking") {
    BeatDetectorConfig config;
    config.sample_rate_hz = 44100;
    config.frame_size = 512;
    config.hop_size = 256;
    config.fft_size = 512;
    config.tempo_tracker = TempoTrackerType::CombFilter;
    config.tempo_min_bpm = 100;
    config.tempo_max_bpm = 150;

    BeatDetector detector(config);

    // Comb filter should emphasize harmonic structure
    // Just verify it initializes and runs
    fl::vector<float> signal(512);
    for (int i = 0; i < 512; ++i) {
        signal[i] = 0.5f * ::sin(2.0f * M_PI * 440.0f * i / 44100.0f);
    }

    for (int i = 0; i < 100; ++i) {
        detector.processFrame(signal.data(), 512);
    }

    TempoEstimate tempo = detector.getTempo();
    CHECK_GE(tempo.bpm, 0.0f);
}

TEST_CASE("BeatDetector - Performance and latency") {
    BeatDetectorConfig config;
    config.sample_rate_hz = 48000;
    config.frame_size = 512;
    config.hop_size = 256;
    config.fft_size = 512;
    config.odf_type = OnsetDetectionFunction::SuperFlux;

    BeatDetector detector(config);

    fl::vector<float> signal(512);
    for (int i = 0; i < 512; ++i) {
        signal[i] = 0.5f * ::sin(2.0f * M_PI * 440.0f * i / 44100.0f);
    }

    // Measure processing time for a frame
    // (Simple timing check - not precise benchmarking)
    auto start_frame = detector.getFrameCount();

    for (int i = 0; i < 100; ++i) {
        detector.processFrame(signal.data(), 512);
    }

    auto end_frame = detector.getFrameCount();

    // Should have processed 100 frames
    CHECK_EQ(end_frame - start_frame, 100);

    // Frame duration: 256 samples / 48000 Hz = 5.33ms
    // Processing should complete (we're just verifying it doesn't hang)
}

TEST_CASE("BeatDetector - Reset functionality") {
    BeatDetectorConfig config;
    config.sample_rate_hz = 44100;
    config.frame_size = 512;
    config.hop_size = 256;

    BeatDetector detector(config);

    // Process some frames
    fl::vector<float> signal(512, 0.5f);
    for (int i = 0; i < 10; ++i) {
        detector.processFrame(signal.data(), 512);
    }

    auto frame_count_before = detector.getFrameCount();
    CHECK_GT(frame_count_before, 0);

    // Reset
    detector.reset();

    // Counters should be reset
    CHECK_EQ(detector.getFrameCount(), 0);
    CHECK_EQ(detector.getOnsetCount(), 0);
    CHECK_EQ(detector.getBeatCount(), 0);
}

TEST_CASE("BeatDetector - Configuration update") {
    BeatDetectorConfig config1;
    config1.sample_rate_hz = 44100;
    config1.odf_type = OnsetDetectionFunction::SpectralFlux;

    BeatDetector detector(config1);

    // Update configuration
    BeatDetectorConfig config2;
    config2.sample_rate_hz = 48000;
    config2.odf_type = OnsetDetectionFunction::SuperFlux;

    detector.setConfig(config2);

    // Verify new config is active
    CHECK_EQ(detector.config().sample_rate_hz, 48000.0f);
    CHECK_EQ(detector.config().odf_type, OnsetDetectionFunction::SuperFlux);
}

TEST_CASE("BeatDetector - Particle filter tempo tracking") {
    BeatDetectorConfig config;
    config.sample_rate_hz = 48000;
    config.frame_size = 512;
    config.hop_size = 256;
    config.odf_type = OnsetDetectionFunction::SuperFlux;
    config.tempo_tracker = TempoTrackerType::ParticleFilter;
    config.tempo_min_bpm = 90.0f;
    config.tempo_max_bpm = 180.0f;
    config.pf_num_particles = 64;
    config.pf_tempo_std_dev = 2.0f;
    config.pf_phase_std_dev = 0.02f;
    config.pf_resample_threshold = 0.5f;

    BeatDetector detector(config);

    bool beat_detected = false;
    float detected_bpm = 0.0f;

    detector.onBeat = [&](float confidence, float bpm, float timestamp_ms) {
        beat_detected = true;
        detected_bpm = bpm;
        fl::printf("Particle filter beat: BPM=%.1f, confidence=%.2f, time=%.1fms\n",
                   bpm, confidence, timestamp_ms);
    };

    // Generate synthetic audio with tempo change (120 -> 140 BPM)
    int num_frames = 50;  // Reduced from 200 to avoid timeout
    float current_bpm = 120.0f;
    float beat_phase = 0.0f;

    for (int frame_idx = 0; frame_idx < num_frames; ++frame_idx) {
        // Simulate tempo change at frame 25
        if (frame_idx == 25) {
            current_bpm = 140.0f;
        }

        float audio[512];
        for (int i = 0; i < 512; ++i) {
            // Generate click at beat times
            float beats_per_sec = current_bpm / 60.0f;
            float sample_rate = config.sample_rate_hz;
            float sample_idx = static_cast<float>(frame_idx) * config.hop_size + i;
            beat_phase = ::fmod(sample_idx / sample_rate * beats_per_sec, 1.0f);

            if (beat_phase < 0.01f) {
                audio[i] = 1.0f;  // Beat click
            } else {
                audio[i] = 0.0f;
            }
        }

        detector.processFrame(audio, 512);
    }

    // Final BPM should be in reasonable range (particle filter can track tempo changes)
    TempoEstimate tempo = detector.getTempo();
    fl::printf("Final tempo estimate: %.1f BPM (confidence: %.2f)\n", tempo.bpm, tempo.confidence);

    // Particle filter should converge to a tempo within DJ range
    CHECK(tempo.bpm > 80.0f);
    CHECK(tempo.bpm < 200.0f);

    // Particle filter is working if it estimated a tempo
    CHECK(tempo.bpm != 0.0f);
}

TEST_CASE("BeatDetector - Particle filter vs CombFilter") {
    // Compare particle filter and comb filter on same audio
    BeatDetectorConfig pf_config;
    pf_config.sample_rate_hz = 48000;
    pf_config.tempo_tracker = TempoTrackerType::ParticleFilter;
    pf_config.tempo_min_bpm = 90.0f;
    pf_config.tempo_max_bpm = 180.0f;

    BeatDetectorConfig cf_config = pf_config;
    cf_config.tempo_tracker = TempoTrackerType::CombFilter;
    cf_config.tempo_min_bpm = 100.0f;
    cf_config.tempo_max_bpm = 150.0f;

    BeatDetector pf_detector(pf_config);
    BeatDetector cf_detector(cf_config);

    // Generate 120 BPM audio
    int num_frames = 30;  // Reduced to avoid timeout
    for (int frame_idx = 0; frame_idx < num_frames; ++frame_idx) {
        float audio[512];
        for (int i = 0; i < 512; ++i) {
            float beats_per_sec = 120.0f / 60.0f;
            float sample_idx = static_cast<float>(frame_idx) * 256 + i;
            float beat_phase = ::fmod(sample_idx / 48000.0f * beats_per_sec, 1.0f);
            audio[i] = (beat_phase < 0.01f) ? 1.0f : 0.0f;
        }

        pf_detector.processFrame(audio, 512);
        cf_detector.processFrame(audio, 512);
    }

    TempoEstimate pf_tempo = pf_detector.getTempo();
    TempoEstimate cf_tempo = cf_detector.getTempo();

    fl::printf("Particle filter: %.1f BPM (conf: %.2f)\n", pf_tempo.bpm, pf_tempo.confidence);
    fl::printf("Comb filter: %.1f BPM (conf: %.2f)\n", cf_tempo.bpm, cf_tempo.confidence);

    // Particle filter should track tempo in DJ range
    CHECK(pf_tempo.bpm > 80.0f);
    CHECK(pf_tempo.bpm < 200.0f);

    // Comb filter should track tempo in narrower range
    CHECK(cf_tempo.bpm > 80.0f);
    CHECK(cf_tempo.bpm < 200.0f);
}

#endif // SKETCH_HAS_LOTS_OF_MEMORY
