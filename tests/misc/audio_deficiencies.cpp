// ok standalone
/// @file audio_deficiencies.cpp
/// @brief TDD tests for FastLED audio library deficiencies.
///
/// These tests assert the DESIRED (correct) behavior. They FAIL against
/// the current implementation, exposing each deficiency. The implementation
/// should be fixed until all tests pass.
///
/// Deficiencies tested:
///   1. Signal conditioning should be enabled by default
///   2. FrequencyBands should isolate bass from mid (pure bass → mid ≈ 0)
///   3. BeatDetector should NOT fire on pure treble transients
///   4. TempoAnalyzer should not penalize BPM values away from range midpoint
///   5. VocalDetector needs enough FFT bins to resolve vocal formants
///   6. FrequencyBands should produce comparable outputs for equal-energy input

#include "test.h"
#include "../fl/audio/test_helpers.h"
#include "fl/audio.h"
#include "fl/audio/audio_context.h"
#include "fl/fft.h"
#include "fl/fx/audio/audio_processor.h"
#include "fl/fx/audio/detectors/beat.h"
#include "fl/fx/audio/detectors/frequency_bands.h"
#include "fl/fx/audio/detectors/tempo_analyzer.h"
#include "fl/fx/audio/detectors/vocal.h"
#include "fl/stl/math.h"

using namespace fl;
using fl::audio::test::makeSample;
using fl::audio::test::generateDC;
using fl::audio::test::generateSine;

namespace {

constexpr float PI = 3.14159265358979f;

} // anonymous namespace

// =============================================================================
// 1. Signal conditioning SHOULD be enabled by default
// =============================================================================
// A user who creates an AudioProcessor and calls update() should get
// conditioned audio (DC removed, spikes filtered) without needing to know
// about setSignalConditioningEnabled(). MEMS microphones like the INMP441
// always have DC offset and occasional I2S glitches.

FL_TEST_CASE("Audio fix - DC offset removed by default") {
    AudioProcessor processor;

    // Sample with large DC offset (3000) — common with INMP441 MEMS mic
    vector<i16> pcm;
    generateSine(pcm, 512, 440.0f, 44100.0f, 5000);
    // Add DC offset
    for (auto& sample : pcm) {
        i32 val = static_cast<i32>(sample) + 3000;
        sample = static_cast<i16>(fl::max(-32768, fl::min(32767, val)));
    }

    processor.update(makeSample(pcm, 1000));

    // Measure DC offset of the sample that reached detectors
    const auto &processed = processor.getSample().pcm();
    i64 sum = 0;
    for (size i = 0; i < processed.size(); ++i) {
        sum += processed[i];
    }
    float meanDC =
        static_cast<float>(sum) / static_cast<float>(processed.size());

    // DESIRED: DC offset should be removed — mean should be near zero
    FL_CHECK_LT(fl::fl_abs(meanDC), 500.0f);
}

FL_TEST_CASE("Audio fix - I2S spike filtered by default") {
    AudioProcessor processor;

    // Sample with a large spike (I2S glitch)
    vector<i16> pcm;
    generateDC(pcm, 512, 0);
    pcm[100] = 30000; // spike well above normal signal range

    processor.update(makeSample(pcm, 1000));

    // DESIRED: spike should be filtered out (zeroed or clamped)
    const auto &processed = processor.getSample().pcm();
    FL_CHECK_LT(fl::fl_abs(static_cast<i32>(processed[100])), 15000);
}

// =============================================================================
// 2. FrequencyBands SHOULD isolate bass from mid
// =============================================================================
// A pure 100 Hz signal should produce strong bass and near-zero mid.
// The current linear bin mapping causes bin 0 (0-1378 Hz) to be shared
// by both bass and mid ranges, so mid is incorrectly non-zero.

FL_TEST_CASE("Audio fix - pure bass signal shows zero mid energy") {
    // Feed a pure 100 Hz bass signal through FrequencyBands
    vector<i16> pcm;
    generateSine(pcm, 1024, 100.0f, 44100.0f, 20000.0f);

    auto context = make_shared<AudioContext>(makeSample(pcm, 1000));
    context->setSampleRate(44100);

    FrequencyBands bands;
    bands.setSampleRate(44100);
    bands.setSmoothing(0.0f); // disable smoothing for immediate response

    context->setSample(makeSample(pcm, 1000));
    bands.update(context);

    float bass = bands.getBass();
    float mid = bands.getMid();
    float treble = bands.getTreble();

    // DESIRED: For a pure 100 Hz signal:
    //   - Bass should have significant energy
    //   - Mid should be near zero (100 Hz is NOT in the mid range)
    //   - Treble should be near zero
    FL_CHECK_GT(bass, 0.0f);
    FL_CHECK_LT(mid, bass * 0.1f);     // mid should be <10% of bass
    FL_CHECK_LT(treble, bass * 0.01f); // treble should be ~0
}

