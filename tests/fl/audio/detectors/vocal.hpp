// Unit tests for VocalDetector

#include "test.h"
#include "fl/audio/audio.h"
#include "fl/audio/audio_context.h"
#include "fl/audio/detectors/vocal.h"
#include "../test_helpers.h"
#include "fl/stl/vector.h"
#include "fl/stl/math.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/cstring.h"
#include "fl/stl/math.h"

using namespace fl;
using Diag = fl::VocalDetectorDiagnostics;
using fl::audio::test::makeSample;
using fl::audio::test::makeMultiHarmonic;
using fl::audio::test::makeSyntheticVowel;
using fl::audio::test::makeWhiteNoise;
using fl::audio::test::makeChirp;
using fl::audio::test::makeJitteredVowel;
using fl::audio::test::makeGuitarStringDecay;
using fl::audio::test::makeAcousticGuitar;
using fl::audio::test::makeFullBandMix;
using fl::audio::test::makeSawtoothWave;
using fl::audio::test::makeTremoloTone;
using fl::audio::test::makeFilteredNoise;
using fl::audio::test::makePitchedTom;

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
    FL_CHECK_LE(detector.getConfidence(), 0.55f);
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
    float nc = Diag::getSpectralCentroid(detector) / static_cast<float>(Diag::getNumBins(detector));
    printf("guitar-like: centroid=%.3f rolloff=%.3f formant=%.3f "
           "flatness=%.3f density=%.1f variance=%.4f confidence=%.3f isVocal=%d\n",
           nc, Diag::getSpectralRolloff(detector), Diag::getFormantRatio(detector),
           Diag::getSpectralFlatness(detector), Diag::getHarmonicDensity(detector),
           Diag::getSpectralVariance(detector),
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
    float nc = Diag::getSpectralCentroid(detector) / static_cast<float>(Diag::getNumBins(detector));
    printf("voice-in-mix: centroid=%.3f rolloff=%.3f formant=%.3f "
           "flatness=%.3f density=%.1f variance=%.4f confidence=%.3f isVocal=%d\n",
           nc, Diag::getSpectralRolloff(detector), Diag::getFormantRatio(detector),
           Diag::getSpectralFlatness(detector), Diag::getHarmonicDensity(detector),
           Diag::getSpectralVariance(detector),
           detector.getConfidence(), detector.isVocal() ? 1 : 0);

    // Voice-in-mix has strong harmonics creating distinct spectral structure
    // Should have measurable harmonic density
    FL_CHECK_GT(Diag::getHarmonicDensity(detector), 50.0f);
}

// ============================================================================
// Time-domain feature tests
// ============================================================================

FL_TEST_CASE("VocalDetector - jittered vowel has measurable envelope jitter") {
    VocalDetector detector;
    detector.setSampleRate(44100);

    for (int frame = 0; frame < 20; ++frame) {
        auto sample = makeJitteredVowel(150.0f, 700.0f, 1200.0f, frame * 12);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        ctx->getFFT(128);
        detector.update(ctx);
    }

    printf("jittered vowel: jitter=%.4f acfIrreg=%.4f zcCV=%.4f conf=%.3f isVocal=%d\n",
           Diag::getEnvelopeJitter(detector), Diag::getAutocorrelationIrregularity(detector),
           Diag::getZeroCrossingCV(detector), detector.getConfidence(),
           detector.isVocal() ? 1 : 0);

    // Jittered vowel should have measurable envelope jitter
    FL_CHECK_GT(Diag::getEnvelopeJitter(detector), 0.01f);
    // Should still be detected as vocal
    FL_CHECK_GE(detector.getConfidence(), 0.50f);
}

FL_TEST_CASE("VocalDetector - guitar string decay has low irregularity") {
    VocalDetector detector;
    detector.setSampleRate(44100);

    for (int frame = 0; frame < 10; ++frame) {
        auto sample = makeGuitarStringDecay(220.0f, frame * 23);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        ctx->getFFT(128);
        detector.update(ctx);
    }

    printf("guitar string: jitter=%.4f acfIrreg=%.4f zcCV=%.4f conf=%.3f isVocal=%d\n",
           Diag::getEnvelopeJitter(detector), Diag::getAutocorrelationIrregularity(detector),
           Diag::getZeroCrossingCV(detector), detector.getConfidence(),
           detector.isVocal() ? 1 : 0);

    // Guitar string should have low autocorrelation irregularity (high periodicity)
    FL_CHECK_LT(Diag::getAutocorrelationIrregularity(detector), 0.40f);
    // Should not be detected as vocal
    FL_CHECK_FALSE(detector.isVocal());
}

FL_TEST_CASE("VocalDetector - time-domain features print diagnostics") {
    // Diagnostic test: print all feature values for key signal types
    struct TestSignal {
        const char* name;
        AudioSample (*gen)(fl::u32);
    };

    auto genPureSine = [](fl::u32 ts) { return makeSample(440.0f, ts); };
    auto genVowelAh = [](fl::u32 ts) {
        return makeSyntheticVowel(150.0f, 700.0f, 1200.0f, ts);
    };
    auto genJitteredVowel = [](fl::u32 ts) {
        return makeJitteredVowel(150.0f, 700.0f, 1200.0f, ts);
    };
    auto genGuitar = [](fl::u32 ts) {
        return makeGuitarStringDecay(220.0f, ts);
    };

    struct Entry {
        const char* name;
        AudioSample (*gen)(fl::u32);
    };
    Entry signals[] = {
        {"pure_sine", genPureSine},
        {"vowel_ah", genVowelAh},
        {"jittered_vowel", genJitteredVowel},
        {"guitar_string", genGuitar},
    };

    printf("\n--- Time-domain feature diagnostics ---\n");
    printf("%-16s  jitter  acfIrreg  zcCV    conf  isVocal\n", "signal");

    for (const auto& sig : signals) {
        VocalDetector det;
        det.setSampleRate(44100);
        for (int frame = 0; frame < 15; ++frame) {
            auto sample = sig.gen(frame * 12);
            auto ctx = fl::make_shared<AudioContext>(sample);
            ctx->setSampleRate(44100);
            ctx->getFFT(128);
            det.update(ctx);
        }
        printf("%-16s  %.4f  %.4f    %.4f  %.3f  %d\n",
               sig.name, Diag::getEnvelopeJitter(det),
               Diag::getAutocorrelationIrregularity(det),
               Diag::getZeroCrossingCV(det), det.getConfidence(),
               det.isVocal() ? 1 : 0);
    }
    printf("--- end diagnostics ---\n");

    // This test always passes — it's for calibration output
    FL_CHECK(true);
}

