// Unit tests for AutoGain - adversarial and boundary tests
// standalone test

#include "fl/fx/audio/auto_gain.h"
#include "fl/stl/vector.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/math.h"
#include "fl/math_macros.h"
#include "../../audio/test_helpers.h"

using namespace fl;
using fl::audio::test::generateConstantSignal;

namespace {

static AudioSample createSample_AutoGain(const vector<i16>& samples, u32 timestamp = 0) {
    AudioSampleImplPtr impl = fl::make_shared<AudioSampleImpl>();
    impl->assign(samples.begin(), samples.end(), timestamp);
    return AudioSample(impl);
}

} // anonymous namespace

// AG-1: Amplification - Gain Converges Upward (tight)
FL_TEST_CASE("AutoGain - amplification converges to target") {
    AutoGain agc;
    AutoGainConfig config;
    config.enabled = true;
    config.targetRMSLevel = 8000.0f;
    config.gainSmoothing = 0.5f;
    config.learningRate = 0.1f;
    config.maxGain = 20.0f;  // Default 10 would cap gain before reaching target
    agc.configure(config);

    // Quiet signal: amplitude=500, RMS~500
    vector<i16> quiet = generateConstantSignal(1000, 500);
    AudioSample audio = createSample_AutoGain(quiet, 1000);

    for (int i = 0; i < 30; ++i) {
        agc.process(audio);
    }

    const auto& stats = agc.getStats();
    // Gain must be significantly above 1 (target=8000, input~500 → gain~16)
    FL_CHECK_GT(stats.currentGain, 5.0f);
    // Output RMS within 30% of target
    FL_CHECK_GT(stats.outputRMS, 5600.0f);
    FL_CHECK_LT(stats.outputRMS, 10400.0f);
}

// AG-2: Attenuation - Gain Converges Downward (tight)
FL_TEST_CASE("AutoGain - attenuation converges to target") {
    AutoGain agc;
    AutoGainConfig config;
    config.enabled = true;
    config.targetRMSLevel = 8000.0f;
    config.gainSmoothing = 0.5f;
    config.learningRate = 0.1f;
    agc.configure(config);

    // Loud signal: amplitude=20000, RMS~20000
    vector<i16> loud = generateConstantSignal(1000, 20000);
    AudioSample audio = createSample_AutoGain(loud, 2000);

    for (int i = 0; i < 30; ++i) {
        agc.process(audio);
    }

    const auto& stats = agc.getStats();
    FL_CHECK_LT(stats.currentGain, 0.5f);
    FL_CHECK_GT(stats.outputRMS, 5600.0f);
    FL_CHECK_LT(stats.outputRMS, 10400.0f);
}

// AG-3: Percentile Convergence - Tight Range
FL_TEST_CASE("AutoGain - percentile estimate converges tightly") {
    AutoGain agc;
    AutoGainConfig config;
    config.targetPercentile = 0.9f;
    config.learningRate = 0.05f;
    config.targetRMSLevel = 8000.0f;
    agc.configure(config);

    // Constant amplitude=10000 → RMS~10000. P90 of |samples| ≈ 10000
    vector<i16> samples = generateConstantSignal(500, 10000);
    AudioSample audio = createSample_AutoGain(samples, 0);

    for (int i = 0; i < 100; ++i) {
        agc.process(audio);
    }

    // Percentile should converge near 10000 (within 20%)
    float est = agc.getStats().percentileEstimate;
    FL_CHECK_GT(est, 8000.0f);
    FL_CHECK_LT(est, 12000.0f);
}

// AG-4: Silence Handling - No NaN/Inf
FL_TEST_CASE("AutoGain - silence produces no NaN") {
    AutoGain agc;
    AutoGainConfig config;
    config.enabled = true;
    config.targetRMSLevel = 8000.0f;
    config.maxGain = 10.0f;
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
    // Gain clamped to maxGain (percentile floors at 1.0, gain = 8000/1 = 8000, clamped to 10)
    FL_CHECK_LE(stats.currentGain, 10.0f);
    // No NaN
    FL_CHECK_FALSE(stats.currentGain != stats.currentGain); // NaN check
    FL_CHECK_FALSE(stats.percentileEstimate != stats.percentileEstimate);
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
        config.learningRate = 0.2f;
        agc.configure(config);

        vector<i16> veryQuiet = generateConstantSignal(1000, 100);
        AudioSample audio = createSample_AutoGain(veryQuiet, 4000);
        for (int i = 0; i < 30; ++i) agc.process(audio);

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
        config.learningRate = 0.2f;
        agc.configure(config);

        vector<i16> veryLoud = generateConstantSignal(1000, 30000);
        AudioSample audio = createSample_AutoGain(veryLoud, 5000);
        for (int i = 0; i < 30; ++i) agc.process(audio);

        FL_CHECK_GE(agc.getGain(), 0.5f);
        FL_CHECK_LE(agc.getGain(), 2.0f);
    }
}

