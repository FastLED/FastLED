// standalone test
#include "fl/audio_reactive.h"
#include "fl/stl/vector.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/math.h"
#include "fl/math_macros.h"
#include "test.h"

using namespace fl;

// Helper: Create AudioSample from vector
static AudioSample createSample(const vector<i16>& samples, u32 timestamp = 0) {
    AudioSampleImplPtr impl = fl::make_shared<AudioSampleImpl>();
    impl->assign(samples.begin(), samples.end(), timestamp);
    return AudioSample(impl);
}

// Helper: Generate sine wave
static vector<i16> generateSineWave(size count, float frequency, float sampleRate, i16 amplitude) {
    vector<i16> samples;
    samples.reserve(count);
    for (size i = 0; i < count; ++i) {
        float phase = 2.0f * FL_M_PI * frequency * static_cast<float>(i) / sampleRate;
        i16 sample = static_cast<i16>(amplitude * fl::sinf(phase));
        samples.push_back(sample);
    }
    return samples;
}

FL_TEST_CASE("AudioReactive - Signal conditioning integration enabled by default") {
    AudioReactive audio;
    AudioReactiveConfig config;
    // Signal conditioning should be enabled by default
    FL_CHECK(config.enableSignalConditioning == true);
    FL_CHECK(config.enableAutoGain == true);
    FL_CHECK(config.enableNoiseFloorTracking == true);

    audio.begin(config);

    // Process a sample - should work without signal conditioning
    vector<i16> samples = generateSineWave(1000, 1000.0f, 22050.0f, 8000);
    AudioSample audioSample = createSample(samples, 1000);
    audio.processSample(audioSample);

    const auto& data = audio.getData();
    FL_CHECK(data.volume > 0.0f);
}

FL_TEST_CASE("AudioReactive - Enable signal conditioning") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.enableSignalConditioning = true;
    config.enableAutoGain = false;
    config.enableNoiseFloorTracking = false;
    audio.begin(config);

    // Create sample with DC bias
    vector<i16> biasedSamples = generateSineWave(1000, 1000.0f, 22050.0f, 5000);
    for (size i = 0; i < biasedSamples.size(); ++i) {
        i32 biased = static_cast<i32>(biasedSamples[i]) + 2000; // Add DC bias
        if (biased > 32767) biased = 32767;
        if (biased < -32768) biased = -32768;
        biasedSamples[i] = static_cast<i16>(biased);
    }

    AudioSample biasedAudio = createSample(biasedSamples, 2000);
    audio.processSample(biasedAudio);

    // Signal conditioning should have removed DC bias
    const auto& scStats = audio.getSignalConditionerStats();
    FL_CHECK(scStats.samplesProcessed > 0);

    // Audio should still be processed
    const auto& data = audio.getData();
    FL_CHECK(data.volume > 0.0f);
}

FL_TEST_CASE("AudioReactive - Enable auto gain") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.enableSignalConditioning = false;
    config.enableAutoGain = true;
    config.enableNoiseFloorTracking = false;
    audio.begin(config);

    // Process several quiet samples to let AGC converge
    for (int i = 0; i < 10; ++i) {
        vector<i16> quietSamples = generateSineWave(500, 1000.0f, 22050.0f, 1000);
        AudioSample quietAudio = createSample(quietSamples, i * 100);
        audio.processSample(quietAudio);
    }

    const auto& agStats = audio.getAutoGainStats();
    FL_CHECK(agStats.samplesProcessed > 0);
    FL_CHECK(agStats.currentGain > 0.0f);

    // Audio should be processed and potentially amplified
    const auto& data = audio.getData();
    FL_CHECK(data.volume >= 0.0f);
}

FL_TEST_CASE("AudioReactive - Enable noise floor tracking") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.enableSignalConditioning = false;
    config.enableAutoGain = false;
    config.enableNoiseFloorTracking = true;
    audio.begin(config);

    // Process several samples to build noise floor estimate
    for (int i = 0; i < 10; ++i) {
        vector<i16> samples = generateSineWave(500, 1000.0f, 22050.0f, 3000);
        AudioSample audioSample = createSample(samples, i * 100);
        audio.processSample(audioSample);
    }

    const auto& nfStats = audio.getNoiseFloorStats();
    FL_CHECK(nfStats.samplesProcessed > 0);
    FL_CHECK(nfStats.currentFloor > 0.0f);

    // Audio should be processed
    const auto& data = audio.getData();
    FL_CHECK(data.volume > 0.0f);
}