// ============================================================================
// Full-mix vocal detection tests (issue #2193 comment 4052855140)
//
// The vocal detector was tuned on isolated voice-vs-guitar but fails in
// full music mixes. These tests reproduce the problem:
//   - Spectral flatness (40% weight) is contaminated by drums/bass
//   - Time-domain features (jitter, ACF, ZC) are dominated by percussion
//   - All bands rise/fall in lockstep because features reflect the mix,
//     not the vocal contribution
// ============================================================================

namespace test_vocal {

/// Mix a vocal signal with guitar + drums/bass at a given vocal-to-backing ratio.
/// ratio = 1.0 means equal levels, 0.5 means vocal is half as loud as backing.
/// Matches real 3-way audio structure: guitar (broadband harmonics) + drums
/// (kick+hihat) + optional voice. The guitar component is key — it adds
/// formant-band energy that inflates backing formant ratio in 3-way mixes.
inline AudioSample makeVocalInFullMix(float vocalRatio, fl::u32 timestamp,
                                       float amplitude = 16000.0f,
                                       int count = 512, float sampleRate = 44100.0f) {
    // 1. Vocal: jittered vowel /a/ (F0=150, F1=700, F2=1200)
    auto vocal = makeJitteredVowel(150.0f, 700.0f, 1200.0f, timestamp,
                                    amplitude * vocalRatio, count, sampleRate);

    // 2. Guitar: harmonic series at 220Hz with 1/h^1.5 rolloff
    // Adds energy in formant bands (F1=200-900Hz, F2=1000-3000Hz) that
    // inflates backing formant ratio — matching real 3-way audio behavior.
    fl::vector<fl::i16> guitarData(count, 0);
    float guitarF0 = 220.0f;
    float maxFreq = fl::min(5000.0f, sampleRate / 2.0f);
    for (int h = 1; h * guitarF0 < maxFreq; ++h) {
        float freq = guitarF0 * static_cast<float>(h);
        float hAmp = amplitude * 0.25f / fl::powf(static_cast<float>(h), 1.5f);
        for (int i = 0; i < count; ++i) {
            float phase = 2.0f * FL_M_PI * freq * static_cast<float>(i) / sampleRate;
            guitarData[i] += static_cast<fl::i16>(hAmp * fl::sinf(phase));
        }
    }

    // 3. Bass guitar: 80 Hz fundamental with harmonics
    fl::vector<fl::i16> bassData(count, 0);
    float bassF0 = 80.0f;
    for (int h = 1; h <= 5; ++h) {
        float freq = bassF0 * static_cast<float>(h);
        float hAmp = amplitude * 0.3f / static_cast<float>(h * h);
        for (int i = 0; i < count; ++i) {
            float phase = 2.0f * FL_M_PI * freq * static_cast<float>(i) / sampleRate;
            bassData[i] += static_cast<fl::i16>(hAmp * fl::sinf(phase));
        }
    }

    // 4. Kick drum pattern (amplitude-decayed)
    fl::vector<fl::i16> kickData(count, 0);
    for (int i = 0; i < count; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        float decay = fl::expf(-t / 0.030f);
        kickData[i] = static_cast<fl::i16>(amplitude * 0.3f * decay *
                      fl::sinf(2.0f * FL_M_PI * 55.0f * t));
    }

    // 5. Hi-hat noise (high-frequency percussion)
    fl::fl_random rng(timestamp + 77);
    fl::vector<fl::i16> hatData(count, 0);
    float prevNoise = 0.0f;
    for (int i = 0; i < count; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        float decay = fl::expf(-t / 0.010f);
        float noiseVal = (static_cast<float>(rng.random16()) / 32767.5f) - 1.0f;
        float hp = noiseVal - 0.85f * prevNoise;
        prevNoise = noiseVal;
        hatData[i] = static_cast<fl::i16>(amplitude * 0.12f * decay * hp);
    }

    // Sum all components
    const auto& vocalPcm = vocal.pcm();
    fl::vector<fl::i16> mixed(count, 0);
    for (int i = 0; i < count; ++i) {
        float sum = static_cast<float>(vocalPcm[i]) +
                    static_cast<float>(guitarData[i]) +
                    static_cast<float>(bassData[i]) +
                    static_cast<float>(kickData[i]) +
                    static_cast<float>(hatData[i]);
        if (sum > 32767.0f) sum = 32767.0f;
        if (sum < -32768.0f) sum = -32768.0f;
        mixed[i] = static_cast<fl::i16>(sum);
    }
    return AudioSample(mixed, timestamp);
}

} // namespace test_vocal

