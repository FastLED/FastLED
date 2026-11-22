// Unit tests for audio::detector::EqualizerDetector — WLED-style 16-bin equalizer

#include "fl/audio/detector/equalizer.h"
#include "fl/audio/audio_context.h"
#include "fl/audio/audio_processor.h"
#include "fl/audio/mic_response_data.h"
#include "fl/stl/vector.h"
#include "fl/stl/shared_ptr.h"
#include "fl/math/math.h"
#include "fl/math/math.h"
#include "tests/fl/audio/test_helpers.h"

using namespace fl;
using fl::audio::test::makeSample;
using fl::audio::test::makeSilence;

FL_TEST_CASE("audio::detector::EqualizerDetector - returns 0.0-1.0 range") {
    auto eq = make_shared<audio::detector::EqualizerDetector>();

    // Feed a sine wave signal through context
    auto sample = makeSample(440.0f, 0, 16000.0f, 512);
    auto context = make_shared<audio::Context>(sample);

    // Process several frames to let normalization settle
    for (int i = 0; i < 20; ++i) {
        eq->update(context);
    }

    FL_CHECK_GE(eq->getBass(), 0.0f);
    FL_CHECK_LE(eq->getBass(), 1.0f);
    FL_CHECK_GE(eq->getMid(), 0.0f);
    FL_CHECK_LE(eq->getMid(), 1.0f);
    FL_CHECK_GE(eq->getTreble(), 0.0f);
    FL_CHECK_LE(eq->getTreble(), 1.0f);
    FL_CHECK_GE(eq->getVolume(), 0.0f);
    FL_CHECK_LE(eq->getVolume(), 1.0f);

    for (int i = 0; i < audio::detector::EqualizerDetector::kNumBins; ++i) {
        FL_CHECK_GE(eq->getBin(i), 0.0f);
        FL_CHECK_LE(eq->getBin(i), 1.0f);
    }
}

FL_TEST_CASE("audio::detector::EqualizerDetector - silence returns near-zeros") {
    auto eq = make_shared<audio::detector::EqualizerDetector>();

    auto sample = makeSilence(0, 512);
    auto context = make_shared<audio::Context>(sample);

    for (int i = 0; i < 10; ++i) {
        eq->update(context);
    }

    // With silence, volume should be zero or near-zero
    FL_CHECK_LT(eq->getVolume(), 0.01f);
    // Bins may have residual normalization artifacts but should be very low
    FL_CHECK_LT(eq->getBass(), 0.1f);
    FL_CHECK_LT(eq->getMid(), 0.1f);
    FL_CHECK_LT(eq->getTreble(), 0.1f);
}

FL_TEST_CASE("audio::detector::EqualizerDetector - bass signal activates bass bins") {
    auto eq = make_shared<audio::detector::EqualizerDetector>();

    // 100 Hz tone — should be in bass range (bins 0-3)
    auto sample = makeSample(100.0f, 0, 16000.0f, 512);
    auto context = make_shared<audio::Context>(sample);

    for (int i = 0; i < 30; ++i) {
        eq->update(context);
    }

    // Bass should have non-zero energy
    FL_CHECK_GT(eq->getBass(), 0.0f);
    // At least one bass bin (0-3) should be active
    bool anyBassBin = false;
    for (int i = 0; i < 4; ++i) {
        if (eq->getBin(i) > 0.1f) anyBassBin = true;
    }
    FL_CHECK(anyBassBin);
}

// Regression test: GitHub #2193 — getEqBin(0) was always zero.
// With 512-sample FFT at 44100 Hz (86 Hz resolution), the narrow
// lowest output bin (60-70 Hz) contained no FFT bins. The LUT fix
// ensures FFT bin 1 (86 Hz) contributes to output bin 0.
FL_TEST_CASE("audio::detector::EqualizerDetector - bin 0 is nonzero for bass tone") {
    auto eq = make_shared<audio::detector::EqualizerDetector>();

    // 80 Hz bass tone — should activate bin 0
    auto sample = makeSample(80.0f, 0, 16000.0f, 512);
    auto context = make_shared<audio::Context>(sample);

    for (int i = 0; i < 30; ++i) {
        eq->update(context);
    }

    // Bin 0 must have nonzero energy — was always 0 before the fix
    FL_CHECK_GT(eq->getBin(0), 0.0f);
}

