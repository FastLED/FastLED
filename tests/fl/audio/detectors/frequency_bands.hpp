// Unit tests for FrequencyBands — 3-band frequency detector with tone sweep validation

#include "fl/audio/detectors/frequency_bands.h"
#include "fl/audio/audio_context.h"
#include "fl/stl/math.h"
#include "fl/stl/shared_ptr.h"
#include "../test_helpers.h"

using namespace fl;
using fl::audio::test::makeSample;

// ============================================================================
// Tone sweep tests: continuous band motion and jitter detection
// ============================================================================

FL_TEST_CASE("FrequencyBands - tone sweep shows continuous band motion") {
    // Sweep a tone from 100 Hz to 8000 Hz. The FrequencyBands detector has:
    //   Bass: 20-250 Hz, Mid: 250-4000 Hz, Treble: 4000-20000 Hz
    // The raw energy should shift bass → mid → treble as frequency rises.

    FrequencyBands detector;

    const int numFrames = 300;
    const float startFreq = 100.0f;
    const float endFreq = 8000.0f;
    const float amplitude = 16000.0f;

    float bassHistory[300];
    float midHistory[300];
    float trebHistory[300];

    for (int frame = 0; frame < numFrames; ++frame) {
        float t = static_cast<float>(frame) / static_cast<float>(numFrames - 1);
        float freq = startFreq + (endFreq - startFreq) * t;

        auto sample = makeSample(freq, frame * 12, amplitude);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        detector.update(ctx);

        bassHistory[frame] = detector.getBass();
        midHistory[frame] = detector.getMid();
        trebHistory[frame] = detector.getTreble();
    }

    // --- Check 1: Each band should peak during the sweep ---
    float bassPeak = 0.0f, midPeak = 0.0f, trebPeak = 0.0f;
    int bassPeakFrame = 0, midPeakFrame = 0, trebPeakFrame = 0;
    for (int i = 0; i < numFrames; ++i) {
        if (bassHistory[i] > bassPeak) {
            bassPeak = bassHistory[i];
            bassPeakFrame = i;
        }
        if (midHistory[i] > midPeak) {
            midPeak = midHistory[i];
            midPeakFrame = i;
        }
        if (trebHistory[i] > trebPeak) {
            trebPeak = trebHistory[i];
            trebPeakFrame = i;
        }
    }

    FL_CHECK_GT(bassPeak, 0.0f);
    FL_CHECK_GT(midPeak, 0.0f);
    FL_CHECK_GT(trebPeak, 0.0f);

    // --- Check 2: Peaks should be ordered bass < mid < treb in time ---
    FASTLED_WARN("FreqBands sweep peak frames: bass=" << bassPeakFrame
                 << " mid=" << midPeakFrame << " treb=" << trebPeakFrame);
    FL_CHECK_LT(bassPeakFrame, midPeakFrame);
    FL_CHECK_LT(midPeakFrame, trebPeakFrame);

    // --- Check 3: Dominant band at known frequencies ---
    // At 150 Hz (bass band center), bass should dominate
    int bassCheckFrame = static_cast<int>(
        (150.0f - startFreq) / (endFreq - startFreq) * (numFrames - 1));
    FL_CHECK_GT(bassHistory[bassCheckFrame], midHistory[bassCheckFrame]);

    // At 1500 Hz (mid band), mid should dominate
    int midCheckFrame = static_cast<int>(
        (1500.0f - startFreq) / (endFreq - startFreq) * (numFrames - 1));
    FL_CHECK_GT(midHistory[midCheckFrame], bassHistory[midCheckFrame]);
    FL_CHECK_GT(midHistory[midCheckFrame], trebHistory[midCheckFrame]);

    // At 6000 Hz (treble band), treble should dominate
    int trebCheckFrame = static_cast<int>(
        (6000.0f - startFreq) / (endFreq - startFreq) * (numFrames - 1));
    FL_CHECK_GT(trebHistory[trebCheckFrame], bassHistory[trebCheckFrame]);
    FL_CHECK_GT(trebHistory[trebCheckFrame], midHistory[trebCheckFrame]);
}