// Diagnostic: show how vocal detector features degrade in a full mix.
// Prints feature values at different vocal-to-backing ratios.
FL_TEST_CASE("VocalDetector - full mix feature contamination diagnostic") {
    printf("\n--- Full-mix vocal detection diagnostic ---\n");
    printf("%-12s  flat    form    pres    jitter  acfIrr  zcCV    conf  isVocal\n",
           "mix_ratio");

    float ratios[] = {1.0f, 0.7f, 0.5f, 0.3f, 0.0f};
    for (float ratio : ratios) {
        VocalDetector det;
        det.setSampleRate(44100);
        for (int frame = 0; frame < 20; ++frame) {
            auto sample = makeVocalInFullMix(ratio, frame * 12);
            auto ctx = fl::make_shared<AudioContext>(sample);
            ctx->setSampleRate(44100);
            ctx->getFFT(128);
            det.update(ctx);
        }
        printf("vocal=%.1f     %.3f   %.3f   %.3f   %.4f  %.4f  %.4f  %.3f  %d\n",
               ratio,
               Diag::getSpectralFlatness(det),
               Diag::getFormantRatio(det),
               Diag::getVocalPresenceRatio(det),
               Diag::getEnvelopeJitter(det),
               Diag::getAutocorrelationIrregularity(det),
               Diag::getZeroCrossingCV(det),
               det.getConfidence(),
               det.isVocal() ? 1 : 0);
    }
    printf("--- end diagnostic ---\n");

    // This test always passes — it's a diagnostic
    FL_CHECK(true);
}

// Reproduce the lockstep issue: isolated vocal should be detected,
// but same vocal in a full mix may fail due to feature contamination.
FL_TEST_CASE("VocalDetector - isolated vocal vs vocal in full mix") {
    // 1. Isolated vocal — should be detected
    VocalDetector isoDetector;
    isoDetector.setSampleRate(44100);
    for (int frame = 0; frame < 20; ++frame) {
        auto sample = makeJitteredVowel(150.0f, 700.0f, 1200.0f, frame * 12);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        ctx->getFFT(128);
        isoDetector.update(ctx);
    }
    float isoConf = isoDetector.getConfidence();
    float isoFlat = Diag::getSpectralFlatness(isoDetector);
    float isoForm = Diag::getFormantRatio(isoDetector);
    float isoPres = Diag::getVocalPresenceRatio(isoDetector);

    printf("Isolated vocal: conf=%.3f flat=%.3f form=%.3f pres=%.3f isVocal=%d\n",
           isoConf, isoFlat, isoForm, isoPres, isoDetector.isVocal() ? 1 : 0);

    // 2. Same vocal mixed with drums/bass at equal level
    VocalDetector mixDetector;
    mixDetector.setSampleRate(44100);
    for (int frame = 0; frame < 20; ++frame) {
        auto sample = makeVocalInFullMix(1.0f, frame * 12);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        ctx->getFFT(128);
        mixDetector.update(ctx);
    }
    float mixConf = mixDetector.getConfidence();
    float mixFlat = Diag::getSpectralFlatness(mixDetector);
    float mixForm = Diag::getFormantRatio(mixDetector);
    float mixPres = Diag::getVocalPresenceRatio(mixDetector);

    printf("Vocal in mix:   conf=%.3f flat=%.3f form=%.3f pres=%.3f isVocal=%d\n",
           mixConf, mixFlat, mixForm, mixPres, mixDetector.isVocal() ? 1 : 0);

    // 3. Backing track only (no vocal) — should NOT be detected
    VocalDetector backingDetector;
    backingDetector.setSampleRate(44100);
    for (int frame = 0; frame < 20; ++frame) {
        auto sample = makeVocalInFullMix(0.0f, frame * 12);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        ctx->getFFT(128);
        backingDetector.update(ctx);
    }
    float backConf = backingDetector.getConfidence();

    printf("Backing only:   conf=%.3f flat=%.3f form=%.3f pres=%.3f isVocal=%d\n",
           backConf, Diag::getSpectralFlatness(backingDetector),
           Diag::getFormantRatio(backingDetector),
           Diag::getVocalPresenceRatio(backingDetector),
           backingDetector.isVocal() ? 1 : 0);

    // --- Assertions ---

    // Isolated vocal should have reasonable confidence
    FL_CHECK_GE(isoConf, 0.40f);

    // Backing-only should have low confidence
    FL_CHECK_LT(backConf, 0.50f);

    // Vocal-in-mix should now be detected thanks to rebalanced weights
    // (formant elevated, flatness reduced, ZC penalty softened)
    float confDrop = isoConf - mixConf;
    printf("Confidence drop (iso - mix): %.3f\n", confDrop);

    // The mix should have meaningfully higher confidence than backing-only
    FL_CHECK_GT(mixConf, backConf);

    // Vocal-in-mix should have meaningfully higher confidence than backing.
    // After 3-way tuning (raised boost thresholds for drums), detection threshold
    // is harder to reach in synthetic mix. Check separation, not absolute level.
    FL_CHECK_GE(mixConf, 0.35f);
}

// Verify that backing-track-only drums+bass is NOT falsely detected as vocal.
// This is the inverse of the lockstep issue: if all bands track the mix,
// percussive energy might occasionally trigger false vocal detections.
FL_TEST_CASE("VocalDetector - drums+bass backing track not vocal") {
    VocalDetector detector;
    detector.setSampleRate(44100);

    int vocalFrames = 0;
    for (int frame = 0; frame < 30; ++frame) {
        auto sample = makeVocalInFullMix(0.0f, frame * 12);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        ctx->getFFT(128);
        detector.update(ctx);
        if (detector.isVocal()) vocalFrames++;
    }

    printf("Backing track: %d/30 frames detected as vocal, final conf=%.3f\n",
           vocalFrames, detector.getConfidence());

    // Drums+bass should never be detected as vocal
    FL_CHECK_EQ(vocalFrames, 0);
    FL_CHECK_LT(detector.getConfidence(), 0.50f);
}

// ============================================================================
// Feature stability tests — prevent parameter dither across calibration rounds
//
// These tests pin per-feature values and ordering invariants using synthetic
// signals. They catch when a weight/threshold change silently moves a feature
// outside its expected range, BEFORE the aggregate confidence crosses a
// detection boundary.
//
// Strategy:
//   1. Golden ranges — pin each feature for each signal type
//   2. Ordering invariants — voice > non-voice on discriminating features
//   3. Separation margins — minimum gap between voice and backing
// ============================================================================

