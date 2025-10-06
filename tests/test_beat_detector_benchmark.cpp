#include "test.h"

#if SKETCH_HAS_LOTS_OF_MEMORY

#include "fx/audio/beat_detector.h"
#include "fl/printf.h"
#include <math.h>

#ifdef FASTLED_TESTING
#include <chrono>
#endif

using namespace fl;

TEST_CASE("BeatDetector - Performance benchmark") {
#ifdef FASTLED_TESTING
    BeatDetectorConfig config;
    config.sample_rate_hz = 48000;
    config.frame_size = 512;
    config.hop_size = 256;
    config.fft_size = 512;
    config.odf_type = OnsetDetectionFunction::SuperFlux;
    config.peak_mode = PeakPickingMode::SuperFluxPeaks;
    config.tempo_tracker = TempoTrackerType::CombFilter;
    config.adaptive_whitening = true;
    config.log_compression = true;

    BeatDetector detector(config);

    // Generate test signal (complex waveform)
    fl::vector<float> signal(512);
    for (int i = 0; i < 512; ++i) {
        // Multi-frequency signal simulating music
        signal[i] = 0.3f * ::sin(2.0f * M_PI * 80.0f * i / 48000.0f)   // Bass/kick
                  + 0.2f * ::sin(2.0f * M_PI * 440.0f * i / 48000.0f)  // Mid
                  + 0.1f * ::sin(2.0f * M_PI * 8000.0f * i / 48000.0f); // High/hi-hat
    }

    // Warm-up (JIT, cache, etc.)
    for (int i = 0; i < 10; ++i) {
        detector.processFrame(signal.data(), 512);
    }

    detector.reset();

    // Benchmark: Process 1000 frames and measure time
    const int num_frames = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_frames; ++i) {
        detector.processFrame(signal.data(), 512);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Calculate metrics
    float total_time_ms = duration.count() / 1000.0f;
    float time_per_frame_ms = total_time_ms / num_frames;
    float frame_duration_ms = (256.0f / 48000.0f) * 1000.0f; // Hop size duration
    float cpu_usage_percent = (time_per_frame_ms / frame_duration_ms) * 100.0f;

    fl::printf("\n=== Beat Detector Performance Benchmark ===\n");
    fl::printf("Configuration:\n");
    fl::printf("  Sample rate: %.0f Hz\n", config.sample_rate_hz);
    fl::printf("  Frame size: %d samples\n", config.frame_size);
    fl::printf("  Hop size: %d samples\n", config.hop_size);
    fl::printf("  FFT size: %d\n", config.fft_size);
    fl::printf("  ODF: SuperFlux\n");
    fl::printf("  Peak picking: SuperFluxPeaks\n");
    fl::printf("  Tempo tracking: CombFilter\n");
    fl::printf("  Adaptive whitening: Yes\n");
    fl::printf("  Log compression: Yes\n");
    fl::printf("\nResults (%d frames processed):\n", num_frames);
    fl::printf("  Total time: %.2f ms\n", total_time_ms);
    fl::printf("  Time per frame: %.3f ms\n", time_per_frame_ms);
    fl::printf("  Frame duration (real-time): %.3f ms\n", frame_duration_ms);
    fl::printf("  CPU usage: %.1f%%\n", cpu_usage_percent);
    fl::printf("\nPerformance assessment:\n");

    // Check against requirements
    bool meets_latency = time_per_frame_ms < 8.0f;
    bool meets_cpu = cpu_usage_percent < 20.0f;

    fl::printf("  ✓ Latency requirement (<8ms/frame): %s (%.3f ms)\n",
               meets_latency ? "PASS" : "FAIL", time_per_frame_ms);
    fl::printf("  ✓ CPU requirement (<20%%): %s (%.1f%%)\n",
               meets_cpu ? "PASS" : "FAIL", cpu_usage_percent);

    if (meets_latency && meets_cpu) {
        fl::printf("\n✅ All performance targets met!\n");
    } else {
        fl::printf("\n⚠️  Some performance targets not met\n");
    }

    fl::printf("==========================================\n\n");

    // Verify tests pass
    CHECK_LT(time_per_frame_ms, 10.0f); // Allow some margin above 8ms target
    CHECK_LT(cpu_usage_percent, 30.0f); // Allow some margin above 20% target
#endif
}

TEST_CASE("BeatDetector - Memory footprint") {
    BeatDetectorConfig config;
    config.sample_rate_hz = 48000;
    config.frame_size = 512;
    config.hop_size = 256;
    config.fft_size = 512;

    fl::printf("\n=== Beat Detector Memory Footprint ===\n");
    fl::printf("Configuration size: %zu bytes\n", sizeof(BeatDetectorConfig));
    fl::printf("BeatDetector size: %zu bytes\n", sizeof(BeatDetector));
    fl::printf("OnsetEvent size: %zu bytes\n", sizeof(OnsetEvent));
    fl::printf("BeatEvent size: %zu bytes\n", sizeof(BeatEvent));
    fl::printf("TempoEstimate size: %zu bytes\n", sizeof(TempoEstimate));
    fl::printf("\nEstimated memory usage:\n");
    fl::printf("  BeatDetector instance: ~%zu KB\n", sizeof(BeatDetector) / 1024);
    fl::printf("  Target: <100 KB\n");
    fl::printf("  ESP32-S3 SRAM: 512 KB total\n");

    size_t detector_kb = sizeof(BeatDetector) / 1024;
    fl::printf("\n✓ Memory usage: %s\n",
               detector_kb < 100 ? "Acceptable" : "High");
    fl::printf("======================================\n\n");

    // Should be well under 512KB SRAM limit
    CHECK_LT(sizeof(BeatDetector), 100 * 1024); // Less than 100KB
}