// =============================================================================
// 3. BeatDetector SHOULD NOT fire on pure treble transients
// =============================================================================
// A real beat detector should weight bass frequencies and use tempo-locked
// acceptance. A hi-hat (high-frequency transient) is not a musical beat.

FL_TEST_CASE("Audio fix - BeatDetector ignores treble-only transients") {
    BeatDetector detector;
    auto context = make_shared<AudioContext>(AudioSample());
    context->setSampleRate(44100);

    int beatCount = 0;
    detector.onBeat.add([&beatCount]() { beatCount++; });

    // 20 frames of silence to establish baseline
    vector<i16> silence;
    generateDC(silence, 512, 0);
    for (int i = 0; i < 20; ++i) {
        context->setSample(makeSample(silence));
        detector.update(context);
        detector.fireCallbacks();
    }
    FL_CHECK_EQ(beatCount, 0);

    // Inject a pure treble burst (8 kHz — hi-hat, NOT a musical beat)
    vector<i16> hihat;
    generateSine(hihat, 512, 8000.0f, 44100.0f, 20000.0f);
    context->setSample(makeSample(hihat, 500));
    detector.update(context);
    detector.fireCallbacks();

    // DESIRED: BeatDetector should NOT fire on a pure treble transient.
    // Musical beats are characterized by bass/low-mid energy (kick drum).
    FL_CHECK_EQ(beatCount, 0);
}

// =============================================================================
// 4. TempoAnalyzer SHOULD NOT bias toward range midpoint
// =============================================================================
// calculateIntervalScore currently returns higher scores for BPM values
// near the midpoint of [minBPM, maxBPM]. An 80 BPM hypothesis gets
// penalized 33% vs a 120 BPM match. All valid BPM values should
// score equally to allow unbiased detection.

FL_TEST_CASE("Audio fix - TempoAnalyzer scores all BPM values equally") {
    // The TempoAnalyzer::calculateIntervalScore should return the same score
    // for all BPM values within the valid range [minBPM, maxBPM].
    // Previously it used a center-biased formula that penalized BPM values
    // away from the midpoint. Now it returns 1.0 for all in-range BPM.

    TempoAnalyzer analyzer;
    analyzer.setMinBPM(60.0f);
    analyzer.setMaxBPM(180.0f);

    // Convert BPM to interval (ms) and score it via the analyzer
    auto scoreForBPM = [&](float bpm) -> float {
        u32 interval = static_cast<u32>(60000.0f / bpm);
        return analyzer.calculateIntervalScore(interval);
    };

    float score80 = scoreForBPM(80.0f);
    float score120 = scoreForBPM(120.0f);
    float score160 = scoreForBPM(160.0f);

    // DESIRED: All valid BPM values should produce the same score.
    // The tempo analyzer should be unbiased within its valid range.
    FL_CHECK_CLOSE(score80, score120, 0.05f);
    FL_CHECK_CLOSE(score120, score160, 0.05f);
}

// =============================================================================
// 5. VocalDetector SHOULD have sufficient frequency resolution
// =============================================================================
// At 44100 Hz with 16 bins, each bin is ~1378 Hz. The F1 vocal formant
// (500-900 Hz) fits inside a single bin, making formant detection impossible.
// The detector needs at least 64 bins (or log-spaced bins) to resolve F1/F2.

FL_TEST_CASE("Audio fix - VocalDetector F1 formant spans multiple FFT bins") {
    // The VocalDetector should request enough FFT bins so that the F1 vocal
    // formant range (500-900 Hz) spans at least 3 bins for meaningful
    // formant detection. With 128 bins at 44100 Hz:
    //   nyquist = 22050, hzPerBin = 22050/128 = 172.3 Hz
    //   F1 min bin = 500/172.3 = 2, F1 max bin = 900/172.3 = 5
    //   Span = 5 - 2 + 1 = 4 bins (sufficient)

    VocalDetector detector;
    const int numBins = detector.getNumBins();

    const float sampleRate = 44100.0f;
    const float nyquist = sampleRate / 2.0f;
    const float hzPerBin = nyquist / static_cast<float>(numBins);

    int f1MinBin = static_cast<int>(500.0f / hzPerBin);
    int f1MaxBin = static_cast<int>(900.0f / hzPerBin);

    // DESIRED: F1 formant should span at least 3 bins for meaningful detection
    int f1BinSpan = f1MaxBin - f1MinBin + 1;
    FL_CHECK_GE(f1BinSpan, 3);
}