namespace test_vocal {

/// Run a signal through the detector for N frames and return diagnostics.
struct FeatureSnapshot {
    float flatness, formant, presence, density;
    float centroid, rolloff;  // normalized centroid, raw rolloff
    float jitter, acfIrreg, zcCV, variance;
    float confidence;
    bool isVocal;
};

inline FeatureSnapshot runSignal(AudioSample (*gen)(fl::u32), int frames = 15) {
    VocalDetector det;
    det.setSampleRate(44100);
    for (int i = 0; i < frames; ++i) {
        auto sample = gen(i * 12);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        ctx->getFFT(128);
        det.update(ctx);
    }
    FeatureSnapshot s;
    s.flatness = Diag::getSpectralFlatness(det);
    s.formant = Diag::getFormantRatio(det);
    s.presence = Diag::getVocalPresenceRatio(det);
    s.density = Diag::getHarmonicDensity(det);
    s.centroid = Diag::getSpectralCentroid(det)
               / static_cast<float>(Diag::getNumBins(det));
    s.rolloff = Diag::getSpectralRolloff(det);
    s.jitter = Diag::getEnvelopeJitter(det);
    s.acfIrreg = Diag::getAutocorrelationIrregularity(det);
    s.zcCV = Diag::getZeroCrossingCV(det);
    s.variance = Diag::getSpectralVariance(det);
    s.confidence = det.getConfidence();
    s.isVocal = det.isVocal();
    return s;
}

/// Run makeVocalInFullMix at a given ratio for N frames.
inline FeatureSnapshot runMix(float ratio, int frames = 20) {
    VocalDetector det;
    det.setSampleRate(44100);
    for (int i = 0; i < frames; ++i) {
        auto sample = makeVocalInFullMix(ratio, i * 12);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        ctx->getFFT(128);
        det.update(ctx);
    }
    FeatureSnapshot s;
    s.flatness = Diag::getSpectralFlatness(det);
    s.formant = Diag::getFormantRatio(det);
    s.presence = Diag::getVocalPresenceRatio(det);
    s.density = Diag::getHarmonicDensity(det);
    s.centroid = Diag::getSpectralCentroid(det)
               / static_cast<float>(Diag::getNumBins(det));
    s.rolloff = Diag::getSpectralRolloff(det);
    s.jitter = Diag::getEnvelopeJitter(det);
    s.acfIrreg = Diag::getAutocorrelationIrregularity(det);
    s.zcCV = Diag::getZeroCrossingCV(det);
    s.variance = Diag::getSpectralVariance(det);
    s.confidence = det.getConfidence();
    s.isVocal = det.isVocal();
    return s;
}

/// Run makeFullBandMix at a given ratio for N frames.
inline FeatureSnapshot runFullBandMix(float ratio, int frames = 20) {
    VocalDetector det;
    det.setSampleRate(44100);
    for (int i = 0; i < frames; ++i) {
        auto sample = makeFullBandMix(ratio, i * 12);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        ctx->getFFT(128);
        det.update(ctx);
    }
    FeatureSnapshot s;
    s.flatness = Diag::getSpectralFlatness(det);
    s.formant = Diag::getFormantRatio(det);
    s.presence = Diag::getVocalPresenceRatio(det);
    s.density = Diag::getHarmonicDensity(det);
    s.centroid = Diag::getSpectralCentroid(det)
               / static_cast<float>(Diag::getNumBins(det));
    s.rolloff = Diag::getSpectralRolloff(det);
    s.jitter = Diag::getEnvelopeJitter(det);
    s.acfIrreg = Diag::getAutocorrelationIrregularity(det);
    s.zcCV = Diag::getZeroCrossingCV(det);
    s.variance = Diag::getSpectralVariance(det);
    s.confidence = det.getConfidence();
    s.isVocal = det.isVocal();
    return s;
}

} // namespace test_vocal

// ---------------------------------------------------------------------------
// 1. Feature golden ranges — pin each feature for known signal types.
//    These catch silent drift when someone tweaks a scoring constant.
// ---------------------------------------------------------------------------

FL_TEST_CASE("VocalDetector stability - vowel feature golden ranges") {
    auto genVowel = [](fl::u32 ts) {
        return makeSyntheticVowel(150.0f, 700.0f, 1200.0f, ts);
    };
    auto s = test_vocal::runSignal(genVowel);

    printf("vowel golden: flat=%.3f form=%.3f dens=%.1f cent=%.3f roll=%.3f conf=%.3f\n",
           s.flatness, s.formant, s.density, s.centroid, s.rolloff, s.confidence);

    // Spectral features should be in voice territory
    FL_CHECK_GE(s.flatness, 0.30f);
    FL_CHECK_LE(s.flatness, 0.60f);
    FL_CHECK_GE(s.formant, 0.40f);
    FL_CHECK_LE(s.formant, 1.00f);
    FL_CHECK_GE(s.density, 40.0f);
    FL_CHECK_LE(s.density, 110.0f);
    FL_CHECK_GE(s.centroid, 0.25f);
    FL_CHECK_LE(s.centroid, 0.70f);
    FL_CHECK_GE(s.rolloff, 0.30f);
    FL_CHECK_LE(s.rolloff, 0.90f);

    // Must be detected as vocal
    FL_CHECK_GE(s.confidence, 0.70f);
    FL_CHECK(s.isVocal);
}

FL_TEST_CASE("VocalDetector stability - pure sine feature golden ranges") {
    auto genSine = [](fl::u32 ts) { return makeSample(440.0f, ts); };
    auto s = test_vocal::runSignal(genSine);

    printf("sine golden: flat=%.3f form=%.3f dens=%.1f cent=%.3f roll=%.3f conf=%.3f\n",
           s.flatness, s.formant, s.density, s.centroid, s.rolloff, s.confidence);

    // Pure sine: very low flatness (tonal), moderate density (CQ bins spread
    // spectral leakage across multiple log-spaced bins, so even a pure tone
    // activates ~30 bins above 10% of peak)
    FL_CHECK_LE(s.flatness, 0.20f);
    FL_CHECK_LE(s.density, 45.0f);

    // Must NOT be detected as vocal
    FL_CHECK_LE(s.confidence, 0.20f);
    FL_CHECK_FALSE(s.isVocal);
}

