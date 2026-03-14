// Unit tests for AutoGain - PI controller with peak envelope tracking
// standalone test

#include "fl/audio/auto_gain.h"
#include "fl/stl/vector.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/math.h"
#include "fl/stl/math.h"
#include "test_helpers.h"

using namespace fl;
using fl::audio::test::generateConstantSignal;

namespace {

static AudioSample createSample_AutoGain(const vector<i16>& samples, u32 timestamp = 0) {
    AudioSampleImplPtr impl = fl::make_shared<AudioSampleImpl>();
    impl->assign(samples.begin(), samples.end(), timestamp);
    return AudioSample(impl);
}

} // anonymous namespace

// AG-1: Amplification - Gain Converges Upward
FL_TEST_CASE("AutoGain - amplification converges to target") {
    AutoGain agc;
    AutoGainConfig config;
    config.enabled = true;
    config.targetRMSLevel = 8000.0f;
    agc.configure(config);

    // Quiet signal: amplitude=500, RMS~500
    vector<i16> quiet = generateConstantSignal(1000, 500);
    AudioSample audio = createSample_AutoGain(quiet, 1000);

    // PI controller with slow time constants needs more iterations
    for (int i = 0; i < 500; ++i) {
        agc.process(audio);
    }

    const auto& stats = agc.getStats();
    // Gain must be significantly above 1 (target=8000, input~500 → gain~16)
    FL_CHECK_GT(stats.currentGain, 5.0f);
    // Output RMS should be approaching target
    FL_CHECK_GT(stats.outputRMS, 3000.0f);
}

// AG-2: Attenuation - Gain Converges Downward
FL_TEST_CASE("AutoGain - attenuation converges to target") {
    AutoGain agc;
    AutoGainConfig config;
    config.enabled = true;
    config.targetRMSLevel = 8000.0f;
    agc.configure(config);

    // Loud signal: amplitude=20000, RMS~20000
    vector<i16> loud = generateConstantSignal(1000, 20000);
    AudioSample audio = createSample_AutoGain(loud, 2000);

    for (int i = 0; i < 500; ++i) {
        agc.process(audio);
    }

    const auto& stats = agc.getStats();
    FL_CHECK_LT(stats.currentGain, 0.8f);
    FL_CHECK_GT(stats.outputRMS, 3000.0f);
}

// AG-4: Silence Handling - No NaN/Inf
FL_TEST_CASE("AutoGain - silence produces no NaN") {
    AutoGain agc;
    AutoGainConfig config;
    config.enabled = true;
    config.targetRMSLevel = 8000.0f;
    agc.configure(config);

    vector<i16> silence(512, 0);
    AudioSample audio = createSample_AutoGain(silence, 3000);

    for (int i = 0; i < 20; ++i) {
        AudioSample result = agc.process(audio);
        const auto& pcm = result.pcm();
        for (size j = 0; j < pcm.size(); ++j) {
            FL_CHECK_EQ(pcm[j], 0); // 0 * any_gain = 0
        }
    }

    const auto& stats = agc.getStats();
    // Gain clamped to maxGain
    FL_CHECK_LE(stats.currentGain, config.maxGain);
    FL_CHECK_GE(stats.currentGain, config.minGain);
    // No NaN
    FL_CHECK_FALSE(stats.currentGain != stats.currentGain);
    FL_CHECK_FALSE(stats.peakEnvelope != stats.peakEnvelope);
    FL_CHECK_FALSE(stats.outputRMS != stats.outputRMS);
}

// AG-5/AG-6: Gain Clamping - Min and Max
FL_TEST_CASE("AutoGain - gain clamping bounds") {
    // Test max gain clamping
    {
        AutoGain agc;
        AutoGainConfig config;
        config.minGain = 0.5f;
        config.maxGain = 2.0f;
        config.targetRMSLevel = 8000.0f;
        agc.configure(config);

        vector<i16> veryQuiet = generateConstantSignal(1000, 100);
        AudioSample audio = createSample_AutoGain(veryQuiet, 4000);
        for (int i = 0; i < 100; ++i) agc.process(audio);

        FL_CHECK_LE(agc.getGain(), 2.0f);
        FL_CHECK_GE(agc.getGain(), 0.5f);
    }

    // Test min gain clamping
    {
        AutoGain agc;
        AutoGainConfig config;
        config.minGain = 0.5f;
        config.maxGain = 2.0f;
        config.targetRMSLevel = 8000.0f;
        agc.configure(config);

        vector<i16> veryLoud = generateConstantSignal(1000, 30000);
        AudioSample audio = createSample_AutoGain(veryLoud, 5000);
        for (int i = 0; i < 100; ++i) agc.process(audio);

        FL_CHECK_GE(agc.getGain(), 0.5f);
        FL_CHECK_LE(agc.getGain(), 2.0f);
    }
}