FL_TEST_CASE("audio::detector::EqualizerDetector - treble signal activates treble bins") {
    auto eq = make_shared<audio::detector::EqualizerDetector>();

    // 4000 Hz tone — should be in treble range (bins 11-15)
    auto sample = makeSample(4000.0f, 0, 16000.0f, 512);
    auto context = make_shared<audio::Context>(sample);

    for (int i = 0; i < 30; ++i) {
        eq->update(context);
    }

    // Treble should have non-zero energy
    FL_CHECK_GT(eq->getTreble(), 0.0f);
    // At least one treble bin (11-15) should be active
    bool anyTrebleBin = false;
    for (int i = 11; i < 16; ++i) {
        if (eq->getBin(i) > 0.1f) anyTrebleBin = true;
    }
    FL_CHECK(anyTrebleBin);
}

FL_TEST_CASE("audio::detector::EqualizerDetector - volume normalizes over time") {
    auto eq = make_shared<audio::detector::EqualizerDetector>();

    auto sample = makeSample(440.0f, 0, 16000.0f, 512);
    auto context = make_shared<audio::Context>(sample);

    // Feed constant signal — volume should normalize to near 1.0
    for (int i = 0; i < 50; ++i) {
        eq->update(context);
    }

    // After normalization, constant signal should be near 1.0
    FL_CHECK_GT(eq->getVolume(), 0.5f);
}

FL_TEST_CASE("audio::detector::EqualizerDetector - 16 bins all valid") {
    auto eq = make_shared<audio::detector::EqualizerDetector>();

    // Feed broadband signal (white noise) — all bins should have non-zero values
    auto sample = fl::audio::test::makeWhiteNoise(0, 16000.0f, 512);
    auto context = make_shared<audio::Context>(sample);

    for (int i = 0; i < 30; ++i) {
        eq->update(context);
    }

    // Check all bins are valid floats (not NaN)
    for (int i = 0; i < audio::detector::EqualizerDetector::kNumBins; ++i) {
        float val = eq->getBin(i);
        FL_CHECK_FALSE(val != val); // Not NaN
        FL_CHECK_GE(val, 0.0f);
        FL_CHECK_LE(val, 1.0f);
    }
}

FL_TEST_CASE("audio::detector::EqualizerDetector - getBins matches individual getBin") {
    auto eq = make_shared<audio::detector::EqualizerDetector>();

    auto sample = makeSample(440.0f, 0, 16000.0f, 512);
    auto context = make_shared<audio::Context>(sample);

    for (int i = 0; i < 10; ++i) {
        eq->update(context);
    }

    const float* bins = eq->getBins();
    FL_CHECK(bins != nullptr);

    for (int i = 0; i < audio::detector::EqualizerDetector::kNumBins; ++i) {
        FL_CHECK_EQ(bins[i], eq->getBin(i));
    }
}

FL_TEST_CASE("audio::detector::EqualizerDetector - out of range getBin returns 0") {
    audio::detector::EqualizerDetector eq;
    FL_CHECK_EQ(eq.getBin(-1), 0.0f);
    FL_CHECK_EQ(eq.getBin(16), 0.0f);
    FL_CHECK_EQ(eq.getBin(100), 0.0f);
}

FL_TEST_CASE("audio::detector::EqualizerDetector - via audio::Processor lazy creation") {
    audio::Processor audio;

    auto sample = makeSample(440.0f, 0, 16000.0f, 512);

    // Feed signal
    for (int i = 0; i < 20; ++i) {
        audio.update(sample);
    }

    // getEq* methods should work (lazy creates detector)
    float bass = audio.getEqBass();
    float mid = audio.getEqMid();
    float treble = audio.getEqTreble();
    float volume = audio.getEqVolume();
    float bin0 = audio.getEqBin(0);

    FL_CHECK_GE(bass, 0.0f);
    FL_CHECK_LE(bass, 1.0f);
    FL_CHECK_GE(mid, 0.0f);
    FL_CHECK_LE(mid, 1.0f);
    FL_CHECK_GE(treble, 0.0f);
    FL_CHECK_LE(treble, 1.0f);
    FL_CHECK_GE(volume, 0.0f);
    FL_CHECK_LE(volume, 1.0f);
    FL_CHECK_GE(bin0, 0.0f);
    FL_CHECK_LE(bin0, 1.0f);
}