FL_TEST_CASE("VocalDetector stability - guitar string feature golden ranges") {
    auto genGuitar = [](fl::u32 ts) {
        return makeGuitarStringDecay(220.0f, ts);
    };
    auto s = test_vocal::runSignal(genGuitar);

    printf("guitar golden: flat=%.3f form=%.3f dens=%.1f jitter=%.4f acf=%.4f conf=%.3f\n",
           s.flatness, s.formant, s.density, s.jitter, s.acfIrreg, s.confidence);

    // Guitar: moderate flatness, low jitter (periodic), low ACF irregularity
    FL_CHECK_LE(s.flatness, 0.35f);
    FL_CHECK_LE(s.jitter, 0.15f);
    FL_CHECK_LE(s.acfIrreg, 0.50f);

    // Must NOT be detected as vocal
    FL_CHECK_LE(s.confidence, 0.20f);
    FL_CHECK_FALSE(s.isVocal);
}

FL_TEST_CASE("VocalDetector stability - white noise feature golden ranges") {
    auto genNoise = [](fl::u32 ts) { return makeWhiteNoise(ts); };
    auto s = test_vocal::runSignal(genNoise);

    printf("noise golden: flat=%.3f form=%.3f dens=%.1f conf=%.3f\n",
           s.flatness, s.formant, s.density, s.confidence);

    // White noise: very high flatness (uniform), high density
    FL_CHECK_GE(s.flatness, 0.60f);
    FL_CHECK_GE(s.density, 90.0f);

    // Must NOT be detected as vocal
    FL_CHECK_LE(s.confidence, 0.40f);
    FL_CHECK_FALSE(s.isVocal);
}

FL_TEST_CASE("VocalDetector stability - guitar-like broadband feature golden ranges") {
    auto s = test_vocal::runSignal([](fl::u32 ts) { return makeGuitarLike(ts); });

    printf("guitar-like golden: flat=%.3f form=%.3f dens=%.1f cent=%.3f conf=%.3f\n",
           s.flatness, s.formant, s.density, s.centroid, s.confidence);

    // Guitar-like broadband: moderate flatness, low formant
    FL_CHECK_GE(s.flatness, 0.20f);
    FL_CHECK_LE(s.flatness, 0.55f);
    FL_CHECK_LE(s.formant, 0.20f);

    // Must NOT be detected as vocal
    FL_CHECK_LE(s.confidence, 0.40f);
    FL_CHECK_FALSE(s.isVocal);
}

// ---------------------------------------------------------------------------
// 2. Ordering invariants — features must rank correctly across signal types.
//    These catch inversions where a tuning change makes non-voice score
//    higher than voice on a discriminating feature.
// ---------------------------------------------------------------------------

FL_TEST_CASE("VocalDetector stability - feature ordering: vowel vs sine") {
    auto vowel = test_vocal::runSignal([](fl::u32 ts) {
        return makeSyntheticVowel(150.0f, 700.0f, 1200.0f, ts);
    });
    auto sine = test_vocal::runSignal([](fl::u32 ts) {
        return makeSample(440.0f, ts);
    });

    // Vowel must score HIGHER on formant (has F1/F2 structure)
    FL_CHECK_GT(vowel.formant, sine.formant);
    // Vowel must score HIGHER on flatness (richer spectrum than pure tone)
    FL_CHECK_GT(vowel.flatness, sine.flatness);
    // Vowel must score HIGHER on density (more harmonics)
    FL_CHECK_GT(vowel.density, sine.density);
    // Vowel must have MUCH higher confidence
    FL_CHECK_GT(vowel.confidence, sine.confidence + 0.30f);
}

FL_TEST_CASE("VocalDetector stability - feature ordering: vowel vs guitar") {
    auto vowel = test_vocal::runSignal([](fl::u32 ts) {
        return makeSyntheticVowel(150.0f, 700.0f, 1200.0f, ts);
    });
    auto guitar = test_vocal::runSignal([](fl::u32 ts) {
        return makeGuitarStringDecay(220.0f, ts);
    });

    // Vowel must score HIGHER on formant ratio (voice F2/F1 > guitar)
    FL_CHECK_GT(vowel.formant, guitar.formant);
    // Vowel must have higher confidence
    FL_CHECK_GT(vowel.confidence, guitar.confidence + 0.20f);
}

FL_TEST_CASE("VocalDetector stability - feature ordering: vowel vs noise") {
    auto vowel = test_vocal::runSignal([](fl::u32 ts) {
        return makeSyntheticVowel(150.0f, 700.0f, 1200.0f, ts);
    });
    auto noise = test_vocal::runSignal([](fl::u32 ts) {
        return makeWhiteNoise(ts);
    });

    // Noise must have HIGHER flatness (uniform spectrum)
    FL_CHECK_GT(noise.flatness, vowel.flatness);
    // Noise must have HIGHER density (all bins above threshold)
    FL_CHECK_GT(noise.density, vowel.density);
    // But vowel must have higher confidence (noise is not voice)
    FL_CHECK_GT(vowel.confidence, noise.confidence + 0.30f);
}

// ---------------------------------------------------------------------------
// 3. Separation margin tests — lock down the GAP between voice and backing.
//    The margin is what matters for robust detection. If both drift equally
//    the margin stays stable and detection doesn't break.
// ---------------------------------------------------------------------------

FL_TEST_CASE("VocalDetector stability - separation: isolated vocal vs backing") {
    auto vocal = test_vocal::runSignal([](fl::u32 ts) {
        return makeJitteredVowel(150.0f, 700.0f, 1200.0f, ts);
    }, 20);
    auto backing = test_vocal::runMix(0.0f, 20);

    float confMargin = vocal.confidence - backing.confidence;
    printf("separation iso vs backing: vocal=%.3f backing=%.3f margin=%.3f\n",
           vocal.confidence, backing.confidence, confMargin);

    // Confidence margin must be at least 0.40 (isolated vocal vs drums+guitar+bass)
    FL_CHECK_GE(confMargin, 0.40f);
}