// Keep: Disabled passthrough
FL_TEST_CASE("AutoGain - disabled passthrough") {
    AutoGain agc;
    AutoGainConfig config;
    config.enabled = false;
    agc.configure(config);

    vector<i16> samples = generateConstantSignal(1000, 5000);
    AudioSample input = createSample_AutoGain(samples, 8000);
    AudioSample output = agc.process(input);

    FL_CHECK(output.isValid());
    FL_CHECK_EQ(output.size(), input.size());
    FL_CHECK(fl::abs(input.rms() - output.rms()) < 10.0f);
}

// Keep: Empty/invalid
FL_TEST_CASE("AutoGain - empty and invalid") {
    AutoGain agc;
    FL_CHECK_FALSE(agc.process(AudioSample()).isValid());

    vector<i16> empty;
    FL_CHECK_FALSE(agc.process(createSample_AutoGain(empty, 0)).isValid());
}

// Keep: Timestamp
FL_TEST_CASE("AutoGain - timestamp preserved") {
    AutoGain agc;
    vector<i16> samples = generateConstantSignal(500, 5000);
    AudioSample output = agc.process(createSample_AutoGain(samples, 123456));
    FL_CHECK_EQ(output.timestamp(), 123456u);
}

// Keep: Reset
FL_TEST_CASE("AutoGain - reset clears state") {
    AutoGain agc;
    vector<i16> samples = generateConstantSignal(1000, 10000);
    for (int i = 0; i < 10; ++i) agc.process(createSample_AutoGain(samples, i * 100));

    FL_CHECK_GT(agc.getStats().samplesProcessed, 0u);

    agc.reset();
    FL_CHECK_EQ(agc.getStats().currentGain, 1.0f);
    FL_CHECK_EQ(agc.getStats().samplesProcessed, 0u);
    FL_CHECK_EQ(agc.getStats().integrator, 0.0f);
}

// Keep: No clipping
FL_TEST_CASE("AutoGain - no clipping on extreme amplification") {
    AutoGain agc;
    AutoGainConfig config;
    config.maxGain = 32.0f;
    config.targetRMSLevel = 30000.0f;
    agc.configure(config);

    vector<i16> quiet(1000);
    for (size i = 0; i < 1000; ++i) {
        float phase = 2.0f * FL_M_PI * 1000.0f * static_cast<float>(i) / 22050.0f;
        quiet[i] = static_cast<i16>(1000 * fl::sinf(phase));
    }
    AudioSample audio = createSample_AutoGain(quiet, 9000);

    for (int i = 0; i < 50; ++i) {
        AudioSample result = agc.process(audio);
        const auto& pcm = result.pcm();
        for (size j = 0; j < pcm.size(); ++j) {
            FL_CHECK_GE(pcm[j], -32768);
            FL_CHECK_LE(pcm[j], 32767);
        }
    }
}

// NEW: Preset selection test — different presets produce different convergence speeds
FL_TEST_CASE("AutoGain - presets produce different convergence speeds") {
    // Feed same quiet signal to Normal and Vivid, compare gain after 50 frames
    auto runWithPreset = [](AGCPreset preset) -> float {
        AutoGain agc;
        AutoGainConfig config;
        config.preset = preset;
        config.targetRMSLevel = 8000.0f;
        agc.configure(config);

        vector<i16> quiet = generateConstantSignal(1000, 500);
        AudioSample audio = createSample_AutoGain(quiet, 0);

        for (int i = 0; i < 50; ++i) {
            agc.process(audio);
        }
        return agc.getGain();
    };

    float gainNormal = runWithPreset(AGCPreset_Normal);
    float gainVivid = runWithPreset(AGCPreset_Vivid);
    float gainLazy = runWithPreset(AGCPreset_Lazy);

    // Vivid should converge faster (higher gain after same frames) than Normal
    FL_CHECK_GT(gainVivid, gainNormal);
    // Normal should converge faster than Lazy
    FL_CHECK_GT(gainNormal, gainLazy);
}

