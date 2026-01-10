
#include "fl/fx/audio/detectors/backbeat.h"
#include "fl/fx/audio/detectors/beat.h"
#include "fl/fx/audio/detectors/downbeat.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/math.h"
#include "fl/stl/new.h"
#include "doctest.h"
#include "fl/audio.h"
#include "fl/int.h"
#include "fl/math_macros.h"
#include "fl/slice.h"
#include "fl/stl/allocator.h"
#include "fl/stl/cstring.h"
#include "fl/stl/function.h"
#include "fl/stl/move.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/vector.h"

using namespace fl;

TEST_CASE("BackbeatDetector - Basic initialization") {
    // Test construction with own BeatDetector
    BackbeatDetector detector1;
    CHECK_EQ(detector1.isBackbeat(), false);
    CHECK_EQ(detector1.getConfidence(), 0.0f);
    CHECK_EQ(detector1.getStrength(), 0.0f);

    // Test construction with shared BeatDetector
    auto beatDetector = make_shared<BeatDetector>();
    BackbeatDetector detector2(beatDetector);
    CHECK_EQ(detector2.isBackbeat(), false);
    CHECK_EQ(detector2.getConfidence(), 0.0f);

    // Test construction with shared BeatDetector and DownbeatDetector
    auto downbeatDetector = make_shared<DownbeatDetector>(beatDetector);
    BackbeatDetector detector3(beatDetector, downbeatDetector);
    CHECK_EQ(detector3.isBackbeat(), false);
    CHECK_EQ(detector3.getConfidence(), 0.0f);
}

TEST_CASE("BackbeatDetector - Configuration") {
    BackbeatDetector detector;

    // Test threshold configuration
    detector.setConfidenceThreshold(0.8f);
    detector.setBassThreshold(1.5f);
    detector.setMidThreshold(1.4f);
    detector.setHighThreshold(1.2f);

    // Test backbeat mask configuration (beats 2 and 4 in 4/4)
    detector.setBackbeatExpectedBeats(0x0A);  // Bits 1 and 3

    // Test adaptive mode
    detector.setAdaptive(true);
    detector.setAdaptive(false);

    // Basic smoke test - should not crash
    CHECK(true);
}

TEST_CASE("BackbeatDetector - Reset functionality") {
    BackbeatDetector detector;

    // Set some configuration
    detector.setConfidenceThreshold(0.9f);
    detector.setAdaptive(true);

    // Reset should clear state but preserve configuration
    detector.reset();

    CHECK_EQ(detector.isBackbeat(), false);
    CHECK_EQ(detector.getLastBackbeatNumber(), 0);
    CHECK_EQ(detector.getConfidence(), 0.0f);
    CHECK_EQ(detector.getStrength(), 0.0f);
}

TEST_CASE("BackbeatDetector - Callbacks") {
    auto beatDetector = make_shared<BeatDetector>();
    BackbeatDetector detector(beatDetector);

    bool backbeat_called = false;
    u8 backbeat_number = 0;
    float backbeat_confidence = 0.0f;
    float backbeat_strength = 0.0f;

    detector.onBackbeat.add([&](u8 beatNumber, float confidence, float strength) {
        backbeat_called = true;
        backbeat_number = beatNumber;
        backbeat_confidence = confidence;
        backbeat_strength = strength;
    });

    // Create a simple audio context with synthetic audio
    vector<i16> samples(512, 0);

    // Generate a simple sine wave
    for (size i = 0; i < samples.size(); i++) {
        float phase = 2.0f * FL_M_PI * 440.0f * static_cast<float>(i) / 16000.0f;
        samples[i] = static_cast<i16>(0.5f * fl::sinf(phase) * 32767.0f);
    }

    AudioSample sample(fl::span<const i16>(samples.data(), samples.size()));
    auto context = make_shared<AudioContext>(sample);

    // Update detector
    detector.update(context);

    // Callbacks may or may not be triggered by this simple signal
    // Just verify the mechanism doesn't crash
    (void)backbeat_called;
    (void)backbeat_number;
    (void)backbeat_confidence;
    (void)backbeat_strength;

    CHECK(true);  // Smoke test passed
}