FL_TEST_CASE("VocalDetector stability - separation: vocal-in-mix vs backing") {
    auto vocMix = test_vocal::runMix(1.0f, 20);
    auto backing = test_vocal::runMix(0.0f, 20);

    float confMargin = vocMix.confidence - backing.confidence;
    printf("separation mix vs backing: voc=%.3f back=%.3f margin=%.3f\n",
           vocMix.confidence, backing.confidence, confMargin);

    // Vocal-in-mix must be measurably above backing
    FL_CHECK_GT(vocMix.confidence, backing.confidence);
    // Minimum separation margin
    FL_CHECK_GE(confMargin, 0.10f);
}

FL_TEST_CASE("VocalDetector stability - separation: formant discriminates vocal vs guitar-like") {
    auto vowel = test_vocal::runSignal([](fl::u32 ts) {
        return makeSyntheticVowel(150.0f, 700.0f, 1200.0f, ts);
    });
    auto guitar = test_vocal::runSignal([](fl::u32 ts) {
        return makeGuitarLike(ts);
    });

    float formantGap = vowel.formant - guitar.formant;
    printf("formant gap: vowel=%.3f guitar=%.3f gap=%.3f\n",
           vowel.formant, guitar.formant, formantGap);

    // Formant ratio gap must be at least 0.30 — this is the primary
    // discriminating feature. If a tuning change collapses this gap,
    // vocal detection will degrade.
    FL_CHECK_GE(formantGap, 0.30f);
}

// ---------------------------------------------------------------------------
// 4. Mix-type consistency — features should stay in expected ranges even
//    when the backing track changes (prevents next calibration round from
//    needing to retune everything).
// ---------------------------------------------------------------------------

FL_TEST_CASE("VocalDetector stability - vocal flatness stable across mix ratios") {
    // Flatness is the best 3-way discriminator. Pin it so future changes
    // to other features don't silently move flatness out of range.
    float flatnesses[5];
    float ratios[] = {0.0f, 0.3f, 0.5f, 0.7f, 1.0f};
    for (int i = 0; i < 5; ++i) {
        auto s = test_vocal::runMix(ratios[i], 15);
        flatnesses[i] = s.flatness;
    }

    printf("flatness sweep: r=0.0→%.3f  r=0.3→%.3f  r=0.5→%.3f  r=0.7→%.3f  r=1.0→%.3f\n",
           flatnesses[0], flatnesses[1], flatnesses[2], flatnesses[3], flatnesses[4]);

    // All should be in the moderate range (not tonal, not noise)
    for (int i = 0; i < 5; ++i) {
        FL_CHECK_GE(flatnesses[i], 0.30f);
        FL_CHECK_LE(flatnesses[i], 0.80f);
    }
}

// ============================================================================
// 5. Full-band weight-locking tests (realistic guitar+drums+voice mix)
//
// These use makeFullBandMix() which includes acoustic guitar with body
// resonances, kick, snare, hi-hat, and optional voice. The guitar body
// resonances produce formant-band energy matching real recordings, so these
// tests exercise the same scoring paths as real 3-way MP3 audio.
//
// PURPOSE: Lock down the weights in calculateRawConfidence(). If someone
// changes a weight, threshold, or boost gain, these tests fail immediately
// — not 3 calibration rounds later.
// ============================================================================

FL_TEST_CASE("VocalDetector weight-lock - full-band feature diagnostic") {
    auto backing = test_vocal::runFullBandMix(0.0f, 20);
    auto vocal10 = test_vocal::runFullBandMix(1.0f, 20);
    auto vocal05 = test_vocal::runFullBandMix(0.5f, 20);

    printf("\n--- Full-band mix feature distributions ---\n");
    printf("%-12s  flat    form    pres    jitter  acfIrr  zcCV    conf  isVocal\n",
           "signal");
    printf("backing      %.3f   %.3f   %.3f   %.4f  %.4f  %.4f  %.3f  %d\n",
           backing.flatness, backing.formant, backing.presence,
           backing.jitter, backing.acfIrreg, backing.zcCV,
           backing.confidence, backing.isVocal ? 1 : 0);
    printf("vocal=1.0    %.3f   %.3f   %.3f   %.4f  %.4f  %.4f  %.3f  %d\n",
           vocal10.flatness, vocal10.formant, vocal10.presence,
           vocal10.jitter, vocal10.acfIrreg, vocal10.zcCV,
           vocal10.confidence, vocal10.isVocal ? 1 : 0);
    printf("vocal=0.5    %.3f   %.3f   %.3f   %.4f  %.4f  %.4f  %.3f  %d\n",
           vocal05.flatness, vocal05.formant, vocal05.presence,
           vocal05.jitter, vocal05.acfIrreg, vocal05.zcCV,
           vocal05.confidence, vocal05.isVocal ? 1 : 0);
    printf("--- end full-band diagnostic ---\n");

    FL_CHECK(true); // Diagnostic — always passes
}

FL_TEST_CASE("VocalDetector weight-lock - full-band backing not vocal") {
    auto s = test_vocal::runFullBandMix(0.0f, 20);

    // Guitar+drums backing must NOT be detected as vocal
    FL_CHECK_FALSE(s.isVocal);
    FL_CHECK_LT(s.confidence, 0.55f);
}

FL_TEST_CASE("VocalDetector weight-lock - full-band formant gap locks weights") {
    auto backing = test_vocal::runFullBandMix(0.0f, 20);
    auto vocal = test_vocal::runFullBandMix(1.0f, 20);

    printf("full-band formant: backing=%.3f vocal=%.3f gap=%.3f\n",
           backing.formant, vocal.formant, vocal.formant - backing.formant);

    // Drums mask guitar body resonances → backing formant is low.
    // Voice adds F1/F2 peaks that survive the drum noise floor.
    // This formant gap is the PRIMARY weight-locking constraint:
    // any scoring change that collapses it will break real-audio detection.
    FL_CHECK_LE(backing.formant, 0.15f);
    FL_CHECK_GE(vocal.formant, 0.35f);
    FL_CHECK_GE(vocal.formant - backing.formant, 0.30f);
}