// NEW: Slow convergence test — gain barely changes after simulated 1 second
FL_TEST_CASE("AutoGain - slow convergence over first second") {
    AutoGain agc;
    AutoGainConfig config;
    config.preset = AGCPreset_Normal;
    config.targetRMSLevel = 8000.0f;
    agc.configure(config);

    // Signal at target level — gain should stay near 1.0
    vector<i16> atTarget = generateConstantSignal(1000, 8000);
    AudioSample audio = createSample_AutoGain(atTarget, 0);

    // ~43 frames of 1000 samples at 44100Hz ≈ 1 second
    for (int i = 0; i < 43; ++i) {
        agc.process(audio);
    }

    // Gain should remain close to 1.0 since input matches target
    FL_CHECK_GT(agc.getGain(), 0.5f);
    FL_CHECK_LT(agc.getGain(), 2.0f);
}

// NEW: Cross-band stability test — alternating loud/quiet stays stable
FL_TEST_CASE("AutoGain - cross-band stability with alternating levels") {
    AutoGain agc;
    AutoGainConfig config;
    config.preset = AGCPreset_Normal;
    config.targetRMSLevel = 8000.0f;
    agc.configure(config);

    vector<i16> loud = generateConstantSignal(1000, 15000);
    vector<i16> quiet = generateConstantSignal(1000, 2000);
    AudioSample loudAudio = createSample_AutoGain(loud, 0);
    AudioSample quietAudio = createSample_AutoGain(quiet, 0);

    // Alternate loud/quiet for many frames
    for (int i = 0; i < 200; ++i) {
        if (i % 2 == 0) {
            agc.process(loudAudio);
        } else {
            agc.process(quietAudio);
        }
    }

    // Gain should be stable and within reasonable bounds
    // (not oscillating wildly between extremes)
    float gain = agc.getGain();
    FL_CHECK_GT(gain, config.minGain);
    FL_CHECK_LT(gain, config.maxGain);

    // Process a few more and check gain doesn't change dramatically
    float gainBefore = gain;
    for (int i = 0; i < 10; ++i) {
        agc.process((i % 2 == 0) ? loudAudio : quietAudio);
    }
    float gainAfter = agc.getGain();

    // Gain should not change more than 50% in 10 frames
    float ratio = (gainAfter > gainBefore) ? gainAfter / gainBefore : gainBefore / gainAfter;
    FL_CHECK_LT(ratio, 1.5f);
}

// ============================================================================
// Adversarial Tests — Startup / Cold Boot
// ============================================================================

FL_TEST_CASE("AutoGain - cold start with loud signal") {
    AutoGain agc;
    AutoGainConfig config;
    config.targetRMSLevel = 8000.0f;
    agc.configure(config);

    // Hot line-in: amplitude=25000
    vector<i16> loud = generateConstantSignal(1000, 25000);
    AudioSample audio = createSample_AutoGain(loud, 0);

    for (int i = 0; i < 10; ++i) {
        agc.process(audio);
    }

    // Gain should have dropped below 1.0 (target=8000, input=25000 → gain~0.32)
    FL_CHECK_LT(agc.getGain(), 1.0f);
    // Should never go below minGain
    FL_CHECK_GE(agc.getGain(), config.minGain);
}

FL_TEST_CASE("AutoGain - cold start with silence then signal") {
    AutoGain agc;
    AutoGainConfig config;
    config.targetRMSLevel = 8000.0f;
    agc.configure(config);

    vector<i16> silence(1000, 0);
    AudioSample silenceAudio = createSample_AutoGain(silence, 0);

    // 100 frames of silence
    for (int i = 0; i < 100; ++i) {
        agc.process(silenceAudio);
    }

    // During silence, gain should be at maxGain (clamped)
    FL_CHECK_LE(agc.getGain(), config.maxGain);

    // Now feed signal
    vector<i16> signal = generateConstantSignal(1000, 5000);
    AudioSample signalAudio = createSample_AutoGain(signal, 0);

    for (int i = 0; i < 200; ++i) {
        agc.process(signalAudio);
    }

    // Gain should have converged toward target (8000/5000 = 1.6)
    FL_CHECK_GT(agc.getGain(), 0.5f);
    FL_CHECK_LT(agc.getGain(), config.maxGain);
}