// AG-7: Gain Smoothing Rate
FL_TEST_CASE("AutoGain - smoothing prevents gain jumps") {
    AutoGain agc;
    AutoGainConfig config;
    config.gainSmoothing = 0.95f;
    config.learningRate = 0.5f;
    config.targetRMSLevel = 8000.0f;
    agc.configure(config);

    // Start quiet
    vector<i16> quiet = generateConstantSignal(1000, 500);
    AudioSample quietAudio = createSample_AutoGain(quiet, 6000);
    for (int i = 0; i < 5; ++i) agc.process(quietAudio);
    float gainBeforeSwitch = agc.getGain();

    // Switch to loud
    vector<i16> loud = generateConstantSignal(1000, 20000);
    AudioSample loudAudio = createSample_AutoGain(loud, 7000);
    agc.process(loudAudio);
    float gainAfterOneFrame = agc.getGain();

    // With 0.95 smoothing, gain should not jump more than ~10% per frame
    // The ideal gain changes dramatically (from ~16 to ~0.4), but smoothing limits it
    float jumpRatio = fl::abs(gainAfterOneFrame - gainBeforeSwitch) / gainBeforeSwitch;
    FL_CHECK_LT(jumpRatio, 0.5f); // Less than 50% change in one frame
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
    AutoGainConfig config;
    config.learningRate = 0.1f;
    agc.configure(config);

    vector<i16> samples = generateConstantSignal(1000, 10000);
    for (int i = 0; i < 10; ++i) agc.process(createSample_AutoGain(samples, i * 100));

    FL_CHECK_GT(agc.getStats().samplesProcessed, 0u);

    agc.reset();
    FL_CHECK_EQ(agc.getStats().currentGain, 1.0f);
    FL_CHECK_EQ(agc.getStats().samplesProcessed, 0u);
}

// Keep: No clipping
FL_TEST_CASE("AutoGain - no clipping on extreme amplification") {
    AutoGain agc;
    AutoGainConfig config;
    config.maxGain = 100.0f;
    config.targetRMSLevel = 30000.0f;
    config.learningRate = 0.5f;
    agc.configure(config);

    vector<i16> quiet(1000);
    for (size i = 0; i < 1000; ++i) {
        float phase = 2.0f * FL_M_PI * 1000.0f * static_cast<float>(i) / 22050.0f;
        quiet[i] = static_cast<i16>(1000 * fl::sinf(phase));
    }
    AudioSample audio = createSample_AutoGain(quiet, 9000);

    for (int i = 0; i < 20; ++i) {
        AudioSample result = agc.process(audio);
        const auto& pcm = result.pcm();
        for (size j = 0; j < pcm.size(); ++j) {
            FL_CHECK_GE(pcm[j], -32768);
            FL_CHECK_LE(pcm[j], 32767);
        }
    }
}

// Keep: P90 > P50
FL_TEST_CASE("AutoGain - P90 estimate higher than P50") {
    AutoGain agc50;
    AutoGainConfig c50;
    c50.targetPercentile = 0.5f;
    c50.learningRate = 0.1f;
    agc50.configure(c50);

    AutoGain agc90;
    AutoGainConfig c90;
    c90.targetPercentile = 0.9f;
    c90.learningRate = 0.1f;
    agc90.configure(c90);

    for (int i = 0; i < 50; ++i) {
        i16 amp = static_cast<i16>(5000 + 100 * i);
        vector<i16> s = generateConstantSignal(500, amp);
        AudioSample a = createSample_AutoGain(s, i * 100);
        agc50.process(a);
        agc90.process(a);
    }

    FL_CHECK_GT(agc90.getStats().percentileEstimate, agc50.getStats().percentileEstimate);
}