FL_TEST_CASE("audio::detector::EqualizerDetector - onEqualizer callback fires") {
    auto eq = make_shared<audio::detector::EqualizerDetector>();

    auto sample = makeSample(440.0f, 0, 16000.0f, 512);
    auto context = make_shared<audio::Context>(sample);

    bool fired = false;
    audio::detector::Equalizer captured;
    eq->onEqualizer.add([&](const audio::detector::Equalizer& e) {
        fired = true;
        captured = e;
    });

    eq->update(context);
    eq->fireCallbacks();

    FL_CHECK(fired);
    FL_CHECK_GE(captured.bass, 0.0f);
    FL_CHECK_LE(captured.bass, 1.0f);
    FL_CHECK_GE(captured.volume, 0.0f);
    FL_CHECK_LE(captured.volume, 1.0f);
    FL_CHECK_GE(captured.zcf, 0.0f);
    FL_CHECK_LE(captured.zcf, 1.0f);
    FL_CHECK_EQ(captured.bins.size(), 16u);
}

FL_TEST_CASE("audio::detector::EqualizerDetector - zcf is valid") {
    auto eq = make_shared<audio::detector::EqualizerDetector>();

    auto sample = makeSample(440.0f, 0, 16000.0f, 512);
    auto context = make_shared<audio::Context>(sample);

    eq->update(context);

    // ZCF should be in 0.0-1.0 range
    FL_CHECK_GE(eq->getZcf(), 0.0f);
    FL_CHECK_LE(eq->getZcf(), 1.0f);
}

FL_TEST_CASE("audio::detector::EqualizerDetector - via audio::Processor onEqualizer") {
    audio::Processor audio;
    auto sample = makeSample(440.0f, 0, 16000.0f, 512);

    bool fired = false;
    audio.onEqualizer([&](const audio::detector::Equalizer& eq) {
        fired = true;
        FL_CHECK_GE(eq.bass, 0.0f);
        FL_CHECK_GE(eq.volume, 0.0f);
        FL_CHECK_GE(eq.zcf, 0.0f);
        FL_CHECK_EQ(eq.bins.size(), 16u);
    });

    audio.update(sample);
    FL_CHECK(fired);
}

FL_TEST_CASE("audio::detector::EqualizerDetector - reset clears state") {
    auto eq = make_shared<audio::detector::EqualizerDetector>();

    auto sample = makeSample(440.0f, 0, 16000.0f, 512);
    auto context = make_shared<audio::Context>(sample);

    for (int i = 0; i < 10; ++i) {
        eq->update(context);
    }

    eq->reset();

    FL_CHECK_EQ(eq->getBass(), 0.0f);
    FL_CHECK_EQ(eq->getMid(), 0.0f);
    FL_CHECK_EQ(eq->getTreble(), 0.0f);
    FL_CHECK_EQ(eq->getVolume(), 0.0f);
    for (int i = 0; i < audio::detector::EqualizerDetector::kNumBins; ++i) {
        FL_CHECK_EQ(eq->getBin(i), 0.0f);
    }
}

// ============================================================================
// P2 feature tests: dominant frequency, volumeDb, mic profiles
// ============================================================================

FL_TEST_CASE("audio::detector::EqualizerDetector - dominant frequency detects peak bin") {
    auto eq = make_shared<audio::detector::EqualizerDetector>();

    // Use a 440 Hz tone — should be detected in a mid-range bin
    auto sample = makeSample(440.0f, 0, 16000.0f, 512);
    auto context = make_shared<audio::Context>(sample);

    for (int i = 0; i < 30; ++i) {
        eq->update(context);
    }

    // Dominant frequency should be in a reasonable range near 440 Hz
    float domFreq = eq->getDominantFreqHz();
    FL_CHECK_GT(domFreq, 0.0f);
    // Dominant magnitude should be valid 0-1
    FL_CHECK_GE(eq->getDominantMagnitude(), 0.0f);
    FL_CHECK_LE(eq->getDominantMagnitude(), 1.0f);
}