FL_TEST_CASE("FrequencyBands - tone sweep has no jitter in raw band energy") {
    // Sweep a tone smoothly and check for jitter (high 2nd derivative).
    FrequencyBands detector;

    const int numFrames = 300;
    const float startFreq = 100.0f;
    const float endFreq = 8000.0f;

    float bassHistory[300];
    float midHistory[300];
    float trebHistory[300];

    for (int frame = 0; frame < numFrames; ++frame) {
        float t = static_cast<float>(frame) / static_cast<float>(numFrames - 1);
        float freq = startFreq + (endFreq - startFreq) * t;

        auto sample = makeSample(freq, frame * 12, 16000.0f);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        detector.update(ctx);

        bassHistory[frame] = detector.getBass();
        midHistory[frame] = detector.getMid();
        trebHistory[frame] = detector.getTreble();
    }

    // Compute max normalized 2nd derivative for each band
    auto computeJitter = [](const float* history, int n) -> float {
        float minVal = history[0], maxVal = history[0];
        for (int i = 1; i < n; ++i) {
            if (history[i] < minVal) minVal = history[i];
            if (history[i] > maxVal) maxVal = history[i];
        }
        float range = maxVal - minVal;
        if (range < 0.001f) return 0.0f;

        float maxAccel = 0.0f;
        for (int i = 1; i < n - 1; ++i) {
            float accel = fl::abs(
                history[i + 1] - 2.0f * history[i] + history[i - 1]);
            float normalized = accel / range;
            if (normalized > maxAccel) maxAccel = normalized;
        }
        return maxAccel;
    };

    float bassJitter = computeJitter(bassHistory, numFrames);
    float midJitter = computeJitter(midHistory, numFrames);
    float trebJitter = computeJitter(trebHistory, numFrames);

    FASTLED_WARN("FreqBands sweep jitter: bass=" << bassJitter
                 << " mid=" << midJitter << " treb=" << trebJitter);

    // With the exponential smoothing in FrequencyBands, transitions should be
    // smooth. Allow 0.5 threshold (same as VibeDetector).
    FL_CHECK_LT(bassJitter, 0.5f);
    FL_CHECK_LT(midJitter, 0.5f);
    FL_CHECK_LT(trebJitter, 0.5f);
}

FL_TEST_CASE("FrequencyBands - tone sweep normalized levels track smoothly") {
    // Test the per-band normalized values (0-1) during a sweep.
    FrequencyBands detector;

    // Warm up with broadband signal
    for (int i = 0; i < 100; ++i) {
        auto sample = fl::audio::test::makeWhiteNoise(i * 12, 16000.0f, 512);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        detector.update(ctx);
    }

    const int sweepFrames = 200;
    float bassNorm[200], midNorm[200], trebNorm[200];

    for (int frame = 0; frame < sweepFrames; ++frame) {
        float t = static_cast<float>(frame) / static_cast<float>(sweepFrames - 1);
        float freq = 100.0f + (8000.0f - 100.0f) * t;

        auto sample = makeSample(freq, (100 + frame) * 12, 16000.0f);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        detector.update(ctx);

        bassNorm[frame] = detector.getBassNorm();
        midNorm[frame] = detector.getMidNorm();
        trebNorm[frame] = detector.getTrebleNorm();
    }

    // All normalized values should be in [0, 1]
    for (int i = 0; i < sweepFrames; ++i) {
        FL_CHECK_GE(bassNorm[i], 0.0f);
        FL_CHECK_LE(bassNorm[i], 1.0f);
        FL_CHECK_GE(midNorm[i], 0.0f);
        FL_CHECK_LE(midNorm[i], 1.0f);
        FL_CHECK_GE(trebNorm[i], 0.0f);
        FL_CHECK_LE(trebNorm[i], 1.0f);
    }

    // Max frame-to-frame delta should be bounded (no sudden jumps)
    float bassMaxDelta = 0.0f, midMaxDelta = 0.0f, trebMaxDelta = 0.0f;
    for (int i = 1; i < sweepFrames; ++i) {
        float bd = fl::abs(bassNorm[i] - bassNorm[i - 1]);
        float md = fl::abs(midNorm[i] - midNorm[i - 1]);
        float td = fl::abs(trebNorm[i] - trebNorm[i - 1]);
        if (bd > bassMaxDelta) bassMaxDelta = bd;
        if (md > midMaxDelta) midMaxDelta = md;
        if (td > trebMaxDelta) trebMaxDelta = td;
    }

    FASTLED_WARN("FreqBands sweep max delta: bass=" << bassMaxDelta
                 << " mid=" << midMaxDelta << " treb=" << trebMaxDelta);

    // Normalized levels shouldn't jump more than 0.5 per frame
    FL_CHECK_LT(bassMaxDelta, 0.5f);
    FL_CHECK_LT(midMaxDelta, 0.5f);
    FL_CHECK_LT(trebMaxDelta, 0.5f);
}
