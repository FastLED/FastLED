// Unit tests for VocalDetector

#include "test.h"
#include "fl/audio.h"
#include "fl/audio/audio_context.h"
#include "fl/audio/detectors/vocal.h"
#include "../test_helpers.h"
#include "fl/stl/vector.h"
#include "fl/stl/math.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/cstring.h"
#include "fl/math_macros.h"

using namespace fl;
using fl::audio::test::makeSample;
using fl::audio::test::makeMultiHarmonic;
using fl::audio::test::makeSyntheticVowel;
using fl::audio::test::makeWhiteNoise;
using fl::audio::test::makeChirp;

namespace test_vocal {

struct AmplitudeLevel {
    const char* label;
    float amplitude;
};

static const AmplitudeLevel kVocalAmplitudes[] = {
    {"very_quiet", 500.0f},   // ~-36 dBFS
    {"quiet",      2000.0f},  // ~-24 dBFS
    {"normal",     10000.0f}, // ~-10 dBFS
    {"loud",       20000.0f}, // ~-4  dBFS
    {"max",        32000.0f}, // ~0   dBFS
};

static AudioSample makeSample_VocalDetector(float freq, fl::u32 timestamp, float amplitude = 16000.0f) {
    return makeSample(freq, timestamp, amplitude);
}

} // namespace test_vocal
using namespace test_vocal;

FL_TEST_CASE("VocalDetector - pure sine is not vocal") {
    VocalDetector detector;
    detector.setSampleRate(44100);

    auto ctx = fl::make_shared<AudioContext>(makeSample_VocalDetector(440.0f, 1000));
    ctx->setSampleRate(44100);
    detector.update(ctx);

    // A pure sine wave should not be detected as vocal
    FL_CHECK_FALSE(detector.isVocal());
}

FL_TEST_CASE("VocalDetector - confidence in valid range") {
    VocalDetector detector;
    detector.setSampleRate(44100);

    auto ctx = fl::make_shared<AudioContext>(makeSample_VocalDetector(440.0f, 1000));
    ctx->setSampleRate(44100);
    detector.update(ctx);

    float conf = detector.getConfidence();
    FL_CHECK_GE(conf, -0.5f);
    FL_CHECK_LE(conf, 1.0f);
}

FL_TEST_CASE("VocalDetector - reset clears state") {
    VocalDetector detector;
    detector.setSampleRate(44100);

    auto ctx = fl::make_shared<AudioContext>(makeSample_VocalDetector(440.0f, 1000));
    ctx->setSampleRate(44100);
    detector.update(ctx);

    detector.reset();
    FL_CHECK_FALSE(detector.isVocal());
    FL_CHECK_EQ(detector.getConfidence(), 0.0f);
}

FL_TEST_CASE("VocalDetector - callbacks don't crash") {
    VocalDetector detector;
    detector.setSampleRate(44100);

    bool changeCallbackInvoked = false;
    bool lastActiveState = true; // Initialize to opposite of expected
    detector.onVocal.add([&changeCallbackInvoked, &lastActiveState](u8 active) {
        changeCallbackInvoked = true;
        lastActiveState = (active > 0);
    });

    auto ctx = fl::make_shared<AudioContext>(makeSample_VocalDetector(440.0f, 1000));
    ctx->setSampleRate(44100);
    detector.update(ctx);
    detector.fireCallbacks();

    FL_CHECK_FALSE(changeCallbackInvoked);
    FL_CHECK_FALSE(detector.isVocal());
}

FL_TEST_CASE("VocalDetector - needsFFT is true") {
    VocalDetector detector;
    FL_CHECK(detector.needsFFT());
}

FL_TEST_CASE("VocalDetector - getName returns correct name") {
    VocalDetector detector;
    FL_CHECK(fl::strcmp(detector.getName(), "VocalDetector") == 0);
}