TEST_CASE("BackbeatDetector - Backbeat ratio") {
    BackbeatDetector detector;

    // Initial ratio should be 1.0 (neutral)
    float ratio = detector.getBackbeatRatio();
    CHECK_GE(ratio, 0.0f);
    CHECK_LE(ratio, 10.0f);  // Plausible range
}

TEST_CASE("BackbeatDetector - State access") {
    auto beatDetector = make_shared<BeatDetector>();
    auto downbeatDetector = make_shared<DownbeatDetector>(beatDetector);
    BackbeatDetector detector(beatDetector, downbeatDetector);

    // Test initial state
    CHECK_EQ(detector.isBackbeat(), false);
    CHECK_EQ(detector.getLastBackbeatNumber(), 0);
    CHECK_GE(detector.getConfidence(), 0.0f);
    CHECK_LE(detector.getConfidence(), 1.0f);
    CHECK_GE(detector.getStrength(), 0.0f);
    CHECK_GE(detector.getBackbeatRatio(), 0.0f);
}

TEST_CASE("BackbeatDetector - Detector dependencies") {
    auto beatDetector = make_shared<BeatDetector>();
    auto downbeatDetector = make_shared<DownbeatDetector>(beatDetector);
    BackbeatDetector detector;

    // Test setting detectors after construction
    detector.setBeatDetector(beatDetector);
    detector.setDownbeatDetector(downbeatDetector);

    // Create audio context
    vector<i16> samples(512, 0);
    AudioSample sample(fl::span<const i16>(samples.data(), samples.size()));
    auto context = make_shared<AudioContext>(sample);

    // Should not crash with shared detectors
    detector.update(context);
    detector.reset();

    CHECK(true);  // Smoke test passed
}

TEST_CASE("BackbeatDetector - AudioDetector interface") {
    BackbeatDetector detector;

    // Test AudioDetector interface methods
    CHECK_EQ(detector.needsFFT(), true);
    CHECK_EQ(detector.needsFFTHistory(), false);
    CHECK(detector.getName() != nullptr);

    // Verify name is correct
    const char* name = detector.getName();
    CHECK(fl::strcmp(name, "BackbeatDetector") == 0);
}

TEST_CASE("BackbeatDetector - Multiple update cycles") {
    auto beatDetector = make_shared<BeatDetector>();
    BackbeatDetector detector(beatDetector);

    // Create audio context
    vector<i16> samples(512, 0);

    // Generate silence
    AudioSample sample1(fl::span<const i16>(samples.data(), samples.size()));
    auto context1 = make_shared<AudioContext>(sample1);
    detector.update(context1);

    // Generate sine wave
    for (size i = 0; i < samples.size(); i++) {
        float phase = 2.0f * FL_M_PI * 440.0f * static_cast<float>(i) / 16000.0f;
        samples[i] = static_cast<i16>(0.5f * fl::sinf(phase) * 32767.0f);
    }

    AudioSample sample2(fl::span<const i16>(samples.data(), samples.size()));
    auto context2 = make_shared<AudioContext>(sample2);
    detector.update(context2);

    // Generate louder sine wave
    for (size i = 0; i < samples.size(); i++) {
        float phase = 2.0f * FL_M_PI * 880.0f * static_cast<float>(i) / 16000.0f;
        samples[i] = static_cast<i16>(0.8f * fl::sinf(phase) * 32767.0f);
    }

    AudioSample sample3(fl::span<const i16>(samples.data(), samples.size()));
    auto context3 = make_shared<AudioContext>(sample3);
    detector.update(context3);

    // Should not crash with multiple updates
    CHECK(true);  // Smoke test passed
}