// =============================================================================
// 6. FrequencyBands SHOULD produce comparable outputs for equal-energy input
// =============================================================================
// When equal-amplitude tones are present in bass, mid, and treble ranges,
// the three band outputs should be roughly equal. Without spectral EQ
// or normalization, mid dominates because it covers more FFT bins.

FL_TEST_CASE(
    "Audio fix - equal energy input produces comparable band outputs") {
    FrequencyBands bands;
    bands.setSampleRate(44100);
    bands.setSmoothing(0.0f);

    // Create signal with equal-amplitude sines in each band
    vector<i16> pcm;
    pcm.reserve(1024);
    float sr = 44100.0f;
    for (int i = 0; i < 1024; ++i) {
        float t = static_cast<float>(i) / sr;
        float bass = 5000.0f * fl::sinf(2.0f * PI * 100.0f * t);
        float mid = 5000.0f * fl::sinf(2.0f * PI * 1000.0f * t);
        float treb = 5000.0f * fl::sinf(2.0f * PI * 8000.0f * t);
        i32 combined = static_cast<i32>(bass + mid + treb);
        if (combined > 32767)
            combined = 32767;
        if (combined < -32768)
            combined = -32768;
        pcm.push_back(static_cast<i16>(combined));
    }

    auto context = make_shared<AudioContext>(makeSample(pcm, 1000));
    context->setSampleRate(44100);
    bands.update(context);

    float b = bands.getBass();
    float m = bands.getMid();
    float t = bands.getTreble();

    // DESIRED: For equal-energy input, band outputs should be comparable.
    // Allow 2x tolerance — they don't need to be identical, but should be
    // in the same ballpark. Without EQ, mid typically dominates 5-10x.
    float maxBand = fl::fl_max(b, fl::fl_max(m, t));
    float minBand = fl::fl_min(b, fl::fl_min(m, t));

    // All bands should be non-zero
    FL_CHECK_GT(b, 0.0f);
    FL_CHECK_GT(m, 0.0f);
    FL_CHECK_GT(t, 0.0f);

    // Max band should be within 2x of min band (properly equalized)
    FL_CHECK_LT(maxBand, minBand * 2.0f);
}

FL_TEST_CASE("Audio fix - FrequencyBands callbacks fire") {
    FrequencyBands bands;
    bands.setSampleRate(44100);
    bands.setSmoothing(0.0f);

    float lastBass = -1.0f;
    float lastMid = -1.0f;
    float lastTreble = -1.0f;
    bool levelsUpdated = false;

    bands.onBassLevel.add([&lastBass](float level) { lastBass = level; });
    bands.onMidLevel.add([&lastMid](float level) { lastMid = level; });
    bands.onTrebleLevel.add([&lastTreble](float level) { lastTreble = level; });
    bands.onLevelsUpdate.add([&levelsUpdated](float, float, float) { levelsUpdated = true; });

    // Feed a multi-frequency signal
    vector<i16> pcm;
    pcm.reserve(1024);
    for (int i = 0; i < 1024; ++i) {
        float t = static_cast<float>(i) / 44100.0f;
        float bass = 5000.0f * fl::sinf(2.0f * PI * 100.0f * t);
        float mid = 5000.0f * fl::sinf(2.0f * PI * 1000.0f * t);
        float treb = 5000.0f * fl::sinf(2.0f * PI * 8000.0f * t);
        i32 combined = static_cast<i32>(bass + mid + treb);
        if (combined > 32767) combined = 32767;
        if (combined < -32768) combined = -32768;
        pcm.push_back(static_cast<i16>(combined));
    }

    auto context = make_shared<AudioContext>(makeSample(pcm, 1000));
    context->setSampleRate(44100);
    bands.update(context);
    bands.fireCallbacks();

    // All callbacks should have fired
    FL_CHECK(levelsUpdated);
    FL_CHECK_GT(lastBass, 0.0f);
    FL_CHECK_GT(lastMid, 0.0f);
    FL_CHECK_GT(lastTreble, 0.0f);
}