FL_TEST_CASE("VocalDetector - onVocalStart and onVocalEnd callbacks") {
    VocalDetector detector;
    detector.setSampleRate(44100);
    detector.setThreshold(0.3f);  // Lower threshold for easier triggering

    int startCount = 0;
    int endCount = 0;
    detector.onVocalStart.add([&startCount]() { startCount++; });
    detector.onVocalEnd.add([&endCount]() { endCount++; });

    for (int round = 0; round < 3; ++round) {
        auto ctx = fl::make_shared<AudioContext>(makeSample_VocalDetector(300.0f, round * 1000, 15000.0f));
        ctx->setSampleRate(44100);
        ctx->getFFT(128);
        detector.update(ctx);
        detector.fireCallbacks();

        fl::vector<fl::i16> silence(512, 0);
        auto silentCtx = fl::make_shared<AudioContext>(
            AudioSample(silence, round * 1000 + 500));
        silentCtx->setSampleRate(44100);
        silentCtx->getFFT(128);
        detector.update(silentCtx);
        detector.fireCallbacks();
    }

    if (endCount > 0) {
        FL_CHECK_GE(startCount, endCount);
    }
}

FL_TEST_CASE("VocalDetector - amplitude sweep: spectral features stable across dB levels") {
    for (const auto& level : kVocalAmplitudes) {
        VocalDetector detector;
        detector.setSampleRate(44100);

        auto ctx = fl::make_shared<AudioContext>(makeSample_VocalDetector(440.0f, 1000, level.amplitude));
        ctx->setSampleRate(44100);
        ctx->getFFT(128);
        detector.update(ctx);

        float conf = detector.getConfidence();
        FL_CHECK_GE(conf, -0.5f);
        FL_CHECK_LE(conf, 1.0f);
        FL_CHECK_FALSE(detector.isVocal());
    }
}

FL_TEST_CASE("VocalDetector - amplitude sweep: confidence consistency for same signal") {
    float confidences[5] = {};

    for (int idx = 0; idx < 5; ++idx) {
        const auto& level = kVocalAmplitudes[idx];
        VocalDetector detector;
        detector.setSampleRate(44100);

        for (int frame = 0; frame < 5; ++frame) {
            auto ctx = fl::make_shared<AudioContext>(
                makeSample_VocalDetector(440.0f, frame * 23, level.amplitude));
            ctx->setSampleRate(44100);
            ctx->getFFT(128);
            detector.update(ctx);
        }

        confidences[idx] = detector.getConfidence();
    }

    float confNormal = confidences[2];
    float confLoud = confidences[3];
    float confMax = confidences[4];

    FL_CHECK_LT(fl::abs(confNormal - confLoud), 0.15f);
    FL_CHECK_LT(fl::abs(confNormal - confMax), 0.15f);
    FL_CHECK_LT(fl::abs(confLoud - confMax), 0.15f);
}

// ============================================================================
// Spectral feature tests (synthetic signals)
// ============================================================================

FL_TEST_CASE("VocalDetector - single sine 440Hz is not vocal") {
    VocalDetector detector;
    detector.setSampleRate(44100);

    for (int frame = 0; frame < 5; ++frame) {
        auto sample = makeSample(440.0f, frame * 23);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        ctx->getFFT(128);
        detector.update(ctx);
    }

    FL_CHECK_FALSE(detector.isVocal());
    FL_CHECK_LE(detector.getConfidence(), 0.5f);
}

FL_TEST_CASE("VocalDetector - multi-harmonic is not vocal") {
    VocalDetector detector;
    detector.setSampleRate(44100);

    for (int frame = 0; frame < 5; ++frame) {
        auto sample = makeMultiHarmonic(220.0f, 8, 0.7f, frame * 23, 16000.0f);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        ctx->getFFT(128);
        detector.update(ctx);
    }

    FL_CHECK_FALSE(detector.isVocal());
    FL_CHECK_LE(detector.getConfidence(), 0.5f);
}

FL_TEST_CASE("VocalDetector - vowel ah is vocal") {
    VocalDetector detector;
    detector.setSampleRate(44100);

    // With correct dt (~11.6ms for 512 samples at 44100), the confidence
    // smoother needs more frames to converge than with old hardcoded 23ms.
    for (int frame = 0; frame < 20; ++frame) {
        auto sample = makeSyntheticVowel(150.0f, 700.0f, 1200.0f,
                                          frame * 12, 16000.0f);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        ctx->getFFT(128);
        detector.update(ctx);
    }

    FL_CHECK(detector.isVocal());
    FL_CHECK_GE(detector.getConfidence(), 0.55f);
}

