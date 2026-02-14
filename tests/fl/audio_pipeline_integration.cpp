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

FL_TEST_CASE("AudioReactive - FrequencyBinMapper is always active") {
    AudioReactive audio;
    AudioReactiveConfig config;

    // Verify log bin spacing is enabled by default
    FL_CHECK(config.enableLogBinSpacing == true);

    // Begin with default config
    audio.begin(config);

    // Process a sample to verify mapper works
    vector<i16> samples = generateSineWave(512, 1000.0f, 22050.0f, 8000);
    AudioSample audioSample = createSample(samples, 1000);
    audio.processSample(audioSample);

    // Verify frequency bins are populated
    const auto& data = audio.getData();
    FL_CHECK(data.volume > 0.0f);

    // Verify frequency bins contain energy from the 1kHz sine
    bool hasBinData = false;
    for (int i = 0; i < 16; ++i) {
        if (data.frequencyBins[i] > 0.0f) {
            hasBinData = true;
            break;
        }
    }
    FL_CHECK(hasBinData);
}

FL_TEST_CASE("AudioReactive - SpectralEqualizer disabled by default") {
    AudioReactive audio;
    AudioReactiveConfig config;

    // Verify spectral equalizer is disabled by default
    FL_CHECK_FALSE(config.enableSpectralEqualizer);

    audio.begin(config);

    // Process a sample - should work without EQ
    vector<i16> samples = generateSineWave(512, 1000.0f, 22050.0f, 8000);
    AudioSample audioSample = createSample(samples, 1000);
    audio.processSample(audioSample);

    const auto& data = audio.getData();
    FL_CHECK(data.volume > 0.0f);

    bool hasBinData = false;
    for (int i = 0; i < 16; ++i) {
        if (data.frequencyBins[i] > 0.0f) {
            hasBinData = true;
            break;
        }
    }
    FL_CHECK(hasBinData);
}

FL_TEST_CASE("AudioReactive - Log bin spacing uses sample rate") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.sampleRate = 16000;
    config.enableLogBinSpacing = true;

    audio.begin(config);

    // Generate a sine wave in the mid-frequency range (500 Hz)
    vector<i16> samples = generateSineWave(512, 500.0f, 16000.0f, 10000);
    AudioSample audioSample = createSample(samples, 2000);
    audio.processSample(audioSample);

    // Verify frequency bins are populated
    const auto& data = audio.getData();
    FL_CHECK(data.volume > 0.0f);

    // Check that at least some bins are non-zero
    bool hasBinData = false;
    for (int i = 0; i < 16; ++i) {
        if (data.frequencyBins[i] > 0.0f) {
            hasBinData = true;
            break;
        }
    }
    FL_CHECK(hasBinData);
}

FL_TEST_CASE("AudioReactive - Linear bin spacing fallback") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.sampleRate = 22050;
    config.enableLogBinSpacing = false;  // Use linear spacing

    audio.begin(config);

    // Generate a sine wave
    vector<i16> samples = generateSineWave(512, 1000.0f, 22050.0f, 8000);
    AudioSample audioSample = createSample(samples, 3000);
    audio.processSample(audioSample);

    // Verify frequency bins are populated
    const auto& data = audio.getData();
    FL_CHECK(data.volume > 0.0f);

    // Check that bins contain data
    bool hasBinData = false;
    for (int i = 0; i < 16; ++i) {
        if (data.frequencyBins[i] > 0.0f) {
            hasBinData = true;
            break;
        }
    }
    FL_CHECK(hasBinData);
}

FL_TEST_CASE("AudioReactive - SpectralEqualizer integration") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.sampleRate = 22050;
    config.enableSpectralEqualizer = true;  // Enable EQ

    audio.begin(config);

    // Generate a sine wave
    vector<i16> samples = generateSineWave(512, 1000.0f, 22050.0f, 8000);
    AudioSample audioSample = createSample(samples, 4000);
    audio.processSample(audioSample);

    // Verify frequency bins are populated (EQ modifies values but doesn't zero them out)
    const auto& data = audio.getData();
    FL_CHECK(data.volume > 0.0f);

    bool hasBinData = false;
    for (int i = 0; i < 16; ++i) {
        if (data.frequencyBins[i] > 0.0f) {
            hasBinData = true;
            break;
        }
    }
    FL_CHECK(hasBinData);
}

FL_TEST_CASE("AudioReactive - SpectralEqualizer lazy creation") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.sampleRate = 22050;
    config.enableSpectralEqualizer = false;  // Start with EQ disabled

    audio.begin(config);

    // Process a sample without EQ
    vector<i16> samples1 = generateSineWave(512, 1000.0f, 22050.0f, 8000);
    AudioSample audioSample1 = createSample(samples1, 5000);
    audio.processSample(audioSample1);

    const auto& data1 = audio.getData();
    FL_CHECK(data1.volume > 0.0f);

    // Now reconfigure with EQ enabled
    config.enableSpectralEqualizer = true;
    audio.begin(config);

    // Process another sample with EQ
    vector<i16> samples2 = generateSineWave(512, 1000.0f, 22050.0f, 8000);
    AudioSample audioSample2 = createSample(samples2, 6000);
    audio.processSample(audioSample2);

    const auto& data2 = audio.getData();
    FL_CHECK(data2.volume > 0.0f);

    bool hasBinData = false;
    for (int i = 0; i < 16; ++i) {
        if (data2.frequencyBins[i] > 0.0f) {
            hasBinData = true;
            break;
        }
    }
    FL_CHECK(hasBinData);
}