TEST_CASE("BeatDetector - Accuracy test with synthetic beats") {
    BeatDetectorConfig config;
    config.sample_rate_hz = 48000;
    config.frame_size = 512;
    config.hop_size = 256;
    config.fft_size = 512;
    config.odf_type = OnsetDetectionFunction::SuperFlux;
    config.peak_mode = PeakPickingMode::SuperFluxPeaks;
    config.min_inter_onset_ms = 30;

    BeatDetector detector(config);

    // Generate synthetic beat pattern: 120 BPM (500ms per beat)
    const float bpm = 120.0f;
    const float beat_period_ms = 60000.0f / bpm; // 500ms
    const int samples_per_beat = static_cast<int>(beat_period_ms * config.sample_rate_hz / 1000.0f);

    fl::vector<float> detected_onset_times;
    fl::vector<float> expected_onset_times;

    detector.onOnset = [&](float confidence, float timestamp_ms) {
        detected_onset_times.push_back(timestamp_ms);
        fl::printf("Onset detected at %.1f ms (confidence: %.3f)\n", timestamp_ms, confidence);
    };

    // Generate 10 beats with impulses
    const int num_beats = 10;
    const int total_samples = num_beats * samples_per_beat;

    fl::vector<float> audio_track(total_samples, 0.0f);

    for (int beat = 0; beat < num_beats; ++beat) {
        int onset_sample = beat * samples_per_beat;

        // Create kick drum impulse (sharp attack in bass range)
        for (int i = 0; i < 200 && onset_sample + i < total_samples; ++i) {
            float envelope = ::exp(-i / 50.0f); // Decay envelope
            audio_track[onset_sample + i] = 0.8f * envelope * ::sin(2.0f * M_PI * 80.0f * i / config.sample_rate_hz);
        }

        float expected_time_ms = (onset_sample / config.sample_rate_hz) * 1000.0f;
        expected_onset_times.push_back(expected_time_ms);
    }

    // Process audio in chunks
    for (size_t i = 0; i + config.frame_size <= audio_track.size(); i += config.hop_size) {
        detector.processFrame(&audio_track[i], config.frame_size);
    }

    fl::printf("\n=== Onset Detection Accuracy ===\n");
    fl::printf("Expected beats: %d\n", num_beats);
    fl::printf("Detected onsets: %zu\n", detected_onset_times.size());

    // Calculate accuracy metrics
    int true_positives = 0;
    const float tolerance_ms = 50.0f; // 50ms tolerance window

    for (float expected_time : expected_onset_times) {
        bool found = false;
        for (float detected_time : detected_onset_times) {
            if (::fabs(detected_time - expected_time) < tolerance_ms) {
                found = true;
                break;
            }
        }
        if (found) true_positives++;
    }

    float precision = detected_onset_times.size() > 0
        ? static_cast<float>(true_positives) / detected_onset_times.size()
        : 0.0f;
    float recall = num_beats > 0
        ? static_cast<float>(true_positives) / num_beats
        : 0.0f;
    float f_measure = (precision + recall > 0.0f)
        ? 2.0f * precision * recall / (precision + recall)
        : 0.0f;

    fl::printf("\nAccuracy metrics (tolerance: %.0f ms):\n", tolerance_ms);
    fl::printf("  True positives: %d\n", true_positives);
    fl::printf("  Precision: %.2f\n", precision);
    fl::printf("  Recall: %.2f\n", recall);
    fl::printf("  F-measure: %.2f\n", f_measure);
    fl::printf("  Target: >0.80 for synthetic beats\n");

    fl::printf("\n%s\n", f_measure > 0.80f ? "✅ Good accuracy" : "⚠️  Accuracy could be improved");
    fl::printf("=================================\n\n");

    // Should detect most synthetic beats (allow some tolerance)
    CHECK_GE(true_positives, num_beats / 2); // At least 50% detection
}

TEST_CASE("BeatDetector - Different ODF comparison") {
    BeatDetectorConfig config;
    config.sample_rate_hz = 48000;
    config.frame_size = 512;
    config.hop_size = 256;
    config.fft_size = 512;

    fl::vector<float> test_signal(512);
    for (int i = 0; i < 512; ++i) {
        test_signal[i] = 0.5f * ::sin(2.0f * M_PI * 440.0f * i / 48000.0f);
    }

    fl::printf("\n=== ODF Performance Comparison ===\n");

#ifdef FASTLED_TESTING
    struct ODFTest {
        const char* name;
        OnsetDetectionFunction odf_type;
    };

    ODFTest tests[] = {
        {"Energy", OnsetDetectionFunction::Energy},
        {"SpectralFlux", OnsetDetectionFunction::SpectralFlux},
        {"SuperFlux", OnsetDetectionFunction::SuperFlux},
        {"HFC", OnsetDetectionFunction::HFC},
        {"MultiBand", OnsetDetectionFunction::MultiBand}
    };

    for (const auto& test : tests) {
        config.odf_type = test.odf_type;
        BeatDetector detector(config);

        // Warm-up
        for (int i = 0; i < 10; ++i) {
            detector.processFrame(test_signal.data(), 512);
        }

        detector.reset();

        // Benchmark
        const int num_frames = 1000;
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < num_frames; ++i) {
            detector.processFrame(test_signal.data(), 512);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        float time_per_frame_us = static_cast<float>(duration.count()) / num_frames;

        fl::printf("  %-15s: %.1f µs/frame\n", test.name, time_per_frame_us);
    }

    fl::printf("==================================\n\n");
#endif
}

#endif // SKETCH_HAS_LOTS_OF_MEMORY