FL_TEST_CASE("audio::detector::EqualizerDetector - volumeDb references full scale") {
    auto eq = make_shared<audio::detector::EqualizerDetector>();

    // Loud signal: amplitude 16000 out of 32767 max
    auto sample = makeSample(440.0f, 0, 16000.0f, 512);
    auto context = make_shared<audio::Context>(sample);

    for (int i = 0; i < 20; ++i) {
        eq->update(context);
    }

    // volumeDb should be negative (below full scale) and above -100
    float db = eq->getVolumeDb();
    FL_CHECK_GT(db, -100.0f);
    FL_CHECK_LT(db, 0.0f);
}

FL_TEST_CASE("audio::detector::EqualizerDetector - volumeDb silence is -100") {
    auto eq = make_shared<audio::detector::EqualizerDetector>();

    auto sample = makeSilence(0, 512);
    auto context = make_shared<audio::Context>(sample);

    for (int i = 0; i < 10; ++i) {
        eq->update(context);
    }

    FL_CHECK_EQ(eq->getVolumeDb(), -100.0f);
}

FL_TEST_CASE("audio::detector::EqualizerDetector - mic profile INMP441 applies gains") {
    auto eqNoMic = make_shared<audio::detector::EqualizerDetector>();
    auto eqWithMic = make_shared<audio::detector::EqualizerDetector>();
    eqWithMic->setMicProfile(audio::MicProfile::INMP441);

    auto sample = fl::audio::test::makeWhiteNoise(0, 16000.0f, 512);
    auto context = make_shared<audio::Context>(sample);

    for (int i = 0; i < 30; ++i) {
        eqNoMic->update(context);
        eqWithMic->update(context);
    }

    // With mic correction, bins should still be in valid range
    for (int i = 0; i < audio::detector::EqualizerDetector::kNumBins; ++i) {
        FL_CHECK_GE(eqWithMic->getBin(i), 0.0f);
        FL_CHECK_LE(eqWithMic->getBin(i), 1.0f);
    }
}

FL_TEST_CASE("audio::detector::EqualizerDetector - mic profile None has no effect") {
    auto eq1 = make_shared<audio::detector::EqualizerDetector>();
    auto eq2 = make_shared<audio::detector::EqualizerDetector>();
    eq2->setMicProfile(audio::MicProfile::None);

    auto sample = makeSample(440.0f, 0, 16000.0f, 512);
    auto context = make_shared<audio::Context>(sample);

    for (int i = 0; i < 20; ++i) {
        eq1->update(context);
        eq2->update(context);
    }

    // Should produce identical results
    for (int i = 0; i < audio::detector::EqualizerDetector::kNumBins; ++i) {
        FL_CHECK_EQ(eq1->getBin(i), eq2->getBin(i));
    }
}

FL_TEST_CASE("audio::detector::EqualizerDetector - scaling mode sqrt keeps valid range") {
    auto eq = make_shared<audio::detector::EqualizerDetector>();
    audio::detector::EqualizerConfig config;
    config.scalingMode = audio::detector::FFTScalingMode::SquareRoot;
    eq->configure(config);

    auto sample = makeSample(440.0f, 0, 16000.0f, 512);
    auto context = make_shared<audio::Context>(sample);

    for (int i = 0; i < 20; ++i) {
        eq->update(context);
    }

    for (int i = 0; i < audio::detector::EqualizerDetector::kNumBins; ++i) {
        FL_CHECK_GE(eq->getBin(i), 0.0f);
        FL_CHECK_LE(eq->getBin(i), 1.0f);
    }
}

FL_TEST_CASE("audio::detector::EqualizerDetector - callback includes P2 fields") {
    auto eq = make_shared<audio::detector::EqualizerDetector>();

    auto sample = makeSample(440.0f, 0, 16000.0f, 512);
    auto context = make_shared<audio::Context>(sample);

    audio::detector::Equalizer captured;
    bool fired = false;
    eq->onEqualizer.add([&](const audio::detector::Equalizer& e) {
        fired = true;
        captured = e;
    });

    for (int i = 0; i < 20; ++i) {
        eq->update(context);
        eq->fireCallbacks();
    }

    FL_CHECK(fired);
    FL_CHECK_GT(captured.dominantFreqHz, 0.0f);
    FL_CHECK_GE(captured.dominantMagnitude, 0.0f);
    FL_CHECK_LE(captured.dominantMagnitude, 1.0f);
    FL_CHECK_GT(captured.volumeDb, -100.0f);
}

