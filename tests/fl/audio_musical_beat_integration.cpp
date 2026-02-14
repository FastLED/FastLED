// Integration tests for musical beat detection through AudioReactive
// standalone test
#include "fl/audio_reactive.h"
#include "fl/stl/vector.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/math.h"
#include "fl/math_macros.h"
#include "test.h"

using namespace fl;

namespace { // musical_beat_integration

AudioSample createSample(const vector<i16>& samples, u32 timestamp = 0) {
    AudioSampleImplPtr impl = fl::make_shared<AudioSampleImpl>();
    impl->assign(samples.begin(), samples.end(), timestamp);
    return AudioSample(impl);
}

vector<i16> generateSineWave(size count, float frequency, float sampleRate, i16 amplitude) {
    vector<i16> samples;
    samples.reserve(count);
    for (size i = 0; i < count; ++i) {
        float phase = 2.0f * FL_M_PI * frequency * static_cast<float>(i) / sampleRate;
        i16 sample = static_cast<i16>(amplitude * fl::sinf(phase));
        samples.push_back(sample);
    }
    return samples;
}

} // anonymous namespace

// INT-1: Full pipeline with DC offset + noise gate + auto gain
FL_TEST_CASE("AudioReactive - full pipeline DC removal and gain") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.sampleRate = 22050;
    config.enableSignalConditioning = true;
    config.enableAutoGain = true;
    config.enableNoiseFloorTracking = true;
    config.enableLogBinSpacing = true;

    audio.begin(config);

    // Signal: 500 amplitude sine at 1kHz with 3000 DC offset
    for (int iter = 0; iter < 20; ++iter) {
        vector<i16> samples;
        samples.reserve(512);
        for (size i = 0; i < 512; ++i) {
            float phase = 2.0f * FL_M_PI * 1000.0f * static_cast<float>(i) / 22050.0f;
            i32 val = static_cast<i32>(500.0f * fl::sinf(phase)) + 3000;
            if (val > 32767) val = 32767;
            if (val < -32768) val = -32768;
            samples.push_back(static_cast<i16>(val));
        }
        AudioSample audioSample = createSample(samples, iter * 100);
        audio.processSample(audioSample);
    }

    // Signal conditioning should have removed DC and processed signal
    const auto& scStats = audio.getSignalConditionerStats();
    FL_CHECK_GT(scStats.samplesProcessed, 0u);

    // Auto gain should have processed samples
    const auto& agStats = audio.getAutoGainStats();
    FL_CHECK_GT(agStats.samplesProcessed, 0u);

    // Volume should be measurable
    const auto& data = audio.getData();
    FL_CHECK_GT(data.volume, 0.0f);
}

// INT-2: Pipeline with silence - no NaN, no crash
FL_TEST_CASE("AudioReactive - silence pipeline no NaN") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.sampleRate = 22050;
    config.enableSignalConditioning = true;
    config.enableAutoGain = true;
    config.enableNoiseFloorTracking = true;
    audio.begin(config);

    // Feed 20 frames of silence
    for (int iter = 0; iter < 20; ++iter) {
        vector<i16> silence(512, 0);
        AudioSample audioSample = createSample(silence, iter * 100);
        audio.processSample(audioSample);
    }

    // Should not crash, volume should be zero or near-zero
    const auto& data = audio.getData();
    FL_CHECK_LT(data.volume, 100.0f);
    FL_CHECK_FALSE(data.volume != data.volume); // NaN check
}

// INT-3: Musical beat detection actually processes audio
FL_TEST_CASE("AudioReactive - musical beat detection processes audio") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.sampleRate = 22050;
    config.enableMusicalBeatDetection = true;
    config.enableSpectralFlux = true;
    config.musicalBeatMinBPM = 60.0f;
    config.musicalBeatMaxBPM = 180.0f;
    config.musicalBeatConfidence = 0.3f;
    audio.begin(config);

    // Feed actual audio signal (not just config check!)
    // Process 20 frames of a 440 Hz tone with varying amplitude
    for (int iter = 0; iter < 20; ++iter) {
        float amplitude = (iter % 4 == 0) ? 15000.0f : 1000.0f;
        vector<i16> samples = generateSineWave(512, 440.0f, 22050.0f,
                                               static_cast<i16>(amplitude));
        AudioSample audioSample = createSample(samples, iter * 23);
        audio.processSample(audioSample);
    }

    // Verify actual processing happened (not just config)
    const auto& data = audio.getData();
    FL_CHECK_GT(data.volume, 0.0f);

    // Frequency bins should have energy from the 440 Hz tone
    bool hasBinData = false;
    for (int i = 0; i < 16; ++i) {
        if (data.frequencyBins[i] > 0.0f) {
            hasBinData = true;
            break;
        }
    }
    FL_CHECK(hasBinData);
}

// INT-4: Multi-band beat detection actually processes audio
FL_TEST_CASE("AudioReactive - multi-band beat detection processes audio") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.sampleRate = 22050;
    config.enableMultiBandBeats = true;
    config.enableSpectralFlux = true;
    config.bassThreshold = 0.15f;
    config.midThreshold = 0.12f;
    config.trebleThreshold = 0.08f;
    audio.begin(config);

    // Feed bass-heavy signal alternating with silence (simulating kick drum)
    for (int iter = 0; iter < 20; ++iter) {
        vector<i16> samples;
        if (iter % 5 == 0) {
            // Bass burst
            samples = generateSineWave(512, 100.0f, 22050.0f, 15000);
        } else {
            // Quiet
            samples = generateSineWave(512, 100.0f, 22050.0f, 500);
        }
        AudioSample audioSample = createSample(samples, iter * 23);
        audio.processSample(audioSample);
    }

    // Verify actual processing (not just config check)
    const auto& data = audio.getData();
    FL_CHECK_GT(data.volume, 0.0f);

    // Bass energy should be present
    FL_CHECK_GT(data.bassEnergy, 0.0f);
}

// Keep: Pipeline with all middleware enabled (tightened)
FL_TEST_CASE("AudioReactive - all middleware enabled processes correctly") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.sampleRate = 22050;
    config.enableLogBinSpacing = true;
    config.enableSpectralEqualizer = true;
    config.enableSignalConditioning = true;
    config.enableAutoGain = true;
    config.enableNoiseFloorTracking = true;
    audio.begin(config);

    // Process 10 frames of 1kHz sine
    for (int iter = 0; iter < 10; ++iter) {
        vector<i16> samples = generateSineWave(512, 1000.0f, 22050.0f, 5000);
        AudioSample audioSample = createSample(samples, iter * 100);
        audio.processSample(audioSample);
    }

    const auto& data = audio.getData();
    FL_CHECK_GT(data.volume, 0.0f);
    FL_CHECK_GT(data.midEnergy, 0.0f);

    // All stats should show processing occurred
    const auto& scStats = audio.getSignalConditionerStats();
    FL_CHECK_GT(scStats.samplesProcessed, 0u);

    const auto& agStats = audio.getAutoGainStats();
    FL_CHECK_GT(agStats.samplesProcessed, 0u);

    const auto& nfStats = audio.getNoiseFloorStats();
    FL_CHECK_GT(nfStats.samplesProcessed, 0u);
}