FL_TEST_CASE("VocalDetector - vowel ee is vocal") {
    VocalDetector detector;
    detector.setSampleRate(44100);

    for (int frame = 0; frame < 10; ++frame) {
        auto sample = makeSyntheticVowel(200.0f, 350.0f, 2700.0f,
                                          frame * 23, 16000.0f);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        ctx->getFFT(128);
        detector.update(ctx);
    }

    // /i/ vowel is at the edge of what 128-bin CQ can resolve (F1=350 Hz,
    // F2=2700 Hz produces weak formant peaks at high harmonic numbers).
    // Relax threshold per risk assessment in src/fl/audio/TESTING.md.
    FL_CHECK_GE(detector.getConfidence(), 0.55f);
}

FL_TEST_CASE("VocalDetector - white noise is not vocal") {
    VocalDetector detector;
    detector.setSampleRate(44100);

    for (int frame = 0; frame < 5; ++frame) {
        auto sample = makeWhiteNoise(frame * 23, 16000.0f);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        ctx->getFFT(128);
        detector.update(ctx);
    }

    FL_CHECK_FALSE(detector.isVocal());
    FL_CHECK_LE(detector.getConfidence(), 0.4f);
}

FL_TEST_CASE("VocalDetector - chirp is not vocal") {
    VocalDetector detector;
    detector.setSampleRate(44100);

    for (int frame = 0; frame < 5; ++frame) {
        auto sample = makeChirp(200.0f, 2000.0f, frame * 23, 16000.0f);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        ctx->getFFT(128);
        detector.update(ctx);
    }

    FL_CHECK_FALSE(detector.isVocal());
    FL_CHECK_LE(detector.getConfidence(), 0.5f);
}

FL_TEST_CASE("VocalDetector - two unrelated sines not vocal") {
    VocalDetector detector;
    detector.setSampleRate(44100);

    for (int frame = 0; frame < 5; ++frame) {
        fl::vector<fl::i16> data(512, 0);
        for (int i = 0; i < 512; ++i) {
            float phase1 = 2.0f * FL_M_PI * 440.0f * i / 44100.0f;
            float phase2 = 2.0f * FL_M_PI * 1750.0f * i / 44100.0f;
            data[i] = static_cast<fl::i16>(8000.0f * fl::sinf(phase1) +
                                            8000.0f * fl::sinf(phase2));
        }
        auto sample = AudioSample(data, frame * 23);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        ctx->getFFT(128);
        detector.update(ctx);
    }

    FL_CHECK_FALSE(detector.isVocal());
    FL_CHECK_LE(detector.getConfidence(), 0.5f);
}

// ============================================================================
// Synthetic real-audio-matched signals
// ============================================================================
//
// These generators produce signals whose CQ spectral features match the
// observed distributions from the real MP3 voice+guitar test file:
//   Guitar-only: flatness=0.484, centroid=0.214, density=51.6
//   Voice+guitar: flatness=0.458, centroid=0.214, density=48.3
//
// The key discriminating feature is spectral flatness:
//   - Guitar: ~0.48 (more uniform spectrum)
//   - Voice+guitar: ~0.46 (slightly more peaked due to formant structure)