FL_TEST_CASE("AudioReactive - Band energies use mapper") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.sampleRate = 22050;
    config.enableLogBinSpacing = true;

    audio.begin(config);

    // Generate a low-frequency sine wave (100 Hz) with high amplitude
    // This should produce energy in the bass range
    vector<i16> samples = generateSineWave(512, 100.0f, 22050.0f, 15000);
    AudioSample audioSample = createSample(samples, 7000);
    audio.processSample(audioSample);

    // Check that bassEnergy > 0
    const auto& data = audio.getData();
    FL_CHECK(data.bassEnergy > 0.0f);

    // Check that getData() contains valid band energies
    // Bass bins (0-1) should have energy
    bool hasBassData = false;
    for (int i = 0; i < 2; ++i) {
        if (data.frequencyBins[i] > 0.0f) {
            hasBassData = true;
            break;
        }
    }
    FL_CHECK(hasBassData);
}

FL_TEST_CASE("AudioReactive - Multiple frequency ranges") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.sampleRate = 22050;
    config.enableLogBinSpacing = true;

    audio.begin(config);

    // Process bass frequency (100 Hz)
    vector<i16> bassSamples = generateSineWave(512, 100.0f, 22050.0f, 10000);
    AudioSample bassAudio = createSample(bassSamples, 8000);
    audio.processSample(bassAudio);

    const auto& bassData = audio.getData();
    float bassEnergy1 = bassData.bassEnergy;
    FL_CHECK(bassEnergy1 > 0.0f);

    // Process mid frequency (1000 Hz)
    vector<i16> midSamples = generateSineWave(512, 1000.0f, 22050.0f, 10000);
    AudioSample midAudio = createSample(midSamples, 9000);
    audio.processSample(midAudio);

    const auto& midData = audio.getData();
    float midEnergy = midData.midEnergy;
    FL_CHECK(midEnergy > 0.0f);

    // Process treble frequency (8000 Hz)
    vector<i16> trebleSamples = generateSineWave(512, 8000.0f, 22050.0f, 10000);
    AudioSample trebleAudio = createSample(trebleSamples, 10000);
    audio.processSample(trebleAudio);

    const auto& trebleData = audio.getData();
    float trebleEnergy = trebleData.trebleEnergy;
    FL_CHECK(trebleEnergy > 0.0f);
}

FL_TEST_CASE("AudioReactive - Frequency bin consistency with mapper") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.sampleRate = 22050;
    config.enableLogBinSpacing = true;

    audio.begin(config);

    // Process a full-spectrum signal (mix of frequencies)
    vector<i16> complexSamples = generateSineWave(512, 100.0f, 22050.0f, 3000);
    vector<i16> mid = generateSineWave(512, 1000.0f, 22050.0f, 3000);
    vector<i16> treble = generateSineWave(512, 5000.0f, 22050.0f, 3000);

    // Mix signals
    for (size i = 0; i < 512; ++i) {
        i32 mixed = static_cast<i32>(complexSamples[i]) +
                    static_cast<i32>(mid[i]) +
                    static_cast<i32>(treble[i]);
        // Clamp to i16 range
        if (mixed > 32767) mixed = 32767;
        if (mixed < -32768) mixed = -32768;
        complexSamples[i] = static_cast<i16>(mixed);
    }

    AudioSample complexAudio = createSample(complexSamples, 11000);
    audio.processSample(complexAudio);

    // Verify all frequency bands have energy
    const auto& data = audio.getData();
    FL_CHECK(data.bassEnergy > 0.0f);
    FL_CHECK(data.midEnergy > 0.0f);
    FL_CHECK(data.trebleEnergy > 0.0f);

    // Verify frequency bins are populated across the spectrum
    int nonZeroBins = 0;
    for (int i = 0; i < 16; ++i) {
        if (data.frequencyBins[i] > 0.0f) {
            nonZeroBins++;
        }
    }
    FL_CHECK(nonZeroBins > 0);
}

FL_TEST_CASE("AudioReactive - Pipeline with all middleware enabled") {
    AudioReactive audio;
    AudioReactiveConfig config;
    config.sampleRate = 22050;
    config.enableLogBinSpacing = true;
    config.enableSpectralEqualizer = true;
    config.enableSignalConditioning = true;
    config.enableAutoGain = true;
    config.enableNoiseFloorTracking = true;

    audio.begin(config);

    // Process multiple samples to let middleware converge
    for (int iter = 0; iter < 10; ++iter) {
        vector<i16> samples = generateSineWave(512, 1000.0f, 22050.0f, 5000);
        AudioSample audioSample = createSample(samples, iter * 100);
        audio.processSample(audioSample);
    }

    // Verify all components are active
    const auto& data = audio.getData();
    FL_CHECK(data.volume > 0.0f);

    // With a 1kHz sine, mid energy should be dominant
    FL_CHECK(data.midEnergy > 0.0f);

    // Check signal conditioning stats
    const auto& scStats = audio.getSignalConditionerStats();
    FL_CHECK(scStats.samplesProcessed > 0);

    const auto& agStats = audio.getAutoGainStats();
    FL_CHECK(agStats.samplesProcessed > 0);

    const auto& nfStats = audio.getNoiseFloorStats();
    FL_CHECK(nfStats.samplesProcessed > 0);

    // Check frequency bins
    bool hasBinData = false;
    for (int i = 0; i < 16; ++i) {
        if (data.frequencyBins[i] > 0.0f) {
            hasBinData = true;
            break;
        }
    }
    FL_CHECK(hasBinData);
}
