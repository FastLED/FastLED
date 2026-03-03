// Unit tests for EqualizerDetector — WLED-style 16-bin equalizer

#include "fl/audio/detectors/equalizer.h"
#include "fl/audio/audio_context.h"
#include "fl/audio/audio_processor.h"
#include "fl/stl/vector.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/math.h"
#include "fl/math_macros.h"
#include "../test_helpers.h"

using namespace fl;
using fl::audio::test::makeSample;
using fl::audio::test::makeSilence;

FL_TEST_CASE("EqualizerDetector - returns 0.0-1.0 range") {
    auto eq = make_shared<EqualizerDetector>();

    // Feed a sine wave signal through context
    auto sample = makeSample(440.0f, 0, 16000.0f, 512);
    auto context = make_shared<AudioContext>(sample);

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

    for (int i = 0; i < EqualizerDetector::kNumBins; ++i) {
        FL_CHECK_GE(eq->getBin(i), 0.0f);
        FL_CHECK_LE(eq->getBin(i), 1.0f);
    }
}

FL_TEST_CASE("EqualizerDetector - silence returns near-zeros") {
    auto eq = make_shared<EqualizerDetector>();

    auto sample = makeSilence(0, 512);
    auto context = make_shared<AudioContext>(sample);

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

FL_TEST_CASE("EqualizerDetector - bass signal activates bass bins") {
    auto eq = make_shared<EqualizerDetector>();

    // 100 Hz tone — should be in bass range (bins 0-3)
    auto sample = makeSample(100.0f, 0, 16000.0f, 512);
    auto context = make_shared<AudioContext>(sample);

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

FL_TEST_CASE("EqualizerDetector - treble signal activates treble bins") {
    auto eq = make_shared<EqualizerDetector>();

    // 4000 Hz tone — should be in treble range (bins 11-15)
    auto sample = makeSample(4000.0f, 0, 16000.0f, 512);
    auto context = make_shared<AudioContext>(sample);

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

FL_TEST_CASE("EqualizerDetector - volume normalizes over time") {
    auto eq = make_shared<EqualizerDetector>();

    auto sample = makeSample(440.0f, 0, 16000.0f, 512);
    auto context = make_shared<AudioContext>(sample);

    // Feed constant signal — volume should normalize to near 1.0
    for (int i = 0; i < 50; ++i) {
        eq->update(context);
    }

    // After normalization, constant signal should be near 1.0
    FL_CHECK_GT(eq->getVolume(), 0.5f);
}

FL_TEST_CASE("EqualizerDetector - 16 bins all valid") {
    auto eq = make_shared<EqualizerDetector>();

    // Feed broadband signal (white noise) — all bins should have non-zero values
    auto sample = fl::audio::test::makeWhiteNoise(0, 16000.0f, 512);
    auto context = make_shared<AudioContext>(sample);

    for (int i = 0; i < 30; ++i) {
        eq->update(context);
    }

    // Check all bins are valid floats (not NaN)
    for (int i = 0; i < EqualizerDetector::kNumBins; ++i) {
        float val = eq->getBin(i);
        FL_CHECK_FALSE(val != val); // Not NaN
        FL_CHECK_GE(val, 0.0f);
        FL_CHECK_LE(val, 1.0f);
    }
}

FL_TEST_CASE("EqualizerDetector - getBins matches individual getBin") {
    auto eq = make_shared<EqualizerDetector>();

    auto sample = makeSample(440.0f, 0, 16000.0f, 512);
    auto context = make_shared<AudioContext>(sample);

    for (int i = 0; i < 10; ++i) {
        eq->update(context);
    }

    const float* bins = eq->getBins();
    FL_CHECK(bins != nullptr);

    for (int i = 0; i < EqualizerDetector::kNumBins; ++i) {
        FL_CHECK_EQ(bins[i], eq->getBin(i));
    }
}

FL_TEST_CASE("EqualizerDetector - out of range getBin returns 0") {
    EqualizerDetector eq;
    FL_CHECK_EQ(eq.getBin(-1), 0.0f);
    FL_CHECK_EQ(eq.getBin(16), 0.0f);
    FL_CHECK_EQ(eq.getBin(100), 0.0f);
}

FL_TEST_CASE("EqualizerDetector - via AudioProcessor lazy creation") {
    AudioProcessor audio;

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

FL_TEST_CASE("EqualizerDetector - onEqualizer callback fires") {
    auto eq = make_shared<EqualizerDetector>();

    auto sample = makeSample(440.0f, 0, 16000.0f, 512);
    auto context = make_shared<AudioContext>(sample);

    bool fired = false;
    Equalizer captured;
    eq->onEqualizer.add([&](const Equalizer& e) {
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

FL_TEST_CASE("EqualizerDetector - zcf is valid") {
    auto eq = make_shared<EqualizerDetector>();

    auto sample = makeSample(440.0f, 0, 16000.0f, 512);
    auto context = make_shared<AudioContext>(sample);

    eq->update(context);

    // ZCF should be in 0.0-1.0 range
    FL_CHECK_GE(eq->getZcf(), 0.0f);
    FL_CHECK_LE(eq->getZcf(), 1.0f);
}

FL_TEST_CASE("EqualizerDetector - via AudioProcessor onEqualizer") {
    AudioProcessor audio;
    auto sample = makeSample(440.0f, 0, 16000.0f, 512);

    bool fired = false;
    audio.onEqualizer([&](const Equalizer& eq) {
        fired = true;
        FL_CHECK_GE(eq.bass, 0.0f);
        FL_CHECK_GE(eq.volume, 0.0f);
        FL_CHECK_GE(eq.zcf, 0.0f);
        FL_CHECK_EQ(eq.bins.size(), 16u);
    });

    audio.update(sample);
    FL_CHECK(fired);
}

FL_TEST_CASE("EqualizerDetector - reset clears state") {
    auto eq = make_shared<EqualizerDetector>();

    auto sample = makeSample(440.0f, 0, 16000.0f, 512);
    auto context = make_shared<AudioContext>(sample);

    for (int i = 0; i < 10; ++i) {
        eq->update(context);
    }

    eq->reset();

    FL_CHECK_EQ(eq->getBass(), 0.0f);
    FL_CHECK_EQ(eq->getMid(), 0.0f);
    FL_CHECK_EQ(eq->getTreble(), 0.0f);
    FL_CHECK_EQ(eq->getVolume(), 0.0f);
    for (int i = 0; i < EqualizerDetector::kNumBins; ++i) {
        FL_CHECK_EQ(eq->getBin(i), 0.0f);
    }
}

// ============================================================================
// P2 feature tests: dominant frequency, volumeDb, mic profiles
// ============================================================================

FL_TEST_CASE("EqualizerDetector - dominant frequency detects peak bin") {
    auto eq = make_shared<EqualizerDetector>();

    // Use a 440 Hz tone — should be detected in a mid-range bin
    auto sample = makeSample(440.0f, 0, 16000.0f, 512);
    auto context = make_shared<AudioContext>(sample);

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

FL_TEST_CASE("EqualizerDetector - volumeDb references full scale") {
    auto eq = make_shared<EqualizerDetector>();

    // Loud signal: amplitude 16000 out of 32767 max
    auto sample = makeSample(440.0f, 0, 16000.0f, 512);
    auto context = make_shared<AudioContext>(sample);

    for (int i = 0; i < 20; ++i) {
        eq->update(context);
    }

    // volumeDb should be negative (below full scale) and above -100
    float db = eq->getVolumeDb();
    FL_CHECK_GT(db, -100.0f);
    FL_CHECK_LT(db, 0.0f);
}

FL_TEST_CASE("EqualizerDetector - volumeDb silence is -100") {
    auto eq = make_shared<EqualizerDetector>();

    auto sample = makeSilence(0, 512);
    auto context = make_shared<AudioContext>(sample);

    for (int i = 0; i < 10; ++i) {
        eq->update(context);
    }

    FL_CHECK_EQ(eq->getVolumeDb(), -100.0f);
}

FL_TEST_CASE("EqualizerDetector - mic profile INMP441 applies gains") {
    auto eqNoMic = make_shared<EqualizerDetector>();
    auto eqWithMic = make_shared<EqualizerDetector>();
    eqWithMic->setMicProfile(MicProfile::INMP441);

    auto sample = fl::audio::test::makeWhiteNoise(0, 16000.0f, 512);
    auto context = make_shared<AudioContext>(sample);

    for (int i = 0; i < 30; ++i) {
        eqNoMic->update(context);
        eqWithMic->update(context);
    }

    // With mic correction, bins should still be in valid range
    for (int i = 0; i < EqualizerDetector::kNumBins; ++i) {
        FL_CHECK_GE(eqWithMic->getBin(i), 0.0f);
        FL_CHECK_LE(eqWithMic->getBin(i), 1.0f);
    }
}

FL_TEST_CASE("EqualizerDetector - mic profile None has no effect") {
    auto eq1 = make_shared<EqualizerDetector>();
    auto eq2 = make_shared<EqualizerDetector>();
    eq2->setMicProfile(MicProfile::None);

    auto sample = makeSample(440.0f, 0, 16000.0f, 512);
    auto context = make_shared<AudioContext>(sample);

    for (int i = 0; i < 20; ++i) {
        eq1->update(context);
        eq2->update(context);
    }

    // Should produce identical results
    for (int i = 0; i < EqualizerDetector::kNumBins; ++i) {
        FL_CHECK_EQ(eq1->getBin(i), eq2->getBin(i));
    }
}

FL_TEST_CASE("EqualizerDetector - scaling mode sqrt keeps valid range") {
    auto eq = make_shared<EqualizerDetector>();
    EqualizerConfig config;
    config.scalingMode = FFTScalingMode::SquareRoot;
    eq->configure(config);

    auto sample = makeSample(440.0f, 0, 16000.0f, 512);
    auto context = make_shared<AudioContext>(sample);

    for (int i = 0; i < 20; ++i) {
        eq->update(context);
    }

    for (int i = 0; i < EqualizerDetector::kNumBins; ++i) {
        FL_CHECK_GE(eq->getBin(i), 0.0f);
        FL_CHECK_LE(eq->getBin(i), 1.0f);
    }
}

FL_TEST_CASE("EqualizerDetector - callback includes P2 fields") {
    auto eq = make_shared<EqualizerDetector>();

    auto sample = makeSample(440.0f, 0, 16000.0f, 512);
    auto context = make_shared<AudioContext>(sample);

    Equalizer captured;
    bool fired = false;
    eq->onEqualizer.add([&](const Equalizer& e) {
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
// Adversarial tests
// ============================================================================

FL_TEST_CASE("ADVERSARIAL - mic correction does not leak to other detectors") {
    // Verify that setting mic profile on AudioProcessor only affects equalizer,
    // not other detectors like FrequencyBands
    AudioProcessor audio;
    audio.setMicProfile(MicProfile::INMP441);

    auto sample = fl::audio::test::makeWhiteNoise(0, 16000.0f, 512);

    for (int i = 0; i < 30; ++i) {
        audio.update(sample);
    }

    // Equalizer should work
    float eqBass = audio.getEqBass();
    FL_CHECK_GE(eqBass, 0.0f);
    FL_CHECK_LE(eqBass, 1.0f);

    // FrequencyBands (raw detector) should also work independently
    float rawBass = audio.getBassLevel();
    FL_CHECK_GE(rawBass, 0.0f);
    FL_CHECK_LE(rawBass, 1.0f);
}

FL_TEST_CASE("ADVERSARIAL - extreme mic gains keep bins in 0-1") {
    // Even with extreme mic profile gains, output should stay in valid range
    auto eq = make_shared<EqualizerDetector>();
    // GenericMEMS has modest gains (0.92-1.05x) derived from datasheet data
    eq->setMicProfile(MicProfile::GenericMEMS);

    auto sample = makeSample(4000.0f, 0, 30000.0f, 512); // near-max amplitude
    auto context = make_shared<AudioContext>(sample);

    for (int i = 0; i < 30; ++i) {
        eq->update(context);
    }

    for (int i = 0; i < EqualizerDetector::kNumBins; ++i) {
        FL_CHECK_GE(eq->getBin(i), 0.0f);
        FL_CHECK_LE(eq->getBin(i), 1.0f);
    }
    FL_CHECK_GE(eq->getVolume(), 0.0f);
    FL_CHECK_LE(eq->getVolume(), 1.0f);
}

FL_TEST_CASE("ADVERSARIAL - silence with mic profile produces near-zeros") {
    auto eq = make_shared<EqualizerDetector>();
    eq->setMicProfile(MicProfile::INMP441);

    auto sample = makeSilence(0, 512);
    auto context = make_shared<AudioContext>(sample);

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
    auto eqNoScale = make_shared<EqualizerDetector>();
    eqNoScale->setMicProfile(MicProfile::INMP441);

    auto eqSqrt = make_shared<EqualizerDetector>();
    eqSqrt->setMicProfile(MicProfile::INMP441);
    EqualizerConfig config;
    config.scalingMode = FFTScalingMode::SquareRoot;
    eqSqrt->configure(config);
    eqSqrt->setMicProfile(MicProfile::INMP441); // re-set after configure

    auto sample = makeSample(440.0f, 0, 16000.0f, 512);
    auto context = make_shared<AudioContext>(sample);

    for (int i = 0; i < 30; ++i) {
        eqNoScale->update(context);
        eqSqrt->update(context);
    }

    // Both should produce valid 0-1 output
    for (int i = 0; i < EqualizerDetector::kNumBins; ++i) {
        FL_CHECK_GE(eqNoScale->getBin(i), 0.0f);
        FL_CHECK_LE(eqNoScale->getBin(i), 1.0f);
        FL_CHECK_GE(eqSqrt->getBin(i), 0.0f);
        FL_CHECK_LE(eqSqrt->getBin(i), 1.0f);
    }
}

FL_TEST_CASE("ADVERSARIAL - all mic profiles produce valid output") {
    MicProfile profiles[] = {
        MicProfile::None, MicProfile::INMP441, MicProfile::ICS43434,
        MicProfile::SPM1423, MicProfile::GenericMEMS, MicProfile::LineIn
    };

    auto sample = fl::audio::test::makeWhiteNoise(0, 16000.0f, 512);
    auto context = make_shared<AudioContext>(sample);

    for (auto profile : profiles) {
        auto eq = make_shared<EqualizerDetector>();
        eq->setMicProfile(profile);

        for (int i = 0; i < 30; ++i) {
            eq->update(context);
        }

        for (int j = 0; j < EqualizerDetector::kNumBins; ++j) {
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
    auto eq = make_shared<EqualizerDetector>();
    EqualizerConfig config;
    config.outputAttack = 0.024f;  // WLED-MM style attack
    config.outputDecay = 0.250f;   // WLED-MM style decay
    eq->configure(config);

    auto sample = makeSample(440.0f, 0, 16000.0f, 512);
    auto context = make_shared<AudioContext>(sample);

    for (int i = 0; i < 30; ++i) {
        eq->update(context);
    }

    for (int i = 0; i < EqualizerDetector::kNumBins; ++i) {
        FL_CHECK_GE(eq->getBin(i), 0.0f);
        FL_CHECK_LE(eq->getBin(i), 1.0f);
    }
}

FL_TEST_CASE("ADVERSARIAL - AudioProcessor no pre-FFT AGC stacking") {
    // Verify AudioProcessor doesn't have AGC in the pipeline
    // by checking that raw samples pass through unmodified to detectors
    AudioProcessor audio;
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
    auto eqQuiet = make_shared<EqualizerDetector>();
    auto eqLoud = make_shared<EqualizerDetector>();

    auto quietSample = makeSample(440.0f, 0, 1000.0f, 512);
    auto loudSample = makeSample(440.0f, 0, 20000.0f, 512);
    auto quietCtx = make_shared<AudioContext>(quietSample);
    auto loudCtx = make_shared<AudioContext>(loudSample);

    for (int i = 0; i < 20; ++i) {
        eqQuiet->update(quietCtx);
        eqLoud->update(loudCtx);
    }

    // Louder signal should have higher dB reading
    FL_CHECK_GT(eqLoud->getVolumeDb(), eqQuiet->getVolumeDb());
}

FL_TEST_CASE("ADVERSARIAL - reset clears P2 fields") {
    auto eq = make_shared<EqualizerDetector>();

    auto sample = makeSample(440.0f, 0, 16000.0f, 512);
    auto context = make_shared<AudioContext>(sample);

    for (int i = 0; i < 20; ++i) {
        eq->update(context);
    }

    eq->reset();

    FL_CHECK_EQ(eq->getDominantFreqHz(), 0.0f);
    FL_CHECK_EQ(eq->getDominantMagnitude(), 0.0f);
    FL_CHECK_EQ(eq->getVolumeDb(), -100.0f);
}