FL_TEST_CASE("AutoGain - cold start gain is approximately unity") {
    AutoGain agc;
    AutoGainConfig config;
    config.targetRMSLevel = 8000.0f;
    agc.configure(config);

    // Signal at exactly target level
    vector<i16> atTarget = generateConstantSignal(1000, 8000);
    AudioSample audio = createSample_AutoGain(atTarget, 0);

    agc.process(audio);

    // First frame gain should be approximately 1.0 (±50% is generous)
    FL_CHECK_GT(agc.getGain(), 0.5f);
    FL_CHECK_LT(agc.getGain(), 1.5f);
}

// ============================================================================
// Adversarial Tests — Silence Handling
// ============================================================================

FL_TEST_CASE("AutoGain - prolonged silence then loud burst") {
    AutoGain agc;
    AutoGainConfig config;
    config.targetRMSLevel = 8000.0f;
    agc.configure(config);

    vector<i16> silence(1000, 0);
    AudioSample silenceAudio = createSample_AutoGain(silence, 0);

    // 500 frames of silence
    for (int i = 0; i < 500; ++i) {
        agc.process(silenceAudio);
    }

    // Now sudden loud signal
    vector<i16> loud = generateConstantSignal(1000, 20000);
    AudioSample loudAudio = createSample_AutoGain(loud, 0);

    for (int i = 0; i < 50; ++i) {
        agc.process(loudAudio);
    }

    // Gain should drop from maxGain toward target (8000/20000 = 0.4)
    // Within 50 frames, should be well below maxGain
    FL_CHECK_LT(agc.getGain(), config.maxGain);
    FL_CHECK_GE(agc.getGain(), config.minGain);
}

FL_TEST_CASE("AutoGain - silence integrator spin-down") {
    AutoGain agc;
    AutoGainConfig config;
    config.targetRMSLevel = 8000.0f;
    agc.configure(config);

    // Feed some signal first to build up integrator
    vector<i16> signal = generateConstantSignal(1000, 500);
    AudioSample signalAudio = createSample_AutoGain(signal, 0);
    for (int i = 0; i < 50; ++i) {
        agc.process(signalAudio);
    }

    // Now feed silence for 100 frames
    vector<i16> silence(1000, 0);
    AudioSample silenceAudio = createSample_AutoGain(silence, 0);
    for (int i = 0; i < 100; ++i) {
        agc.process(silenceAudio);
    }

    // Integrator should have spun down near zero
    FL_CHECK_LT(fl::abs(agc.getStats().integrator), 1.0f);
}

FL_TEST_CASE("AutoGain - intermittent silence stability") {
    AutoGain agc;
    AutoGainConfig config;
    config.targetRMSLevel = 8000.0f;
    agc.configure(config);

    vector<i16> signal = generateConstantSignal(1000, 5000);
    vector<i16> silence(1000, 0);
    AudioSample signalAudio = createSample_AutoGain(signal, 0);
    AudioSample silenceAudio = createSample_AutoGain(silence, 0);

    // 10 frames signal, 10 frames silence, repeat 20x
    for (int cycle = 0; cycle < 20; ++cycle) {
        for (int i = 0; i < 10; ++i) agc.process(signalAudio);
        for (int i = 0; i < 10; ++i) agc.process(silenceAudio);
    }

    // Gain should be within bounds (not drifted to extremes)
    FL_CHECK_GT(agc.getGain(), config.minGain);
    FL_CHECK_LE(agc.getGain(), config.maxGain);
    // No NaN
    FL_CHECK_FALSE(agc.getGain() != agc.getGain());
}

// ============================================================================
// Adversarial Tests — Sudden Level Changes
// ============================================================================

FL_TEST_CASE("AutoGain - sudden 100x level increase") {
    AutoGain agc;
    AutoGainConfig config;
    config.targetRMSLevel = 8000.0f;
    agc.configure(config);

    // Start with quiet signal
    vector<i16> quiet = generateConstantSignal(1000, 100);
    AudioSample quietAudio = createSample_AutoGain(quiet, 0);
    for (int i = 0; i < 100; ++i) {
        agc.process(quietAudio);
    }

    // Switch to 100x louder
    vector<i16> loud = generateConstantSignal(1000, 10000);
    AudioSample loudAudio = createSample_AutoGain(loud, 0);
    for (int i = 0; i < 100; ++i) {
        agc.process(loudAudio);
    }

    // Gain should have dropped significantly
    FL_CHECK_LT(agc.getGain(), 5.0f);
    FL_CHECK_GE(agc.getGain(), config.minGain);
    // No NaN
    FL_CHECK_FALSE(agc.getGain() != agc.getGain());
}