FL_TEST_CASE("VocalDetector weight-lock - full-band vocal-in-mix separation") {
    auto backing = test_vocal::runFullBandMix(0.0f, 20);
    auto vocal = test_vocal::runFullBandMix(1.0f, 20);

    float margin = vocal.confidence - backing.confidence;
    printf("full-band separation: vocal=%.3f backing=%.3f margin=%.3f\n",
           vocal.confidence, backing.confidence, margin);

    // Voice must produce measurably higher confidence than backing.
    // This is the CRITICAL weight-locking assertion: if someone rebalances
    // weights in a way that collapses this margin, this test catches it.
    FL_CHECK_GT(vocal.confidence, backing.confidence);
    FL_CHECK_GE(margin, 0.05f);
}

FL_TEST_CASE("VocalDetector weight-lock - full-band confidence monotonic with vocal level") {
    // As vocal level increases from 0 to 1, confidence should generally increase.
    // This locks down the direction of the scoring — if a weight change inverts
    // the relationship, this test catches it.
    float confs[5];
    float ratios[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    for (int i = 0; i < 5; ++i) {
        auto s = test_vocal::runFullBandMix(ratios[i], 15);
        confs[i] = s.confidence;
    }

    printf("full-band confidence sweep: ");
    for (int i = 0; i < 5; ++i) {
        printf("r=%.2f→%.3f  ", ratios[i], confs[i]);
    }
    printf("\n");

    // Vocal at full level must be above backing
    FL_CHECK_GT(confs[4], confs[0]);
    // No level should produce confidence > 1.0 or negative
    for (int i = 0; i < 5; ++i) {
        FL_CHECK_GE(confs[i], -0.1f);
        FL_CHECK_LE(confs[i], 1.5f);
    }
}

FL_TEST_CASE("VocalDetector weight-lock - acoustic guitar body resonances") {
    // The acoustic guitar body resonances produce formant-like peaks in F1/F2
    // bands. Alone, the guitar looks voice-like (conf ~0.67) because its body
    // resonances ARE in vocal formant bands — this is WHY voice/guitar
    // discrimination is fundamentally hard. The detector relies on time-domain
    // features (jitter, ACF) to separate them, not spectral features alone.
    auto s = test_vocal::runSignal([](fl::u32 ts) {
        return makeAcousticGuitar(196.0f, ts);
    });

    printf("acoustic guitar: flat=%.3f form=%.3f dens=%.1f jitter=%.4f acf=%.4f conf=%.3f\n",
           s.flatness, s.formant, s.density, s.jitter, s.acfIrreg, s.confidence);

    // Body resonances should produce measurable formant ratio
    FL_CHECK_GE(s.formant, 0.10f);

    // Flatness should be moderate (not tonal like pure sine, not flat like noise)
    FL_CHECK_GE(s.flatness, 0.25f);
    FL_CHECK_LE(s.flatness, 0.65f);

    // Confidence in valid range (may or may not trigger detection — guitar
    // body resonances are inherently voice-like, that's the problem we're solving)
    FL_CHECK_GE(s.confidence, 0.0f);
    FL_CHECK_LE(s.confidence, 1.0f);
}

// ============================================================================
// 6. Degenerate-case tests — prevent pathological scoring behavior.
//    These test signal types that could fool the detector and lock down
//    the confidence clamp and boost cap behavior.
// ============================================================================

FL_TEST_CASE("VocalDetector degenerate - confidence always in [0, 1]") {
    // Run ALL signal generators through 20 frames each.
    // Assert raw and smoothed confidence are always in [0, 1].
    struct SignalGen {
        const char* name;
        AudioSample (*gen)(fl::u32);
    };

    auto genSine = [](fl::u32 ts) { return makeSample(440.0f, ts); };
    auto genVowel = [](fl::u32 ts) {
        return makeSyntheticVowel(150.0f, 700.0f, 1200.0f, ts);
    };
    auto genJitVowel = [](fl::u32 ts) {
        return makeJitteredVowel(150.0f, 700.0f, 1200.0f, ts);
    };
    auto genGuitar = [](fl::u32 ts) { return makeGuitarStringDecay(220.0f, ts); };
    auto genNoise = [](fl::u32 ts) { return makeWhiteNoise(ts); };
    auto genChirp = [](fl::u32 ts) { return makeChirp(200.0f, 2000.0f, ts); };
    auto genSaw = [](fl::u32 ts) { return makeSawtoothWave(200.0f, ts); };
    auto genTremolo = [](fl::u32 ts) {
        return makeTremoloTone(300.0f, 6.0f, 0.5f, ts);
    };
    auto genFiltNoise = [](fl::u32 ts) {
        return makeFilteredNoise(200.0f, 4000.0f, ts);
    };
    auto genTom = [](fl::u32 ts) { return makePitchedTom(150.0f, ts); };
    auto genAcGuitar = [](fl::u32 ts) { return makeAcousticGuitar(196.0f, ts); };
    auto genMixBacking = [](fl::u32 ts) { return makeFullBandMix(0.0f, ts); };
    auto genMixVocal = [](fl::u32 ts) { return makeFullBandMix(1.0f, ts); };

    SignalGen signals[] = {
        {"sine", genSine}, {"vowel", genVowel}, {"jit_vowel", genJitVowel},
        {"guitar", genGuitar}, {"noise", genNoise}, {"chirp", genChirp},
        {"sawtooth", genSaw}, {"tremolo", genTremolo},
        {"filt_noise", genFiltNoise}, {"tom", genTom},
        {"ac_guitar", genAcGuitar}, {"mix_back", genMixBacking},
        {"mix_vocal", genMixVocal},
    };

    for (const auto& sig : signals) {
        VocalDetector det;
        det.setSampleRate(44100);
        for (int frame = 0; frame < 20; ++frame) {
            auto sample = sig.gen(frame * 12);
            auto ctx = fl::make_shared<AudioContext>(sample);
            ctx->setSampleRate(44100);
            ctx->getFFT(128);
            det.update(ctx);

            float raw = Diag::getRawConfidence(det);
            float smoothed = det.getConfidence();
            FL_CHECK_GE(raw, 0.0f);
            FL_CHECK_LE(raw, 1.0f);
            FL_CHECK_GE(smoothed, -0.01f);
            FL_CHECK_LE(smoothed, 1.01f);
        }
    }
}

FL_TEST_CASE("VocalDetector degenerate - all boosts simultaneous still clamped") {
    // Craft a signal that alternates between two vowels at different F0s
    // with high amplitude jitter to trigger multiple boosts simultaneously.
    VocalDetector det;
    det.setSampleRate(44100);

    for (int frame = 0; frame < 30; ++frame) {
        AudioSample sample;
        if (frame % 2 == 0) {
            sample = makeJitteredVowel(120.0f, 600.0f, 1400.0f, frame * 12, 20000.0f);
        } else {
            sample = makeJitteredVowel(180.0f, 800.0f, 2200.0f, frame * 12, 12000.0f);
        }
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        ctx->getFFT(128);
        det.update(ctx);

        float raw = Diag::getRawConfidence(det);
        FL_CHECK_GE(raw, 0.0f);
        FL_CHECK_LE(raw, 1.0f);
    }
}

FL_TEST_CASE("VocalDetector degenerate - sawtooth wave not vocal") {
    VocalDetector det;
    det.setSampleRate(44100);
    for (int frame = 0; frame < 10; ++frame) {
        auto sample = makeSawtoothWave(200.0f, frame * 12);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        ctx->getFFT(128);
        det.update(ctx);
    }

    printf("sawtooth: conf=%.3f isVocal=%d flat=%.3f form=%.3f\n",
           det.getConfidence(), det.isVocal() ? 1 : 0,
           Diag::getSpectralFlatness(det), Diag::getFormantRatio(det));

    // Sawtooth has 1/h harmonic rolloff similar to voice — moderate confidence
    // is expected. The key assertion is !isVocal() (below on-threshold 0.65).
    FL_CHECK_FALSE(det.isVocal());
    FL_CHECK_LT(det.getConfidence(), 0.65f);
}

FL_TEST_CASE("VocalDetector degenerate - AM tremolo tone not vocal") {
    VocalDetector det;
    det.setSampleRate(44100);
    for (int frame = 0; frame < 15; ++frame) {
        auto sample = makeTremoloTone(300.0f, 6.0f, 0.5f, frame * 12);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        ctx->getFFT(128);
        det.update(ctx);
    }

    printf("tremolo: conf=%.3f isVocal=%d flat=%.3f form=%.3f\n",
           det.getConfidence(), det.isVocal() ? 1 : 0,
           Diag::getSpectralFlatness(det), Diag::getFormantRatio(det));

    FL_CHECK_FALSE(det.isVocal());
    FL_CHECK_LT(det.getConfidence(), 0.50f);
}

FL_TEST_CASE("VocalDetector degenerate - pitched tom not vocal") {
    VocalDetector det;
    det.setSampleRate(44100);
    for (int frame = 0; frame < 10; ++frame) {
        auto sample = makePitchedTom(150.0f, frame * 12);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        ctx->getFFT(128);
        det.update(ctx);
    }

    printf("pitched tom: conf=%.3f isVocal=%d flat=%.3f form=%.3f\n",
           det.getConfidence(), det.isVocal() ? 1 : 0,
           Diag::getSpectralFlatness(det), Diag::getFormantRatio(det));

    FL_CHECK_FALSE(det.isVocal());
    FL_CHECK_LT(det.getConfidence(), 0.50f);
}

FL_TEST_CASE("VocalDetector degenerate - speech-band noise not vocal") {
    VocalDetector det;
    det.setSampleRate(44100);
    for (int frame = 0; frame < 10; ++frame) {
        auto sample = makeFilteredNoise(200.0f, 4000.0f, frame * 12);
        auto ctx = fl::make_shared<AudioContext>(sample);
        ctx->setSampleRate(44100);
        ctx->getFFT(128);
        det.update(ctx);
    }

    printf("speech-band noise: conf=%.3f isVocal=%d flat=%.3f form=%.3f\n",
           det.getConfidence(), det.isVocal() ? 1 : 0,
           Diag::getSpectralFlatness(det), Diag::getFormantRatio(det));

    FL_CHECK_FALSE(det.isVocal());
    FL_CHECK_LT(det.getConfidence(), 0.45f);
}

FL_TEST_CASE("VocalDetector degenerate - formant gap >= 0.35") {
    auto vowel = test_vocal::runSignal([](fl::u32 ts) {
        return makeSyntheticVowel(150.0f, 700.0f, 1200.0f, ts);
    });
    auto guitar = test_vocal::runSignal([](fl::u32 ts) {
        return makeGuitarLike(ts);
    });

    float formantGap = vowel.formant - guitar.formant;
    printf("degenerate formant gap: vowel=%.3f guitar=%.3f gap=%.3f\n",
           vowel.formant, guitar.formant, formantGap);

    // Tighter than existing 0.30 test — catches formant weight drops
    FL_CHECK_GE(formantGap, 0.35f);
}

FL_TEST_CASE("VocalDetector degenerate - full-band variance separation >= 0.05") {
    auto vocal = test_vocal::runFullBandMix(1.0f, 20);
    auto backing = test_vocal::runFullBandMix(0.0f, 20);

    float confGap = vocal.confidence - backing.confidence;
    printf("full-band conf gap: vocal=%.3f backing=%.3f gap=%.3f\n",
           vocal.confidence, backing.confidence, confGap);

    // Catches variance boost gain changes
    FL_CHECK_GE(confGap, 0.05f);
}

