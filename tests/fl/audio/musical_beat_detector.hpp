// Unit tests for MusicalBeatDetector - adversarial and boundary tests
// standalone test

#include "fl/audio/musical_beat_detector.h"
#include "fl/stl/math.h"
#include "fl/math_macros.h"

using namespace fl;

namespace {
// No helper functions needed in this file
} // anonymous namespace

// MB-1: Steady 120 BPM - Tight Validation
FL_TEST_CASE("MusicalBeatDetector - steady 120 BPM tight") {
    MusicalBeatDetector detector;
    MusicalBeatDetectorConfig config;
    config.minBPM = 50.0f;
    config.maxBPM = 250.0f;
    config.minBeatConfidence = 0.5f;
    config.bpmSmoothingAlpha = 0.9f;
    // Clean timing: sampleRate=10000, samplesPerFrame=100 -> 1 frame = 10ms
    // 120 BPM = 500ms per beat = 50 frames per beat
    config.sampleRate = 10000;
    config.samplesPerFrame = 100;
    detector.configure(config);

    u32 beatsDetected = 0;

    // 20 beats at exactly 50-frame intervals
    for (u32 beat = 0; beat < 20; ++beat) {
        for (u32 f = 0; f < 49; ++f) {
            detector.processSample(false, 0.0f);
        }
        detector.processSample(true, 1.0f);
        if (detector.isBeat()) {
            beatsDetected++;
        }
    }

    FL_CHECK_GE(beatsDetected, 18u); // 90% detection (not 70%)

    float finalBPM = detector.getBPM();
    FL_CHECK_GE(finalBPM, 115.0f);
    FL_CHECK_LE(finalBPM, 125.0f);

    FL_CHECK_GE(detector.getBeatConfidence(), 0.5f);
}

// MB-2: Random Onsets - Low False Positive Rate
FL_TEST_CASE("MusicalBeatDetector - random onset rejection tight") {
    MusicalBeatDetector detector;
    MusicalBeatDetectorConfig config;
    config.minBPM = 50.0f;
    config.maxBPM = 250.0f;
    config.minBeatConfidence = 0.5f;
    config.sampleRate = 10000;
    config.samplesPerFrame = 100;
    detector.configure(config);

    // Random intervals (not rhythmic) - mostly very short or very long
    const u32 randomIntervals[] = {5, 17, 3, 29, 11, 8, 23, 6, 19, 13,
                                    7, 25, 4, 31, 9, 14, 2, 27, 12, 16};
    const u32 numIntervals = sizeof(randomIntervals) / sizeof(randomIntervals[0]);

    u32 beatsDetected = 0;
    for (u32 i = 0; i < numIntervals; ++i) {
        for (u32 j = 0; j < randomIntervals[i]; ++j) {
            detector.processSample(false, 0.0f);
        }
        detector.processSample(true, 1.0f);
        if (detector.isBeat()) {
            beatsDetected++;
        }
    }

    FL_CHECK_LE(beatsDetected, 7u); // At most 35% validated

    // Most should be rejected
    const auto& stats = detector.getStats();
    FL_CHECK_GE(stats.rejectedOnsets, 10u);
}

// MB-3: First Beat Always Validates
FL_TEST_CASE("MusicalBeatDetector - first beat always validates") {
    MusicalBeatDetector detector;
    MusicalBeatDetectorConfig config;
    config.minBeatConfidence = 0.5f;
    config.sampleRate = 10000;
    config.samplesPerFrame = 100;
    detector.configure(config);

    // Wait enough frames that the IBI is valid (50 frames = 120 BPM)
    for (u32 f = 0; f < 49; ++f) {
        detector.processSample(false, 0.0f);
    }

    // First onset should always validate (mLastBeatFrame == 0)
    detector.processSample(true, 1.0f);

    const auto& stats = detector.getStats();
    FL_CHECK_EQ(stats.totalOnsets, 1u);
    FL_CHECK_GE(stats.validatedBeats, 1u);
}