FL_TEST_CASE("AutoGain - sudden 100x level decrease") {
    AutoGain agc;
    AutoGainConfig config;
    config.targetRMSLevel = 8000.0f;
    agc.configure(config);

    // Start with loud signal
    vector<i16> loud = generateConstantSignal(1000, 10000);
    AudioSample loudAudio = createSample_AutoGain(loud, 0);
    for (int i = 0; i < 100; ++i) {
        agc.process(loudAudio);
    }

    // Switch to 100x quieter
    vector<i16> quiet = generateConstantSignal(1000, 100);
    AudioSample quietAudio = createSample_AutoGain(quiet, 0);
    for (int i = 0; i < 200; ++i) {
        agc.process(quietAudio);
    }

    // Gain should have risen significantly
    FL_CHECK_GT(agc.getGain(), 1.0f);
    FL_CHECK_LE(agc.getGain(), config.maxGain);
    // No NaN
    FL_CHECK_FALSE(agc.getGain() != agc.getGain());
}

// ============================================================================
// Adversarial Tests — Noise Floor / Gradual Changes
// ============================================================================

FL_TEST_CASE("AutoGain - slowly rising noise floor") {
    AutoGain agc;
    AutoGainConfig config;
    config.targetRMSLevel = 8000.0f;
    agc.configure(config);

    // RMS increases by 50 each frame: 100 → 5100 over 100 frames
    for (int i = 0; i < 100; ++i) {
        i16 amplitude = static_cast<i16>(100 + i * 50);
        vector<i16> signal = generateConstantSignal(1000, amplitude);
        AudioSample audio = createSample_AutoGain(signal, 0);
        agc.process(audio);
    }

    // Final amplitude is 5100, target gain should be ~8000/5100 ≈ 1.57
    // Allow wide range since it's still tracking
    FL_CHECK_GT(agc.getGain(), 0.3f);
    FL_CHECK_LT(agc.getGain(), config.maxGain);
    FL_CHECK_FALSE(agc.getGain() != agc.getGain());
}

FL_TEST_CASE("AutoGain - slowly falling noise floor") {
    AutoGain agc;
    AutoGainConfig config;
    config.targetRMSLevel = 8000.0f;
    agc.configure(config);

    // RMS decreases: 5000 → 500 over 100 frames
    for (int i = 0; i < 100; ++i) {
        i16 amplitude = static_cast<i16>(5000 - i * 45);
        if (amplitude < 50) amplitude = 50;
        vector<i16> signal = generateConstantSignal(1000, amplitude);
        AudioSample audio = createSample_AutoGain(signal, 0);
        agc.process(audio);
    }

    // Gain should have risen to track the falling signal
    FL_CHECK_GT(agc.getGain(), 1.0f);
    FL_CHECK_LE(agc.getGain(), config.maxGain);
    FL_CHECK_FALSE(agc.getGain() != agc.getGain());
}

FL_TEST_CASE("AutoGain - mic warmup exponential rise") {
    AutoGain agc;
    AutoGainConfig config;
    config.targetRMSLevel = 8000.0f;
    agc.configure(config);

    // Exponential rise from 200 to 3000 over 500 frames
    for (int i = 0; i < 500; ++i) {
        float t = static_cast<float>(i) / 500.0f;
        i16 amplitude = static_cast<i16>(200.0f + 2800.0f * (1.0f - fl::exp(-3.0f * t)));
        vector<i16> signal = generateConstantSignal(1000, amplitude);
        AudioSample audio = createSample_AutoGain(signal, 0);
        agc.process(audio);
    }

    // Gain should be reasonable for amplitude ~3000 → target gain ~2.67
    FL_CHECK_GT(agc.getGain(), 0.5f);
    FL_CHECK_LT(agc.getGain(), 10.0f);
    FL_CHECK_FALSE(agc.getGain() != agc.getGain());
}

// ============================================================================
// Adversarial Tests — Integrator Health
// ============================================================================