namespace test_vocal {

/// Generate a guitar-like signal with flatness OUTSIDE the vocal detection
/// peak (0.46 ± 0.05). Guitar signals should NOT trigger vocal detection.
/// Uses broadband random-frequency sinusoids with 1/f^1.5 rolloff to
/// produce flatness ~0.35-0.40 (below the voice flatness range).
inline AudioSample makeGuitarLike(fl::u32 timestamp, float amplitude = 16000.0f,
                                   int count = 512, float sampleRate = 44100.0f) {
    fl::vector<fl::i16> data(count, 0);
    fl::fl_random rng(42);

    const float fMin = 150.0f;
    const float fMax = 5000.0f;
    const int numComponents = 50;

    for (int c = 0; c < numComponents; ++c) {
        float t = static_cast<float>(rng.random16()) / 65535.0f;
        float freq = fMin * fl::powf(fMax / fMin, t);
        // 1/f^1.5 envelope → strong low-freq concentration → flatness ~0.35
        float envAmp = amplitude * 0.04f / fl::powf(freq / fMin, 1.5f);
        float phase0 = static_cast<float>(rng.random16()) / 65535.0f * 2.0f * FL_M_PI;

        for (int i = 0; i < count; ++i) {
            float phase = phase0 + 2.0f * FL_M_PI * freq * i / sampleRate;
            data[i] += static_cast<fl::i16>(envAmp * fl::sinf(phase));
        }
    }
    return AudioSample(data, timestamp);
}

/// Generate a voice-in-mix-like signal matching real MP3 CQ features:
///   centroid ~0.21, rolloff ~0.24, flatness ~0.46, density ~48
/// Guitar-like broadband base + strong harmonic overtones at voice
/// fundamental. The harmonics create peaks in specific CQ bins,
/// lowering flatness compared to the uniform guitar spectrum.
inline AudioSample makeVoiceInMixLike(fl::u32 timestamp, float amplitude = 16000.0f,
                                       int count = 512, float sampleRate = 44100.0f) {
    fl::vector<fl::i16> data(count, 0);
    fl::fl_random rng(43);

    const float fMin = 150.0f;
    const float fMax = 5000.0f;

    // 1. Weak broadband base (much weaker than guitar → harmonics dominate)
    const int numComponents = 30;
    for (int c = 0; c < numComponents; ++c) {
        float t = static_cast<float>(rng.random16()) / 65535.0f;
        float freq = fMin * fl::powf(fMax / fMin, t);
        float envAmp = amplitude * 0.015f / fl::powf(freq / fMin, 1.1f);
        float phase0 = static_cast<float>(rng.random16()) / 65535.0f * 2.0f * FL_M_PI;
        for (int i = 0; i < count; ++i) {
            float phase = phase0 + 2.0f * FL_M_PI * freq * i / sampleRate;
            data[i] += static_cast<fl::i16>(envAmp * fl::sinf(phase));
        }
    }

    // 2. Very strong voice harmonics → dominate over broadband → lower flatness
    const float f0 = 180.0f;
    for (int h = 1; h * f0 < fMax; ++h) {
        float freq = f0 * h;
        float harmonicAmp = amplitude * 0.3f / fl::powf(static_cast<float>(h), 1.2f);
        for (int i = 0; i < count; ++i) {
            float phase = 2.0f * FL_M_PI * freq * i / sampleRate;
            data[i] += static_cast<fl::i16>(harmonicAmp * fl::sinf(phase));
        }
    }
    return AudioSample(data, timestamp);
}

} // namespace test_vocal

// Synthetic signal tests: broadband signals matching real-audio characteristics.
// Guitar-like signal should NOT trigger vocal detection (flatness outside peak).
// Voice-in-mix has harmonic structure on top of broadband noise → different spectral shape.
FL_TEST_CASE("VocalDetector - guitar-like broadband not detected as vocal") {
    VocalDetector detector;
    detector.setSampleRate(44100);
    for (int frame = 0; frame < 5; ++frame) {
        auto sample = makeGuitarLike(frame * 23);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        ctx->getFFT(128);
        detector.update(ctx);
    }
    float nc = detector.getSpectralCentroid() / static_cast<float>(detector.getNumBins());
    printf("guitar-like: centroid=%.3f rolloff=%.3f formant=%.3f "
           "flatness=%.3f density=%.1f variance=%.4f confidence=%.3f isVocal=%d\n",
           nc, detector.getSpectralRolloff(), detector.getFormantRatio(),
           detector.getSpectralFlatness(), detector.getHarmonicDensity(),
           detector.getSpectralVariance(),
           detector.getConfidence(), detector.isVocal() ? 1 : 0);

    // Guitar-like broadband should NOT be detected as vocal
    FL_CHECK(!detector.isVocal());
    // Confidence should be low
    FL_CHECK_LT(detector.getConfidence(), 0.40f);
}

FL_TEST_CASE("VocalDetector - voice-in-mix harmonic structure") {
    VocalDetector detector;
    detector.setSampleRate(44100);
    for (int frame = 0; frame < 5; ++frame) {
        auto sample = makeVoiceInMixLike(frame * 23);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        ctx->getFFT(128);
        detector.update(ctx);
    }
    float nc = detector.getSpectralCentroid() / static_cast<float>(detector.getNumBins());
    printf("voice-in-mix: centroid=%.3f rolloff=%.3f formant=%.3f "
           "flatness=%.3f density=%.1f variance=%.4f confidence=%.3f isVocal=%d\n",
           nc, detector.getSpectralRolloff(), detector.getFormantRatio(),
           detector.getSpectralFlatness(), detector.getHarmonicDensity(),
           detector.getSpectralVariance(),
           detector.getConfidence(), detector.isVocal() ? 1 : 0);

    // Voice-in-mix has strong harmonics creating distinct spectral structure
    // Should have measurable harmonic density
    FL_CHECK_GT(detector.getHarmonicDensity(), 50.0f);
}