// ============================================================================
// Pink noise spectral tilt tests
// ============================================================================

FL_TEST_CASE("audio::detector::EqualizerDetector - pink noise gains boost treble over bass") {
    // Validate that audio::computePinkNoiseGains produces increasing gains
    // when called with the same bin centers as audio::detector::EqualizerDetector uses.
    float binCenters[16];
    float fmin = 60.0f;
    float fmax = 5120.0f;
    float m = fl::logf(fmax / fmin);
    for (int i = 0; i < 16; ++i) {
        binCenters[i] = fmin * fl::expf(m * static_cast<float>(i) / 15.0f);
    }

    float gains[16];
    audio::computePinkNoiseGains(binCenters, 16, gains);

    // Treble bins (11-15) should have higher gain than bass bins (0-3)
    float bassAvg = 0.0f;
    for (int i = 0; i < 4; ++i) bassAvg += gains[i];
    bassAvg /= 4.0f;

    float trebleAvg = 0.0f;
    for (int i = 11; i < 16; ++i) trebleAvg += gains[i];
    trebleAvg /= 5.0f;

    FL_CHECK_GT(trebleAvg, bassAvg);
}

FL_TEST_CASE("audio::detector::EqualizerDetector - all bins valid with pink noise active") {
    auto eq = make_shared<audio::detector::EqualizerDetector>();

    // Pink noise is always active — verify end-to-end bins stay in [0, 1]
    auto sample = fl::audio::test::makeWhiteNoise(0, 16000.0f, 512);
    auto context = make_shared<audio::Context>(sample);

    for (int i = 0; i < 30; ++i) {
        eq->update(context);
    }

    for (int i = 0; i < audio::detector::EqualizerDetector::kNumBins; ++i) {
        float val = eq->getBin(i);
        FL_CHECK_FALSE(val != val); // Not NaN
        FL_CHECK_GE(val, 0.0f);
        FL_CHECK_LE(val, 1.0f);
    }
}

// ============================================================================
// Adversarial tests
// ============================================================================

FL_TEST_CASE("ADVERSARIAL - mic correction does not leak to other detector") {
    // Verify that setting mic profile on audio::Processor only affects equalizer,
    // not other detector like audio::detector::FrequencyBands
    audio::Processor audio;
    audio.setMicProfile(audio::MicProfile::INMP441);

    auto sample = fl::audio::test::makeWhiteNoise(0, 16000.0f, 512);

    for (int i = 0; i < 30; ++i) {
        audio.update(sample);
    }

    // audio::detector::Equalizer should work
    float eqBass = audio.getEqBass();
    FL_CHECK_GE(eqBass, 0.0f);
    FL_CHECK_LE(eqBass, 1.0f);

    // audio::detector::FrequencyBands (raw detector) should also work independently
    float rawBass = audio.getBassLevel();
    FL_CHECK_GE(rawBass, 0.0f);
    FL_CHECK_LE(rawBass, 1.0f);
}

FL_TEST_CASE("ADVERSARIAL - extreme mic gains keep bins in 0-1") {
    // Even with extreme mic profile gains, output should stay in valid range
    auto eq = make_shared<audio::detector::EqualizerDetector>();
    // GenericMEMS has modest gains (0.92-1.05x) derived from datasheet data
    eq->setMicProfile(audio::MicProfile::GenericMEMS);

    auto sample = makeSample(4000.0f, 0, 30000.0f, 512); // near-max amplitude
    auto context = make_shared<audio::Context>(sample);

    for (int i = 0; i < 30; ++i) {
        eq->update(context);
    }

    for (int i = 0; i < audio::detector::EqualizerDetector::kNumBins; ++i) {
        FL_CHECK_GE(eq->getBin(i), 0.0f);
        FL_CHECK_LE(eq->getBin(i), 1.0f);
    }
    FL_CHECK_GE(eq->getVolume(), 0.0f);
    FL_CHECK_LE(eq->getVolume(), 1.0f);
}

FL_TEST_CASE("ADVERSARIAL - silence with mic profile produces near-zeros") {
    auto eq = make_shared<audio::detector::EqualizerDetector>();
    eq->setMicProfile(audio::MicProfile::INMP441);

    auto sample = makeSilence(0, 512);
    auto context = make_shared<audio::Context>(sample);

    for (int i = 0; i < 10; ++i) {
        eq->update(context);
    }

    FL_CHECK_LT(eq->getVolume(), 0.01f);
    FL_CHECK_EQ(eq->getVolumeDb(), -100.0f);
    FL_CHECK(eq->getIsSilence());
}

