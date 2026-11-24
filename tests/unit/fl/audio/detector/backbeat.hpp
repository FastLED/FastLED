
#include "fl/audio/detector/backbeat.h"
#include "fl/audio/detector/beat.h"
#include "fl/audio/detector/downbeat.h"
#include "fl/audio/audio_context.h"
#include "fl/math/math.h"
#include "fl/stl/new.h"
#include "test.h"
#include "fl/audio/audio.h"
#include "fl/stl/int.h"
#include "fl/math/math.h"
#include "fl/stl/span.h"
#include "fl/stl/allocator.h"
#include "fl/stl/cstring.h"
#include "fl/stl/function.h"
#include "fl/stl/move.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/vector.h"

using namespace fl;

FL_TEST_CASE("audio::detector::Backbeat - Basic initialization") {
    // Test construction with own audio::detector::Beat
    audio::detector::Backbeat detector1;
    FL_CHECK_EQ(detector1.isBackbeat(), false);
    FL_CHECK_EQ(detector1.getConfidence(), 0.0f);
    FL_CHECK_EQ(detector1.getStrength(), 0.0f);

    // Test construction with shared audio::detector::Beat
    auto beatDetector = make_shared<audio::detector::Beat>();
    audio::detector::Backbeat detector2(beatDetector);
    FL_CHECK_EQ(detector2.isBackbeat(), false);
    FL_CHECK_EQ(detector2.getConfidence(), 0.0f);

    // Test construction with shared audio::detector::Beat and audio::detector::Downbeat
    auto downbeatDetector = make_shared<audio::detector::Downbeat>(beatDetector);
    audio::detector::Backbeat detector3(beatDetector, downbeatDetector);
    FL_CHECK_EQ(detector3.isBackbeat(), false);
    FL_CHECK_EQ(detector3.getConfidence(), 0.0f);
}

FL_TEST_CASE("audio::detector::Backbeat - Configuration") {
    audio::detector::Backbeat detector;

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
    FL_CHECK(true);
}

FL_TEST_CASE("audio::detector::Backbeat - Reset functionality") {
    audio::detector::Backbeat detector;

    // Set some configuration
    detector.setConfidenceThreshold(0.9f);
    detector.setAdaptive(true);

    // Reset should clear state but preserve configuration
    detector.reset();

    FL_CHECK_EQ(detector.isBackbeat(), false);
    FL_CHECK_EQ(detector.getLastBackbeatNumber(), 0);
    FL_CHECK_EQ(detector.getConfidence(), 0.0f);
    FL_CHECK_EQ(detector.getStrength(), 0.0f);
}

FL_TEST_CASE("audio::detector::Backbeat - Callbacks") {
    auto beatDetector = make_shared<audio::detector::Beat>();
    audio::detector::Backbeat detector(beatDetector);

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

    audio::Sample sample(samples);
    auto context = make_shared<audio::Context>(sample);

    // Update detector
    detector.update(context);

    // Callbacks may or may not be triggered by this simple signal
    // Just verify the mechanism doesn't crash
    (void)backbeat_called;
    (void)backbeat_number;
    (void)backbeat_confidence;
    (void)backbeat_strength;

    FL_CHECK(true);  // Smoke test passed
}

FL_TEST_CASE("audio::detector::Backbeat - Backbeat ratio") {
    audio::detector::Backbeat detector;

    // Initial ratio should be 1.0 (neutral)
    float ratio = detector.getBackbeatRatio();
    FL_CHECK_GE(ratio, 0.0f);
    FL_CHECK_LE(ratio, 10.0f);  // Plausible range
}

FL_TEST_CASE("audio::detector::Backbeat - State access") {
    auto beatDetector = make_shared<audio::detector::Beat>();
    auto downbeatDetector = make_shared<audio::detector::Downbeat>(beatDetector);
    audio::detector::Backbeat detector(beatDetector, downbeatDetector);

    // Test initial state
    FL_CHECK_EQ(detector.isBackbeat(), false);
    FL_CHECK_EQ(detector.getLastBackbeatNumber(), 0);
    FL_CHECK_GE(detector.getConfidence(), 0.0f);
    FL_CHECK_LE(detector.getConfidence(), 1.0f);
    FL_CHECK_GE(detector.getStrength(), 0.0f);
    FL_CHECK_GE(detector.getBackbeatRatio(), 0.0f);
}

FL_TEST_CASE("audio::detector::Backbeat - Detector dependencies") {
    auto beatDetector = make_shared<audio::detector::Beat>();
    auto downbeatDetector = make_shared<audio::detector::Downbeat>(beatDetector);
    audio::detector::Backbeat detector;

    // Test setting detector after construction
    detector.setBeatDetector(beatDetector);
    detector.setDownbeatDetector(downbeatDetector);

    // Create audio context
    vector<i16> samples(512, 0);
    audio::Sample sample(samples);
    auto context = make_shared<audio::Context>(sample);

    // Should not crash with shared detector
    detector.update(context);
    detector.reset();

    FL_CHECK(true);  // Smoke test passed
}

FL_TEST_CASE("audio::detector::Backbeat - audio::Detector interface") {
    audio::detector::Backbeat detector;

    // Test audio::Detector interface methods
    FL_CHECK_EQ(detector.needsFFT(), true);
    FL_CHECK_EQ(detector.needsFFTHistory(), false);
    FL_CHECK(detector.getName() != nullptr);

    // Verify name is correct
    const char* name = detector.getName();
    FL_CHECK(fl::strcmp(name, "Backbeat") == 0);
}

FL_TEST_CASE("audio::detector::Backbeat - Multiple update cycles") {
    auto beatDetector = make_shared<audio::detector::Beat>();
    audio::detector::Backbeat detector(beatDetector);

    // Create audio context
    vector<i16> samples(512, 0);

    // Generate silence
    audio::Sample sample1(samples);
    auto context1 = make_shared<audio::Context>(sample1);
    detector.update(context1);

    // Generate sine wave
    for (size i = 0; i < samples.size(); i++) {
        float phase = 2.0f * FL_M_PI * 440.0f * static_cast<float>(i) / 16000.0f;
        samples[i] = static_cast<i16>(0.5f * fl::sinf(phase) * 32767.0f);
    }

    audio::Sample sample2(samples);
    auto context2 = make_shared<audio::Context>(sample2);
    detector.update(context2);

    // Generate louder sine wave
    for (size i = 0; i < samples.size(); i++) {
        float phase = 2.0f * FL_M_PI * 880.0f * static_cast<float>(i) / 16000.0f;
        samples[i] = static_cast<i16>(0.8f * fl::sinf(phase) * 32767.0f);
    }

    audio::Sample sample3(samples);
    auto context3 = make_shared<audio::Context>(sample3);
    detector.update(context3);

    // Should not crash with multiple updates
    FL_CHECK(true);  // Smoke test passed
}