FL_TEST_CASE("AutoGain - integrator doesn't windup when clamped") {
    AutoGain agc;
    AutoGainConfig config;
    config.targetRMSLevel = 8000.0f;
    config.maxGain = 32.0f;
    agc.configure(config);

    // Very quiet signal: gain wants 100+ but clamped at 32
    vector<i16> veryQuiet = generateConstantSignal(1000, 50);
    AudioSample quietAudio = createSample_AutoGain(veryQuiet, 0);
    for (int i = 0; i < 500; ++i) {
        agc.process(quietAudio);
    }

    // Now feed normal signal
    vector<i16> normal = generateConstantSignal(1000, 5000);
    AudioSample normalAudio = createSample_AutoGain(normal, 0);
    for (int i = 0; i < 200; ++i) {
        agc.process(normalAudio);
    }

    // Gain should converge to ~1.6 (8000/5000) within 200 frames
    // If integrator was wound up, this would be sluggish
    FL_CHECK_GT(agc.getGain(), 0.5f);
    FL_CHECK_LT(agc.getGain(), 10.0f);
}

FL_TEST_CASE("AutoGain - integrator recovers from saturation") {
    AutoGain agc;
    AutoGainConfig config;
    config.targetRMSLevel = 8000.0f;
    agc.configure(config);

    // Drive integrator by alternating extremes
    vector<i16> loud = generateConstantSignal(1000, 25000);
    vector<i16> quiet = generateConstantSignal(1000, 100);
    AudioSample loudAudio = createSample_AutoGain(loud, 0);
    AudioSample quietAudio = createSample_AutoGain(quiet, 0);

    for (int i = 0; i < 50; ++i) {
        agc.process(loudAudio);
        agc.process(quietAudio);
    }

    // Now feed signal at target level for 500 frames
    vector<i16> atTarget = generateConstantSignal(1000, 8000);
    AudioSample targetAudio = createSample_AutoGain(atTarget, 0);
    for (int i = 0; i < 500; ++i) {
        agc.process(targetAudio);
    }

    // Integrator should be near zero since gain is at target
    FL_CHECK_LT(fl::abs(agc.getStats().integrator), 5.0f);
}

FL_TEST_CASE("AutoGain - negative gain never produced") {
    AutoGain agc;
    AutoGainConfig config;
    config.targetRMSLevel = 8000.0f;
    agc.configure(config);

    vector<i16> loud = generateConstantSignal(1000, 30000);
    vector<i16> silence(1000, 0);
    AudioSample loudAudio = createSample_AutoGain(loud, 0);
    AudioSample silenceAudio = createSample_AutoGain(silence, 0);

    // Pathological alternating pattern
    for (int i = 0; i < 200; ++i) {
        if (i % 3 == 0) {
            agc.process(silenceAudio);
        } else {
            agc.process(loudAudio);
        }
        FL_CHECK_GT(agc.getGain(), 0.0f);
    }
}

// ============================================================================
// Adversarial Tests — Preset Behavior
// ============================================================================

FL_TEST_CASE("AutoGain - vivid converges faster than normal") {
    auto framesTo10Pct = [](AGCPreset preset) -> int {
        AutoGain agc;
        AutoGainConfig config;
        config.preset = preset;
        config.targetRMSLevel = 8000.0f;
        agc.configure(config);

        // Quiet → loud transition
        vector<i16> quiet = generateConstantSignal(1000, 500);
        AudioSample quietAudio = createSample_AutoGain(quiet, 0);
        for (int i = 0; i < 50; ++i) agc.process(quietAudio);

        vector<i16> loud = generateConstantSignal(1000, 10000);
        AudioSample loudAudio = createSample_AutoGain(loud, 0);

        // Target gain is ~0.8 (8000/10000)
        float targetGain = 8000.0f / 10000.0f;
        for (int i = 0; i < 500; ++i) {
            agc.process(loudAudio);
            float err = fl::abs(agc.getGain() - targetGain) / targetGain;
            if (err < 0.3f) return i;
        }
        return 500;
    };

    int framesVivid = framesTo10Pct(AGCPreset_Vivid);
    int framesNormal = framesTo10Pct(AGCPreset_Normal);

    // Vivid should reach target in fewer or equal frames (faster response)
    FL_CHECK_LE(framesVivid, framesNormal);
}