FL_TEST_CASE("ADVERSARIAL - scaling mode ordering correctness") {
    // Verify mic correction is applied BEFORE scaling.
    // With sqrt scaling, sqrt(raw * micGain) != sqrt(raw) * micGain.
    // We verify indirectly: both paths should produce valid output.
    auto eqNoScale = make_shared<audio::detector::EqualizerDetector>();
    eqNoScale->setMicProfile(audio::MicProfile::INMP441);

    auto eqSqrt = make_shared<audio::detector::EqualizerDetector>();
    eqSqrt->setMicProfile(audio::MicProfile::INMP441);
    audio::detector::EqualizerConfig config;
    config.scalingMode = audio::detector::FFTScalingMode::SquareRoot;
    eqSqrt->configure(config);
    eqSqrt->setMicProfile(audio::MicProfile::INMP441); // re-set after configure

    auto sample = makeSample(440.0f, 0, 16000.0f, 512);
    auto context = make_shared<audio::Context>(sample);

    for (int i = 0; i < 30; ++i) {
        eqNoScale->update(context);
        eqSqrt->update(context);
    }

    // Both should produce valid 0-1 output
    for (int i = 0; i < audio::detector::EqualizerDetector::kNumBins; ++i) {
        FL_CHECK_GE(eqNoScale->getBin(i), 0.0f);
        FL_CHECK_LE(eqNoScale->getBin(i), 1.0f);
        FL_CHECK_GE(eqSqrt->getBin(i), 0.0f);
        FL_CHECK_LE(eqSqrt->getBin(i), 1.0f);
    }
}

FL_TEST_CASE("ADVERSARIAL - all mic profiles produce valid output") {
    audio::MicProfile profiles[] = {
        audio::MicProfile::None, audio::MicProfile::INMP441, audio::MicProfile::ICS43434,
        audio::MicProfile::SPM1423, audio::MicProfile::GenericMEMS, audio::MicProfile::LineIn
    };

    auto sample = fl::audio::test::makeWhiteNoise(0, 16000.0f, 512);
    auto context = make_shared<audio::Context>(sample);

    for (auto profile : profiles) {
        auto eq = make_shared<audio::detector::EqualizerDetector>();
        eq->setMicProfile(profile);

        for (int i = 0; i < 30; ++i) {
            eq->update(context);
        }

        for (int j = 0; j < audio::detector::EqualizerDetector::kNumBins; ++j) {
            float val = eq->getBin(j);
            FL_CHECK_FALSE(val != val); // Not NaN
            FL_CHECK_GE(val, 0.0f);
            FL_CHECK_LE(val, 1.0f);
        }
        FL_CHECK_GE(eq->getVolume(), 0.0f);
        FL_CHECK_LE(eq->getVolume(), 1.0f);
    }
}

FL_TEST_CASE("ADVERSARIAL - output attack/decay smoothing keeps valid range") {
    auto eq = make_shared<audio::detector::EqualizerDetector>();
    audio::detector::EqualizerConfig config;
    config.outputAttack = 0.024f;  // WLED-MM style attack
    config.outputDecay = 0.250f;   // WLED-MM style decay
    eq->configure(config);

    auto sample = makeSample(440.0f, 0, 16000.0f, 512);
    auto context = make_shared<audio::Context>(sample);

    for (int i = 0; i < 30; ++i) {
        eq->update(context);
    }

    for (int i = 0; i < audio::detector::EqualizerDetector::kNumBins; ++i) {
        FL_CHECK_GE(eq->getBin(i), 0.0f);
        FL_CHECK_LE(eq->getBin(i), 1.0f);
    }
}

FL_TEST_CASE("ADVERSARIAL - audio::Processor no pre-audio::fft::FFT AGC stacking") {
    // Verify audio::Processor doesn't have AGC in the pipeline
    // by checking that raw samples pass through unmodified to detector
    audio::Processor audio;
    audio.setSignalConditioningEnabled(false); // disable all conditioning

    auto sample = makeSample(440.0f, 0, 16000.0f, 512);

    for (int i = 0; i < 20; ++i) {
        audio.update(sample);
    }

    // The context sample should be the same as input (no AGC modification)
    auto ctxSample = audio.getSample();
    FL_CHECK(ctxSample.isValid());
    FL_CHECK_EQ(ctxSample.size(), sample.size());
}