// MB-4: Tempo Change Detection
// Uses 60-frame intervals in phase 2 (20% slower than 50-frame phase 1)
// which is within the ±25% validation tolerance.
FL_TEST_CASE("MusicalBeatDetector - tempo change detection") {
    MusicalBeatDetector detector;
    MusicalBeatDetectorConfig config;
    config.minBPM = 50.0f;
    config.maxBPM = 250.0f;
    config.minBeatConfidence = 0.3f;
    config.bpmSmoothingAlpha = 0.5f; // Faster adaptation
    config.sampleRate = 10000;
    config.samplesPerFrame = 100;
    detector.configure(config);

    // Phase 1: 10 beats at 120 BPM (50 frames per beat)
    for (u32 beat = 0; beat < 10; ++beat) {
        for (u32 f = 0; f < 49; ++f) detector.processSample(false, 0.0f);
        detector.processSample(true, 1.0f);
    }

    float bpmAfterPhase1 = detector.getBPM();
    FL_CHECK_GE(bpmAfterPhase1, 110.0f);
    FL_CHECK_LE(bpmAfterPhase1, 130.0f);

    // Phase 2: 10 beats at 100 BPM (60 frames per beat)
    // 60/50 = 1.20 (20% deviation), within ±25% tolerance
    // 60 frames * 100 / 10000 = 0.6s → 100 BPM
    for (u32 beat = 0; beat < 10; ++beat) {
        for (u32 f = 0; f < 59; ++f) detector.processSample(false, 0.0f);
        detector.processSample(true, 1.0f);
    }

    float bpmAfterPhase2 = detector.getBPM();
    // BPM should have shifted downward toward 100
    FL_CHECK_LT(bpmAfterPhase2, bpmAfterPhase1);
    FL_CHECK_GE(bpmAfterPhase2, 95.0f);
    FL_CHECK_LE(bpmAfterPhase2, 115.0f);
}

// MB-5: IBI History Ring Buffer
FL_TEST_CASE("MusicalBeatDetector - IBI ring buffer does not overflow") {
    MusicalBeatDetector detector;
    MusicalBeatDetectorConfig config;
    config.maxIBIHistory = 8;
    config.sampleRate = 10000;
    config.samplesPerFrame = 100;
    config.minBeatConfidence = 0.0f; // Accept all
    detector.configure(config);

    // Feed maxIBIHistory + 5 = 13 beats at steady tempo
    for (u32 beat = 0; beat < 13; ++beat) {
        for (u32 f = 0; f < 49; ++f) detector.processSample(false, 0.0f);
        detector.processSample(true, 1.0f);
    }

    // No crash, IBI count should not exceed maxIBIHistory
    const auto& stats = detector.getStats();
    FL_CHECK_LE(stats.ibiCount, 8u);

    // BPM should be stable
    float bpm = detector.getBPM();
    FL_CHECK_GE(bpm, 110.0f);
    FL_CHECK_LE(bpm, 130.0f);
}

// MB-6: Confidence Calculation - Perfect Consistency
FL_TEST_CASE("MusicalBeatDetector - high confidence for perfect tempo") {
    MusicalBeatDetector detector;
    MusicalBeatDetectorConfig config;
    config.minBeatConfidence = 0.0f; // Accept all to observe confidence
    config.sampleRate = 10000;
    config.samplesPerFrame = 100;
    detector.configure(config);

    // 10 beats at exactly 50-frame intervals (120 BPM, zero variance)
    for (u32 beat = 0; beat < 10; ++beat) {
        for (u32 f = 0; f < 49; ++f) detector.processSample(false, 0.0f);
        detector.processSample(true, 1.0f);
    }

    // With perfectly consistent intervals, confidence should be very high
    FL_CHECK_GE(detector.getBeatConfidence(), 0.8f);
}

// MB-7: Confidence Gate Blocks Low-Confidence Beats
FL_TEST_CASE("MusicalBeatDetector - confidence gate blocks isBeat") {
    MusicalBeatDetector detector;
    MusicalBeatDetectorConfig config;
    config.minBeatConfidence = 0.95f; // Very high threshold
    config.sampleRate = 10000;
    config.samplesPerFrame = 100;
    config.minBPM = 50.0f;
    config.maxBPM = 250.0f;
    detector.configure(config);

    // Feed slightly irregular beats (alternating 45/55 frame intervals)
    // These are within +-25% tolerance so validateBeat passes, but
    // the IBI variance gives confidence ~0.74, below 0.95 threshold
    const u32 intervals[] = {45, 55, 45, 55, 45, 55, 45, 55};
    const u32 numIntervals = sizeof(intervals) / sizeof(intervals[0]);

    u32 isBeatCount = 0;
    for (u32 i = 0; i < numIntervals; ++i) {
        for (u32 f = 0; f < intervals[i] - 1; ++f) {
            detector.processSample(false, 0.0f);
        }
        detector.processSample(true, 1.0f);
        if (detector.isBeat()) {
            isBeatCount++;
        }
    }

    // Beats ARE detected internally (validateBeat passes)
    FL_CHECK_GT(detector.getStats().validatedBeats, 0u);

    // But confidence < 0.95 means isBeat() returns false for most/all
    FL_CHECK_LT(isBeatCount, numIntervals);
}