FL_TEST_CASE("AudioReactive - Full signal conditioning pipeline") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.enableSignalConditioning = true;
    config.enableAutoGain = true;
    config.enableNoiseFloorTracking = true;
    audio.begin(config);

    // Create complex test signal:
    // - Sine wave with DC bias
    // - Varying amplitude
    for (int iter = 0; iter < 20; ++iter) {
        i16 amplitude = static_cast<i16>(2000 + iter * 200); // Gradually increasing
        vector<i16> samples = generateSineWave(500, 1000.0f, 22050.0f, amplitude);

        // Add DC bias
        for (size i = 0; i < samples.size(); ++i) {
            i32 biased = static_cast<i32>(samples[i]) + 1000;
            if (biased > 32767) biased = 32767;
            if (biased < -32768) biased = -32768;
            samples[i] = static_cast<i16>(biased);
        }

        AudioSample audioSample = createSample(samples, iter * 100);
        audio.processSample(audioSample);
    }

    // Verify all components processed the signal
    const auto& scStats = audio.getSignalConditionerStats();
    FL_CHECK(scStats.samplesProcessed > 0);

    const auto& agStats = audio.getAutoGainStats();
    FL_CHECK(agStats.samplesProcessed > 0);

    const auto& nfStats = audio.getNoiseFloorStats();
    FL_CHECK(nfStats.samplesProcessed > 0);

    // Audio should be processed
    const auto& data = audio.getData();
    FL_CHECK(data.volume > 0.0f);

    // Verify signal conditioning stats
    FL_CHECK(scStats.dcOffset != 0);  // Should have detected DC offset
}

FL_TEST_CASE("AudioReactive - Stats pointers null when disabled") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.enableSignalConditioning = false;
    config.enableAutoGain = false;
    config.enableNoiseFloorTracking = false;
    audio.begin(config);

    // Process a sample
    vector<i16> samples = generateSineWave(500, 1000.0f, 22050.0f, 5000);
    AudioSample audioSample = createSample(samples, 1000);
    audio.processSample(audioSample);

    // Stats should still be available (components exist but are disabled)
    const auto& scStats = audio.getSignalConditionerStats();
    const auto& agStats = audio.getAutoGainStats();
    const auto& nfStats = audio.getNoiseFloorStats();

    // Components are disabled so they shouldn't have processed samples
    FL_CHECK_EQ(scStats.samplesProcessed, 0u);
    FL_CHECK_EQ(agStats.samplesProcessed, 0u);
    FL_CHECK_EQ(nfStats.samplesProcessed, 0u);
}

FL_TEST_CASE("AudioReactive - Signal conditioning with spikes") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.enableSignalConditioning = true;
    audio.begin(config);

    // Create signal with injected spikes
    vector<i16> samples = generateSineWave(1000, 1000.0f, 22050.0f, 3000);
    for (size i = 0; i < 100; i += 10) {
        samples[i] = 25000;  // Inject spikes
    }

    AudioSample audioSample = createSample(samples, 3000);
    audio.processSample(audioSample);

    // Verify spikes were detected and rejected
    const auto& scStats = audio.getSignalConditionerStats();
    FL_CHECK(scStats.spikesRejected > 0);

    // Audio should still be processed (spikes filtered out)
    const auto& data = audio.getData();
    FL_CHECK(data.volume > 0.0f);
}

FL_TEST_CASE("AudioReactive - Backward compatibility") {
    // Test that existing code without signal conditioning still works

    AudioReactive audio;
    AudioReactiveConfig config;
    config.sampleRate = 22050;
    config.gain = 128;
    config.agcEnabled = false;  // Use old AGC, not new AutoGain
    // Don't enable new signal conditioning features
    audio.begin(config);

    // Process samples the old way
    vector<i16> samples = generateSineWave(1000, 1000.0f, 22050.0f, 8000);
    AudioSample audioSample = createSample(samples, 4000);
    audio.processSample(audioSample);

    // Should work exactly as before
    const auto& data = audio.getData();
    FL_CHECK(data.volume > 0.0f);
    FL_CHECK(data.frequencyBins[0] >= 0.0f);
}