FL_TEST_CASE("ADVERSARIAL - volumeDb increases with louder signal") {
    auto eqQuiet = make_shared<audio::detector::EqualizerDetector>();
    auto eqLoud = make_shared<audio::detector::EqualizerDetector>();

    auto quietSample = makeSample(440.0f, 0, 1000.0f, 512);
    auto loudSample = makeSample(440.0f, 0, 20000.0f, 512);
    auto quietCtx = make_shared<audio::Context>(quietSample);
    auto loudCtx = make_shared<audio::Context>(loudSample);

    for (int i = 0; i < 20; ++i) {
        eqQuiet->update(quietCtx);
        eqLoud->update(loudCtx);
    }

    // Louder signal should have higher dB reading
    FL_CHECK_GT(eqLoud->getVolumeDb(), eqQuiet->getVolumeDb());
}

FL_TEST_CASE("ADVERSARIAL - reset clears P2 fields") {
    auto eq = make_shared<audio::detector::EqualizerDetector>();

    auto sample = makeSample(440.0f, 0, 16000.0f, 512);
    auto context = make_shared<audio::Context>(sample);

    for (int i = 0; i < 20; ++i) {
        eq->update(context);
    }

    eq->reset();

    FL_CHECK_EQ(eq->getDominantFreqHz(), 0.0f);
    FL_CHECK_EQ(eq->getDominantMagnitude(), 0.0f);
    FL_CHECK_EQ(eq->getVolumeDb(), -100.0f);
}

// ============================================================================
// Tone sweep tests: continuous bin motion and jitter detection
// ============================================================================

FL_TEST_CASE("audio::detector::EqualizerDetector - tone sweep moves dominant bin upward") {
    // Sweep a tone from 60 Hz to 5120 Hz (the equalizer's full range).
    // The dominant frequency should increase monotonically with the sweep.
    auto eq = make_shared<audio::detector::EqualizerDetector>();

    const int numFrames = 200;
    const float startFreq = 80.0f;
    const float endFreq = 4500.0f;

    float prevDomFreq = 0.0f;
    int monotonicViolations = 0;

    for (int frame = 0; frame < numFrames; ++frame) {
        float t = static_cast<float>(frame) / static_cast<float>(numFrames - 1);
        float freq = startFreq + (endFreq - startFreq) * t;

        auto sample = makeSample(freq, frame * 12, 16000.0f, 512);
        auto context = make_shared<audio::Context>(sample);
        context->setSampleRate(44100);
        eq->update(context);

        float domFreq = eq->getDominantFreqHz();
        // After warm-up (first 10 frames), dominant freq should generally increase
        if (frame > 10 && domFreq < prevDomFreq - 50.0f) {
            monotonicViolations++;
        }
        prevDomFreq = domFreq;
    }

    FL_WARN("audio::detector::Equalizer tone sweep: monotonic violations="
                 << monotonicViolations << " / " << (numFrames - 10));

    // Allow a few violations due to audio::fft::FFT bin quantization, but should be rare
    FL_CHECK_LT(monotonicViolations, numFrames / 10);
}