// Keep: BPM range validation
FL_TEST_CASE("MusicalBeatDetector - BPM range rejection") {
    MusicalBeatDetector detector;
    MusicalBeatDetectorConfig config;
    config.minBPM = 100.0f;
    config.maxBPM = 150.0f;
    config.sampleRate = 10000;
    config.samplesPerFrame = 100;
    detector.configure(config);

    // 60 BPM = 1000ms per beat = 100 frames, below minBPM=100
    for (u32 i = 0; i < 5; ++i) {
        for (u32 f = 0; f < 99; ++f) detector.processSample(false, 0.0f);
        detector.processSample(true, 1.0f);
    }

    const auto& stats1 = detector.getStats();
    FL_CHECK_GE(stats1.rejectedOnsets, 3u);

    // Reset and test above maxBPM
    detector.reset();
    // 200 BPM = 300ms per beat = 30 frames, above maxBPM=150
    for (u32 i = 0; i < 5; ++i) {
        for (u32 f = 0; f < 29; ++f) detector.processSample(false, 0.0f);
        detector.processSample(true, 1.0f);
    }

    const auto& stats2 = detector.getStats();
    FL_CHECK_GE(stats2.rejectedOnsets, 3u);
}

// Keep: IBI average tracking
FL_TEST_CASE("MusicalBeatDetector - IBI average tracking") {
    MusicalBeatDetector detector;
    MusicalBeatDetectorConfig config;
    config.sampleRate = 10000;
    config.samplesPerFrame = 100;
    config.maxIBIHistory = 8;
    config.minBeatConfidence = 0.0f;
    detector.configure(config);

    // 10 beats at 50-frame intervals (120 BPM)
    for (u32 i = 0; i < 10; ++i) {
        for (u32 f = 0; f < 49; ++f) detector.processSample(false, 0.0f);
        detector.processSample(true, 1.0f);
    }

    float avgIBI = detector.getAverageIBI();
    float expectedIBI = 50.0f * 100.0f / 10000.0f; // 0.5s

    FL_CHECK_GT(avgIBI, expectedIBI * 0.9f);
    FL_CHECK_LT(avgIBI, expectedIBI * 1.1f);

    const auto& stats = detector.getStats();
    FL_CHECK_GT(stats.ibiCount, 0u);
    FL_CHECK_LE(stats.ibiCount, 8u);
}

// Keep: Reset
FL_TEST_CASE("MusicalBeatDetector - reset clears state") {
    MusicalBeatDetector detector;
    MusicalBeatDetectorConfig config;
    config.sampleRate = 10000;
    config.samplesPerFrame = 100;
    detector.configure(config);

    for (u32 i = 0; i < 5; ++i) {
        for (u32 f = 0; f < 49; ++f) detector.processSample(false, 0.0f);
        detector.processSample(true, 1.0f);
    }

    FL_CHECK_GT(detector.getStats().totalOnsets, 0u);

    detector.reset();

    const auto& stats = detector.getStats();
    FL_CHECK_EQ(stats.totalOnsets, 0u);
    FL_CHECK_EQ(stats.validatedBeats, 0u);
    FL_CHECK_EQ(stats.rejectedOnsets, 0u);
    FL_CHECK_EQ(stats.currentBPM, 120.0f);
    FL_CHECK_EQ(stats.ibiCount, 0u);
    FL_CHECK_FALSE(detector.isBeat());
}

// Keep: Statistics consistency
FL_TEST_CASE("MusicalBeatDetector - statistics consistency") {
    MusicalBeatDetector detector;
    MusicalBeatDetectorConfig config;
    config.minBPM = 50.0f;
    config.maxBPM = 250.0f;
    config.minBeatConfidence = 0.3f;
    config.sampleRate = 10000;
    config.samplesPerFrame = 100;
    detector.configure(config);

    // Mix of rhythmic and random onsets
    const u32 intervals[] = {50, 50, 50, 10, 50, 50, 30, 50, 50, 50};
    const u32 numIntervals = sizeof(intervals) / sizeof(intervals[0]);

    for (u32 i = 0; i < numIntervals; ++i) {
        for (u32 f = 0; f < intervals[i] - 1; ++f) {
            detector.processSample(false, 0.0f);
        }
        detector.processSample(true, 1.0f);
    }

    const auto& stats = detector.getStats();
    FL_CHECK_EQ(stats.totalOnsets, numIntervals);
    FL_CHECK_EQ(stats.totalOnsets, stats.validatedBeats + stats.rejectedOnsets);
    FL_CHECK_GT(stats.validatedBeats, 0u);
    FL_CHECK_GT(stats.rejectedOnsets, 0u);
}