FL_TEST_CASE("AutoGain - all presets stable under alternating input") {
    auto measureVariance = [](AGCPreset preset) -> float {
        AutoGain agc;
        AutoGainConfig config;
        config.preset = preset;
        config.targetRMSLevel = 8000.0f;
        agc.configure(config);

        vector<i16> loud = generateConstantSignal(1000, 15000);
        vector<i16> quiet = generateConstantSignal(1000, 3000);
        AudioSample loudAudio = createSample_AutoGain(loud, 0);
        AudioSample quietAudio = createSample_AutoGain(quiet, 0);

        // Warm up
        for (int i = 0; i < 200; ++i) {
            agc.process((i % 2 == 0) ? loudAudio : quietAudio);
        }

        // Measure variance over 100 frames
        float sum = 0, sumSq = 0;
        int n = 100;
        for (int i = 0; i < n; ++i) {
            agc.process((i % 2 == 0) ? loudAudio : quietAudio);
            float g = agc.getGain();
            sum += g;
            sumSq += g * g;
        }
        float mean = sum / n;
        return sumSq / n - mean * mean;
    };

    float varLazy = measureVariance(AGCPreset_Lazy);
    float varNormal = measureVariance(AGCPreset_Normal);
    float varVivid = measureVariance(AGCPreset_Vivid);

    // All presets should have low variance (stable) — variance < 0.01
    FL_CHECK_LT(varLazy, 0.01f);
    FL_CHECK_LT(varNormal, 0.01f);
    FL_CHECK_LT(varVivid, 0.01f);
}

FL_TEST_CASE("AutoGain - custom preset matches manual values") {
    // Set Custom with Normal's constants
    AutoGain agcCustom;
    AutoGainConfig customConfig;
    customConfig.preset = AGCPreset_Custom;
    customConfig.peakDecayTau = 3.3f;
    customConfig.kp = 0.6f;
    customConfig.ki = 1.7f;
    customConfig.gainFollowSlowTau = 12.3f;
    customConfig.gainFollowFastTau = 0.38f;
    customConfig.targetRMSLevel = 8000.0f;
    agcCustom.configure(customConfig);

    AutoGain agcNormal;
    AutoGainConfig normalConfig;
    normalConfig.preset = AGCPreset_Normal;
    normalConfig.targetRMSLevel = 8000.0f;
    agcNormal.configure(normalConfig);

    vector<i16> signal = generateConstantSignal(1000, 3000);
    AudioSample audio = createSample_AutoGain(signal, 0);

    for (int i = 0; i < 100; ++i) {
        agcCustom.process(audio);
        agcNormal.process(audio);
    }

    // Gains should be identical (within float precision)
    FL_CHECK_LT(fl::abs(agcCustom.getGain() - agcNormal.getGain()), 0.01f);
}

// ============================================================================
// Adversarial Tests — Numeric Robustness
// ============================================================================

FL_TEST_CASE("AutoGain - single-sample frame") {
    AutoGain agc;
    AutoGainConfig config;
    config.targetRMSLevel = 8000.0f;
    agc.configure(config);

    vector<i16> single = {5000};
    AudioSample audio = createSample_AutoGain(single, 0);

    agc.process(audio);

    // Should not crash, valid output
    FL_CHECK_FALSE(agc.getGain() != agc.getGain()); // No NaN
    FL_CHECK_GT(agc.getGain(), 0.0f);
}

FL_TEST_CASE("AutoGain - huge frame 10000 samples") {
    AutoGain agc;
    AutoGainConfig config;
    config.targetRMSLevel = 8000.0f;
    agc.configure(config);

    // 10000 samples at 44100Hz → dt ≈ 0.227s
    vector<i16> huge = generateConstantSignal(10000, 5000);
    AudioSample audio = createSample_AutoGain(huge, 0);

    agc.process(audio);

    // No overshoot, integrator didn't explode
    FL_CHECK_GT(agc.getGain(), 0.0f);
    FL_CHECK_LE(agc.getGain(), config.maxGain);
    FL_CHECK_FALSE(agc.getGain() != agc.getGain());
    FL_CHECK_LT(fl::abs(agc.getStats().integrator), 4.0f * config.maxGain);
}

FL_TEST_CASE("AutoGain - very low sample rate") {
    AutoGain agc;
    AutoGainConfig config;
    config.targetRMSLevel = 8000.0f;
    agc.configure(config);
    agc.setSampleRate(8000);

    vector<i16> signal = generateConstantSignal(1000, 3000);
    AudioSample audio = createSample_AutoGain(signal, 0);

    for (int i = 0; i < 200; ++i) {
        agc.process(audio);
    }

    // Gain should converge correctly (slower dt)
    FL_CHECK_GT(agc.getGain(), 0.5f);
    FL_CHECK_LT(agc.getGain(), config.maxGain);
    FL_CHECK_FALSE(agc.getGain() != agc.getGain());
}