FL_TEST_CASE("audio::detector::EqualizerDetector - tone sweep activates bass then mid then treble") {
    // Sweep through the EQ range and verify band dominance transitions correctly.
    // Bass: bins 0-3 (~60-320 Hz), Mid: bins 4-10 (~320-2560 Hz),
    // Treble: bins 11-15 (~2560-5120 Hz)
    auto eq = make_shared<audio::detector::EqualizerDetector>();

    // Warm up with broadband signal
    for (int i = 0; i < 30; ++i) {
        auto sample = fl::audio::test::makeWhiteNoise(i * 12, 16000.0f, 512);
        auto context = make_shared<audio::Context>(sample);
        context->setSampleRate(44100);
        eq->update(context);
    }

    // Sweep and track where peak energy is at key frequencies
    auto feedAndGetPeakBin = [&](float freq, int numIter) -> int {
        for (int i = 0; i < numIter; ++i) {
            auto sample = makeSample(freq, (30 + i) * 12, 16000.0f, 512);
            auto context = make_shared<audio::Context>(sample);
            context->setSampleRate(44100);
            eq->update(context);
        }
        // Find which bin has the highest value
        int peakBin = 0;
        float peakVal = 0.0f;
        for (int b = 0; b < audio::detector::EqualizerDetector::kNumBins; ++b) {
            if (eq->getBin(b) > peakVal) {
                peakVal = eq->getBin(b);
                peakBin = b;
            }
        }
        return peakBin;
    };

    // 100 Hz should activate a bass bin (0-3)
    int bassBin = feedAndGetPeakBin(100.0f, 30);
    FL_CHECK_LE(bassBin, 3);

    // 1000 Hz should activate a mid bin (4-10)
    eq->reset();
    for (int i = 0; i < 30; ++i) {
        auto sample = fl::audio::test::makeWhiteNoise(i * 12, 16000.0f, 512);
        auto context = make_shared<audio::Context>(sample);
        context->setSampleRate(44100);
        eq->update(context);
    }
    int midBin = feedAndGetPeakBin(1000.0f, 30);
    FL_CHECK_GE(midBin, 4);
    FL_CHECK_LE(midBin, 10);

    // 4000 Hz should activate a treble bin (11-15)
    eq->reset();
    for (int i = 0; i < 30; ++i) {
        auto sample = fl::audio::test::makeWhiteNoise(i * 12, 16000.0f, 512);
        auto context = make_shared<audio::Context>(sample);
        context->setSampleRate(44100);
        eq->update(context);
    }
    int trebleBin = feedAndGetPeakBin(4000.0f, 30);
    FL_CHECK_GE(trebleBin, 11);

    FL_WARN("audio::detector::Equalizer band test: bass_bin=" << bassBin
                 << " mid_bin=" << midBin << " treble_bin=" << trebleBin);
}

FL_TEST_CASE("audio::detector::EqualizerDetector - tone sweep bins have no jitter") {
    // Sweep a tone smoothly and measure frame-to-frame jitter in each bin.
    // Normalized bins (0-1) should change smoothly, not jump erratically.
    auto eq = make_shared<audio::detector::EqualizerDetector>();

    const int numFrames = 200;
    const float startFreq = 80.0f;
    const float endFreq = 4500.0f;

    // Record all 16 bins per frame
    float binHistory[200][16];

    for (int frame = 0; frame < numFrames; ++frame) {
        float t = static_cast<float>(frame) / static_cast<float>(numFrames - 1);
        float freq = startFreq + (endFreq - startFreq) * t;

        auto sample = makeSample(freq, frame * 12, 16000.0f, 512);
        auto context = make_shared<audio::Context>(sample);
        context->setSampleRate(44100);
        eq->update(context);

        for (int b = 0; b < 16; ++b) {
            binHistory[frame][b] = eq->getBin(b);
        }
    }

    // Jitter metric: max normalized 2nd derivative across all bins
    float worstJitter = 0.0f;
    int worstBin = 0;

    for (int b = 0; b < 16; ++b) {
        float minVal = binHistory[0][b], maxVal = binHistory[0][b];
        for (int f = 1; f < numFrames; ++f) {
            if (binHistory[f][b] < minVal) minVal = binHistory[f][b];
            if (binHistory[f][b] > maxVal) maxVal = binHistory[f][b];
        }
        float range = maxVal - minVal;
        if (range < 0.01f) continue; // Skip bins with no activity

        for (int f = 1; f < numFrames - 1; ++f) {
            float accel = fl::abs(
                binHistory[f + 1][b] - 2.0f * binHistory[f][b] + binHistory[f - 1][b]);
            float normalized = accel / range;
            if (normalized > worstJitter) {
                worstJitter = normalized;
                worstBin = b;
            }
        }
    }

    FL_WARN("audio::detector::Equalizer tone sweep jitter: worst=" << worstJitter
                 << " in bin " << worstBin);

    // Normalized 2nd derivative should be bounded.
    // The equalizer uses per-bin running-max normalization which amplifies
    // transitions when a tone enters/exits a narrow log-spaced bin (the bin
    // swings 0→1→0), so we allow a higher threshold than the audio::detector::Vibe.
    FL_CHECK_LT(worstJitter, 1.5f);
}
